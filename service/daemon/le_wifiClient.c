// -------------------------------------------------------------------------------------------------
/**
 *  Legato Wifi Client
 *
 *  Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 *
 */
// -------------------------------------------------------------------------------------------------
#include "legato.h"

#include "interfaces.h"

#include "pa_wifi.h"

//--------------------------------------------------------------------------------------------------
/**
 * The initial allocated AP:s at system start.
 * Note that the pool will grow automatically if it is needed.
 */
//-------------------------------------------------------------------------------------------------
#define INIT_NBR_OF_AP 32

//--------------------------------------------------------------------------------------------------
/**
 * Value of signal strength that indicates that no value was found.
 */
//-------------------------------------------------------------------------------------------------
#define SIGNAL_STRENGTH_DEFAULT  ( 0xfff )

//--------------------------------------------------------------------------------------------------
/**
 * Struct to hold the AccessPoint from the Scan's data.
 *
 */
//-------------------------------------------------------------------------------------------------
typedef struct
{
    pa_wifiClient_AccessPoint_t accessPoint;
    bool foundInLatestScan;
}
FoundAccessPoint_t;

//--------------------------------------------------------------------------------------------------
/**
 * Safe Reference Map for Access Points found during scan or le_wifiClient_Create()
 */
//--------------------------------------------------------------------------------------------------
static le_ref_MapRef_t ScanApRefMap;

//--------------------------------------------------------------------------------------------------
/**
 * Pool from which FoundAccessPoint_t objects are allocated.
 * After each Scan pool is first emptied then populated with the result.
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t AccessPointPool;

//--------------------------------------------------------------------------------------------------
/**
 * The number of found AP:s from the scan used for informative traces.
 *
 */
//--------------------------------------------------------------------------------------------------
static int16_t FoundWifiApCount = 0;

//--------------------------------------------------------------------------------------------------
/**
 * iteration reference used by GetFirst & GetNext
 * @see variable GetFirstSessionRef
 */
//--------------------------------------------------------------------------------------------------
static le_ref_IterRef_t IterRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Saves reference at call of GetFirst to check that GetNext is being called by same caller.
 * This to protect varible IterRef.
 */
//--------------------------------------------------------------------------------------------------
static le_msg_SessionRef_t GetFirstSessionRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * If the Scan is running in a seperate thread, this variable will have a value != NULL.
 * The thread destructor will clear the
 */
//--------------------------------------------------------------------------------------------------
static le_thread_Ref_t ScanThreadRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Event ID for Wifi Event message notification.
 *
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t NewWifiEventId;

//--------------------------------------------------------------------------------------------------
/**
 * The number of calls to le_wifiClient_Start().
 * This is used to call stop the wifi hardware, when the last client calls stop.
 */
//--------------------------------------------------------------------------------------------------
static int16_t ClientStartCount = 0;

//--------------------------------------------------------------------------------------------------
/**
 * CallBack for PA Events.
 */
//--------------------------------------------------------------------------------------------------

static void PaEventHandler
(
    le_wifiClient_Event_t event,
    void* contextPtr
)
{
    LE_DEBUG( "PaEventHandler event: %d ", event );

    le_event_Report( NewWifiEventId, (void*)&event, sizeof( le_wifiClient_Event_t ) );
}

//--------------------------------------------------------------------------------------------------
/**
 * Local function to find an access point reference based on SSID among the AP:s found in scan.
 * If not found will return NULL.
 */
//--------------------------------------------------------------------------------------------------
static le_wifiClient_AccessPointRef_t FindAccessPointRefFromSsid
(
    const uint8_t* SsidPtr,
        ///< [OUT]
        ///< The SSID returned as a octet array.

    size_t SsidNumElements
        ///< [INOUT]
)
{
    le_wifiClient_AccessPointRef_t accessPointRef = NULL;
    le_ref_IterRef_t iter = le_ref_GetIterator( ScanApRefMap );
    LE_DEBUG( "FindAccessPointRefFromSsid" );

    while ( le_ref_NextNode( iter ) == LE_OK )
    {
        accessPointRef = ( le_wifiClient_AccessPointRef_t ) le_ref_GetSafeRef( iter );
        if ( NULL != accessPointRef )
        {
            FoundAccessPoint_t* accessPointPtr = ( FoundAccessPoint_t* )
            le_ref_Lookup( ScanApRefMap, accessPointRef );
            if ( accessPointPtr != NULL )
            {
                if( ( accessPointPtr->accessPoint.ssidLength == SsidNumElements ) )
                {
                    if ( 0 == memcmp( accessPointPtr->accessPoint.ssidBytes,  SsidPtr,
                        SsidNumElements ) )
                    {
                        LE_DEBUG( "FindAccessPointRefFromSsid found accessPointRef %p", accessPointRef );
                        return accessPointRef;
                    }
                }
            }
        }
        else
        {
            LE_ERROR( "FindAccessPointRefFromSsid ERROR le_ref_GetSafeRef"
                                " returned NULL iter:%p", iter );
        }
    }
    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Local function to add AP:s found during scan to AddRef point interface
 */
//--------------------------------------------------------------------------------------------------

static le_wifiClient_AccessPointRef_t  AddAccessPointToApRefMap
(
    pa_wifiClient_AccessPoint_t * accessPointPtr
)
{
    // first see if it alreay exists in our list of reference.
    le_wifiClient_AccessPointRef_t returRef = FindAccessPointRefFromSsid( accessPointPtr->ssidBytes,
        accessPointPtr->ssidLength );

    if( NULL != returRef )
    {
        FoundAccessPoint_t* oldAccessPointPtr =
            ( FoundAccessPoint_t* ) le_ref_Lookup( ScanApRefMap, returRef );


        if( NULL != oldAccessPointPtr )
        {
            LE_DEBUG( "AddAccessPointToApRefMap Already exists %p. Update  SignalStrength %d",
                    returRef, accessPointPtr->signalStrength);

            oldAccessPointPtr->accessPoint.signalStrength = accessPointPtr->signalStrength;
            oldAccessPointPtr->foundInLatestScan = true;
        }

        return returRef;
    }
    else
    {
        FoundAccessPoint_t* foundAccessPointPtr = le_mem_ForceAlloc( AccessPointPool );

        if ( foundAccessPointPtr )
        {
            FoundWifiApCount++;
            LE_DEBUG( "AddAccessPointToApRefMap New AP[%d] SignalStrength %d"
                                "SSID length %d SSID: \"%.*s\"",
                FoundWifiApCount,
                accessPointPtr->signalStrength,
                accessPointPtr->ssidLength,
                accessPointPtr->ssidLength,
                ( char* ) accessPointPtr->ssidBytes
                );

            // struct member value copy
            foundAccessPointPtr->accessPoint = *accessPointPtr;
            foundAccessPointPtr->foundInLatestScan = true;

            // Create a Safe Reference for this object.
            returRef = le_ref_CreateRef( ScanApRefMap, foundAccessPointPtr );

            LE_DEBUG( "AddAccessPointToApRefMap le_ref_CreateRef foundAccessPointPtr %p; Ref%p ",
                            foundAccessPointPtr, returRef );
        }
        else
        {
            LE_ERROR( "AddAccessPointToApRefMap le_mem_ForceAlloc failed.  Count %d", FoundWifiApCount );
        }
    }

    return returRef;
}

//--------------------------------------------------------------------------------------------------
/**
 *  Frees one members of the access point and the corresponding access points memory
 */
//--------------------------------------------------------------------------------------------------

static void RemoveAccessPoint
(
    le_wifiClient_AccessPointRef_t accessPointRef
)
{
    FoundAccessPoint_t* accessPointPtr =
                        ( FoundAccessPoint_t* ) le_ref_Lookup( ScanApRefMap, accessPointRef );

    if (accessPointPtr == NULL )
    {
        LE_ERROR( "Bad reference" );
        return;
    }

    le_ref_DeleteRef( ScanApRefMap, accessPointRef );
    le_mem_Release( accessPointPtr );
}

//--------------------------------------------------------------------------------------------------
/**
 *  Frees all members of the AddRef and the corresponding AccessPoints added during Scan and
 *  le_wifiClient_Create()
 */
//--------------------------------------------------------------------------------------------------

static void ReleaseAllAccessPoints
(
    void
)
{
    le_wifiClient_AccessPointRef_t accessPointRef= NULL;
    le_ref_IterRef_t iter = le_ref_GetIterator( ScanApRefMap );
    LE_DEBUG( "ReleaseAllAccessPoints" );

    while ( le_ref_NextNode( iter ) == LE_OK )
    {
        accessPointRef = ( le_wifiClient_AccessPointRef_t ) le_ref_GetSafeRef( iter );
        if ( NULL != accessPointRef )
        {
            RemoveAccessPoint( accessPointRef );
        }
        else
        {
            LE_ERROR( "ReleaseAllAccessPoints ERROR le_ref_GetSafeRef"
                                " returned NULL iter:%p", iter );
            return;
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Marks the current AccessPoints as old by changing the values for foundInLatestScan
 * and signalStrength.
 * These values will be updated later, if the same AP is still found.
 * This way the new and old AccessPoints can be separated.
 *
 */
//--------------------------------------------------------------------------------------------------

static void MarkAllAccessPointsOld
(
    void
)
{
    le_wifiClient_AccessPointRef_t accessPointRef= NULL;
    le_ref_IterRef_t iter = le_ref_GetIterator( ScanApRefMap );
    int counter = 0;
    LE_DEBUG( "MarkAllAccessPointsOld" );


    while ( le_ref_NextNode( iter ) == LE_OK )
    {
        accessPointRef = ( le_wifiClient_AccessPointRef_t ) le_ref_GetSafeRef( iter );
        if ( NULL != accessPointRef )
        {
            FoundAccessPoint_t* accessPointPtr =
                ( FoundAccessPoint_t* ) le_ref_Lookup( ScanApRefMap, accessPointRef );

            if (accessPointPtr != NULL )
            {
                accessPointPtr->accessPoint.signalStrength = SIGNAL_STRENGTH_DEFAULT;
                accessPointPtr->foundInLatestScan = false;
                LE_DEBUG( "Marking %p as old", accessPointRef );
                counter++;
            }
            else
            {
                LE_ERROR( "MarkAllAccessPointsOld: Bad reference" );
                return;
            }

        }
        else
        {
            LE_ERROR( "MarkAllAccessPointsOld ERROR le_ref_GetSafeRef"
                                " returned NULL iter:%p", iter );
            break;
        }
    }
    LE_DEBUG( "MarkAllAccessPointsOld marked: %d", counter );
}

//--------------------------------------------------------------------------------------------------
/**
 * Start Scanning for Wifi Access points
 * Will result in event LE_WIFICLIENT_EVENT_SCAN_DONE when the scan results are available.
 *
 * @return LE_FAULT         Function failed.
 * @return LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
static void* ScanThread
(
    void* contextPtr
)
{
    pa_wifiClient_AccessPoint_t accessPoint;
    le_result_t paResult = pa_wifiClient_Scan();
    if ( LE_OK != paResult )
    {
        LE_ERROR( "le_wifiClient_Scan failed (%d)", paResult );
        return NULL;
    }

    FoundWifiApCount = 0;

    MarkAllAccessPointsOld();

    while( LE_OK == ( paResult = pa_wifiClient_GetScanResult( &accessPoint )) )
    {
         AddAccessPointToApRefMap( &accessPoint );
    }

    pa_wifiClient_ScanDone();

    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Thread Destructor for scan
 */
//--------------------------------------------------------------------------------------------------
static void ScanThreadDestructor(void *context)
{
    LE_DEBUG( "ScanThreadDestructor: Scan thread exited.");
    ScanThreadRef = NULL;
    // use the PA callback to generate the event.
    PaEventHandler( LE_WIFICLIENT_EVENT_SCAN_DONE, NULL );
}

//--------------------------------------------------------------------------------------------------
/**
 * Handler function to the close session service to detect if the application crashed.
 *
 */
//--------------------------------------------------------------------------------------------------
static void CloseSessionEventHandler
(
    le_msg_SessionRef_t sessionRef,
    void*               contextPtr
)
{
    LE_DEBUG( "CloseSessionEventHandler sessionRef %p GetFirstSessionRef %p",
        sessionRef,
        GetFirstSessionRef );

    if ( sessionRef == GetFirstSessionRef )
    {
        GetFirstSessionRef = NULL;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Is Scan running. Checks if the ScanThread is still running
 */
//--------------------------------------------------------------------------------------------------
static bool IsScanRunning( void )
{
    LE_DEBUG( "IsScanRunning .%d", ( NULL != ScanThreadRef ));
    return ( NULL != ScanThreadRef );
}
//--------------------------------------------------------------------------------------------------
/**
 * The first-layer Wifi Client Event Handler.
 *
 */
//--------------------------------------------------------------------------------------------------
static void FirstLayerWifiClientEventHandler
(
    void* reportPtr,
    void* secondLayerHandlerFunc
)
{
    le_wifiClient_NewEventHandlerFunc_t clientHandlerFunc = secondLayerHandlerFunc;

    le_wifiClient_Event_t  * wifiEvent = ( le_wifiClient_Event_t* ) reportPtr;


    if ( NULL != wifiEvent )
    {
        LE_DEBUG( "FirstLayerWifiClientEventHandler event: %d", *wifiEvent );
        clientHandlerFunc( *wifiEvent, le_event_GetContextPtr() );
    }
    else
    {
        LE_ERROR( "FirstLayerWifiClientEventHandler event is NULL" );
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * returns the value of the field "foundInLatestScan"
 * This is used to determine if a Ref was found in the last scan or not.
 */
//--------------------------------------------------------------------------------------------------
static bool WasApFoundInLastScan
(
    le_wifiClient_AccessPointRef_t accessPointRef
        ///< [IN]
        ///< Wifi Access Point reference.

)
{
    FoundAccessPoint_t* accessPointPtr =
                            ( FoundAccessPoint_t* ) le_ref_Lookup( ScanApRefMap, accessPointRef );

    if (accessPointPtr == NULL )
    {
        LE_ERROR( "Bad reference" );
        return false;
    }

    return accessPointPtr->foundInLatestScan;

}

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'le_wifiClient_NewEvent'
 *
 * This event provide information on Wfifi Client event changes.
 */
//--------------------------------------------------------------------------------------------------
le_wifiClient_NewEventHandlerRef_t le_wifiClient_AddNewEventHandler
(
    le_wifiClient_NewEventHandlerFunc_t handlerFuncPtr,
        ///< [IN]

    void* contextPtr
        ///< [IN]
)
{
    // Note: THIS ONE REGISTERS THE CB function..
    LE_DEBUG( "le_wifiClient_AddNewEventHandler" );
    le_event_HandlerRef_t handlerRef;

    if (handlerFuncPtr == NULL)
    {
        LE_KILL_CLIENT( "handlerFuncPtr is NULL !" );
        return NULL;
    }

    handlerRef = le_event_AddLayeredHandler( "NewWiFiClientMsgHandler",
                    NewWifiEventId,
                    FirstLayerWifiClientEventHandler,
                    (le_event_HandlerFunc_t)handlerFuncPtr);

    le_event_SetContextPtr( handlerRef, contextPtr );

    return ( le_wifiClient_NewEventHandlerRef_t )( handlerRef );

}

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'le_wifiClient_NewEvent'
 */
//--------------------------------------------------------------------------------------------------
void le_wifiClient_RemoveNewEventHandler
(
    le_wifiClient_NewEventHandlerRef_t addHandlerRef
        ///< [IN]
)
{
    LE_DEBUG( "le_wifiClient_RemoveNewEventHandler" );
    le_event_RemoveHandler( (le_event_HandlerRef_t) addHandlerRef );
}

//--------------------------------------------------------------------------------------------------
/**
 * This function starts the WIFI device.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_BUSY          If the WIFI device is already started.
 * @return LE_OK            The function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_Start
(
    void
)
{
    le_result_t pa_result = LE_OK;
    LE_DEBUG( "le_wifiClient_Start" );

    // Count the number of Clients calling start.
    ClientStartCount++;

    // first client starts the hardware
    if( 1 == ClientStartCount )
    {
        pa_result = pa_wifiClient_Start();
    }

    if ( LE_OK != pa_result )
    {
        LE_ERROR( "COMPONENT_INIT (wifi) ERROR: pa_wifiClient_Init returns %d",
                            pa_result );
    }

    return pa_result;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function stops the WIFI device.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_DUPLICATE     If the WIFI device is already stopped.
 * @return LE_OK            The function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_Stop
(
    void
)
{
    ClientStartCount--;
    LE_DEBUG( "le_wifiClient_Stop: ClientStartCount %d", ClientStartCount );

    // last client closes the hardware
    if ( 0 == ClientStartCount )
    {
        pa_wifiClient_Stop();
        ReleaseAllAccessPoints();
        LE_DEBUG( "le_wifiClient_Stop: Last client pa_wifiClient_Release." );
    }

    return LE_OK;
}



//--------------------------------------------------------------------------------------------------
/**
 * Start Scanning for Wifi Access points
 * Will result in event LE_WIFICLIENT_EVENT_SCAN_DONE when the scan results are available.
 *
 * @return LE_FAULT         Function failed.
 * @return LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_Scan
(
    void
)
{
    if ( !IsScanRunning() )
    {
        LE_DEBUG( "le_wifiClient_Scan started" );

        // Start the thread
        ScanThreadRef = le_thread_Create( "Wifi Client Scan Thread", ScanThread, NULL );
        le_thread_AddChildDestructor( ScanThreadRef, ScanThreadDestructor, NULL );

        le_thread_Start( ScanThreadRef );
        return LE_OK;
    }
    else
    {
        LE_DEBUG( "le_wifiClient_Scan ERROR: Scan already running" );
        return LE_BUSY;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the first Wifi Access Point found.
 * Will return the Access Points in the order of found.
 *
 * @return Wifi Access Point reference if ok.
 * @return NULL if no Access Point reference available.
 */
//--------------------------------------------------------------------------------------------------
le_wifiClient_AccessPointRef_t le_wifiClient_GetFirstAccessPoint
(
    void
)
{
    le_wifiClient_AccessPointRef_t accessPointRef = NULL;
    bool found = false;

    if ( IsScanRunning() )
    {
        LE_ERROR( "le_wifiClient_GetFirstAccessPoint: ERROR: Scan is running." );
        return NULL;
    }
    IterRef = le_ref_GetIterator( ScanApRefMap );
    GetFirstSessionRef = le_wifiClient_GetClientSessionRef();

    LE_DEBUG( "le_wifiClient_GetFirstAccessPoint" );

    while (le_ref_NextNode( IterRef ) == LE_OK )
    {
        accessPointRef = ( le_wifiClient_AccessPointRef_t ) le_ref_GetSafeRef( IterRef );
        if( WasApFoundInLastScan( accessPointRef ) )
        {
            found = true;
            break;
        }
    }

    if( found )
    {
        LE_DEBUG( "le_wifiClient_GetFirstAccessPoint %p", accessPointRef );
        return accessPointRef;
    }
    else
    {
        LE_DEBUG( "le_wifiClient_GetFirstAccessPoint NOT FOUND" );
        return NULL;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the next Wifi Access Point.
 * Will return the Access Points in the order of found.
 * This function must be called in the same context as the GetFirstAccessPoint
 *
 * @return Wifi Access Point reference if ok.
 * @return NULL if no Access Point reference available.
*/
//--------------------------------------------------------------------------------------------------
le_wifiClient_AccessPointRef_t le_wifiClient_GetNextAccessPoint
(
    void
)
{
    le_wifiClient_AccessPointRef_t accessPointRef = NULL;
    bool found = false;

    LE_DEBUG( "le_wifiClient_GetNextAccessPoint" );
    if ( IsScanRunning() )
    {
        LE_ERROR( "le_wifiClient_GetFirstAccessPoint: ERROR: Scan is running." );
        return NULL;
    }

    /* This check to protect the variable IterRef that shouldn't be called from different contexts*/
    if ( le_wifiClient_GetClientSessionRef() != GetFirstSessionRef )
    {
        LE_ERROR( "le_wifiClient_GetNextAccessPoint ERROR: Called from"
                            "different context than GetFirstAccessPoint");
        return NULL;
    }

    while ( le_ref_NextNode( IterRef ) == LE_OK )
    {
        accessPointRef = ( le_wifiClient_AccessPointRef_t ) le_ref_GetSafeRef( IterRef );
        LE_DEBUG( "le_wifiClient_GetNextAccessPoint %p", accessPointRef );
        if( WasApFoundInLastScan( accessPointRef ) )
        {
            found = true;
            break;
        }
    }


    if ( found )
    {
        LE_DEBUG( "le_wifiClient_GetNextAccessPoint %p", accessPointRef );
        return accessPointRef;
    }
    else
    {
        LE_DEBUG( "le_wifiClient_GetNextAccessPoint NOT FOUND" );
        GetFirstSessionRef = NULL;
        return NULL;
    }
}



//--------------------------------------------------------------------------------------------------
/**
 * Get the signal strength of the AccessPoint
 *
 * @return
 *  - signal strength in dBm. Example -30 = -30dBm
 *  - if no signal available it will return OxFFFF
 */
//--------------------------------------------------------------------------------------------------
int16_t le_wifiClient_GetSignalStrength
(
    le_wifiClient_AccessPointRef_t accessPointRef
        ///< [IN]
        ///< Wifi Access Point reference.
)
{
    FoundAccessPoint_t * accessPoint = le_ref_Lookup( ScanApRefMap, accessPointRef );
    LE_DEBUG( "le_wifiClient_GetSignalStrength" );
    if ( NULL == accessPoint )
    {
        LE_ERROR( "le_wifiClient_GetSignalStrength: invalid accessPointRef" );
        return SIGNAL_STRENGTH_DEFAULT;
    }
    else
    {
        return accessPoint->accessPoint.signalStrength;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the Service set identification (SSID) of the AccessPoint
 * @note that the SSID does not have to be human readable ASCII values, but often has.
 *
 * @return LE_FAULT         Function failed.
 * @return LE_BAD_PARAMETER Some parameter is invalid.
 * @return LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_GetSsid
(
    le_wifiClient_AccessPointRef_t accessPointRef,
        ///< [IN]
        ///< Wifi Access Point reference.

    uint8_t* SsidPtr,
        ///< [OUT]
        ///< The SSID returned as a octet array.

    size_t* SsidNumElementsPtr
        ///< [INOUT]
)
{
    FoundAccessPoint_t * accessPoint = le_ref_Lookup( ScanApRefMap, accessPointRef );
    LE_DEBUG( "le_wifiClient_GetSsid: ref %p", accessPointRef );
    if ( NULL == accessPoint )
    {
        LE_ERROR( "le_wifiClient_GetSsid: invalid accessPointRef" );
        return LE_BAD_PARAMETER;
    }

    if ( ( NULL == SsidPtr ) || ( NULL == SsidNumElementsPtr ) )
    {
        LE_ERROR( "le_wifiClient_GetSsid: parameter NULL %p %p", SsidPtr, SsidNumElementsPtr );
        return LE_BAD_PARAMETER;
    }

    *SsidNumElementsPtr = accessPoint->accessPoint.ssidLength;
    LE_DEBUG( "le_wifiClient_GetSsid: accessPoint->AccessPoint.ssidLength %d",
                        accessPoint->accessPoint.ssidLength );

    memcpy( &SsidPtr[0],
            &accessPoint->accessPoint.ssidBytes[0],
            accessPoint->accessPoint.ssidLength );

    return LE_OK;


}

//--------------------------------------------------------------------------------------------------
/**
 * Set the passphrase used to generate the PSK.
 *
 * @note the difference between le_wifiClient_SetPreSharedKey() and this function
 *
 * @return LE_FAULT         Function failed.
 * @return LE_BAD_PARAMETER Parameter is invalid.
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_SetPassphrase
(
    le_wifiClient_AccessPointRef_t accessPointRef,
        ///< [IN]
        ///< Wifi Access Point reference.

    const char* PassPhrase
        ///< [IN]
        ///< pass-phrase for PSK
)
{
    le_result_t result = LE_BAD_PARAMETER;
    LE_DEBUG( "le_wifiClient_SetPassphrase" );

    if ( NULL == le_ref_Lookup( ScanApRefMap, accessPointRef) )
    {
        LE_ERROR( "le_wifiClient_SetPassphrase: invalid accessPointRef" );
        return LE_BAD_PARAMETER;
    }

    if ( NULL != PassPhrase )
    {
        result =  pa_wifiClient_SetPassphrase( PassPhrase );
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the Pre Shared Key, PSK.
 * @note the difference between le_wifiClient_SetPassphrase() and this function
 *
 * @return LE_FAULT         Function failed.
 * @return LE_BAD_PARAMETER Parameter is invalid.
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_SetPreSharedKey
(
    le_wifiClient_AccessPointRef_t accessPointRef,
        ///< [IN]
        ///< Wifi Access Point reference.

    const char* PreSharedKey
        ///< [IN]
        ///< PSK. Note the difference between PSK and Pass Phrase.
)
{
    le_result_t result = LE_BAD_PARAMETER;
    LE_DEBUG( "le_wifiClient_SetPreSharedKey" );
    if ( NULL == le_ref_Lookup( ScanApRefMap, accessPointRef ) )
    {
        LE_ERROR( "le_wifiClient_SetPreSharedKey: invalid accessPointRef" );
        return LE_BAD_PARAMETER;
    }

    if ( NULL != PreSharedKey)
    {
        result = pa_wifiClient_SetPreSharedKey( PreSharedKey );
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * WPA-Enterprise requires a username and password to authenticate.
 * This function sets these parameters.
 *
 * @return LE_FAULT         Function failed.
 * @return LE_BAD_PARAMETER Parameter is invalid.
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_SetUserCredentials
(
    le_wifiClient_AccessPointRef_t accessPointRef,
        ///< [IN]
        ///< Wifi Access Point reference.

    const char* userName,
        ///< [IN]
        ///< UserName used for WPA-Enterprise.

    const char* password
        ///< [IN]
        ///< Password used for WPA-Enterprise.
)
{
    le_result_t result = LE_BAD_PARAMETER;
    LE_DEBUG( "le_wifiClient_SetUserCredentials" );
    if ( NULL == le_ref_Lookup( ScanApRefMap, accessPointRef ) )
    {
        LE_ERROR( "le_wifiClient_SetUserCredentials: invalid accessPointRef" );
        return LE_BAD_PARAMETER;
    }

    if (( NULL != userName) && ( NULL != password))
    {
        result = pa_wifiClient_SetUserCredentials( userName, password );
    }

    return result ;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the WEP key (WEP)
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_SetWepKey
(
    le_wifiClient_AccessPointRef_t accessPointRef,
        ///< [IN]
        ///< Wifi Access Point reference.

    const char* wepKey
        ///< [IN]
        ///< WEP key
)
{
    le_result_t result = LE_BAD_PARAMETER;
    LE_DEBUG( "le_wifiClient_SetWepKey" );
    if ( NULL == le_ref_Lookup( ScanApRefMap, accessPointRef ) )
    {
        LE_ERROR( "le_wifiClient_SetWepKey: invalid accessPointRef" );
        return LE_BAD_PARAMETER;
    }

    if ( NULL != wepKey )
    {
        result = pa_wifiClient_SetWepKey( wepKey );
    }

    return result ;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the security mode for connection
 *
 * @return LE_FAULT         Function failed.
 * @return LE_BAD_PARAMETER Parameter is invalid.
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_SetSecurityProtocol
(
    le_wifiClient_AccessPointRef_t accessPointRef,
        ///< [IN]
        ///< Wifi Access Point reference.

    le_wifiClient_SecurityProtocol_t securityProtocol
        ///< [IN]
        ///< Security Mode
)
{
    LE_DEBUG( "le_wifiClient_SetSecurityProtocol" );
    if( NULL == le_ref_Lookup(ScanApRefMap, accessPointRef) )
    {
        LE_ERROR( "le_wifiClient_SetSecurityProtocol: invalid accessPointRef" );
        return LE_BAD_PARAMETER;
    }

    return pa_wifiClient_SetSecurityProtocol( securityProtocol );
}

//--------------------------------------------------------------------------------------------------
/**
 * If an AccessPoint is not announcing it's precense, it will not show up in the scan.
 * But if the SSID is known, a connnection can be tried using this create function.
 * First create the AccessPoint, then le_wifiClient_Connect() to connect to it.
 *
 * @return AccessPoint reference to the current
 */
//--------------------------------------------------------------------------------------------------
le_wifiClient_AccessPointRef_t le_wifiClient_Create
(
    const uint8_t* SsidPtr,
        ///< [IN]
        ///< The SSID as a octet array.

    size_t SsidNumElements
        ///< [IN]
)
{
    le_wifiClient_AccessPointRef_t returRef = NULL;

    if( IsScanRunning() )
    {
        LE_ERROR( "le_wifiClient_Create ERROR: Scan is running" );
        return NULL;
    }

    if ( NULL == SsidPtr )
    {
        LE_ERROR( "le_wifiClient_Create ERROR: SsidPtr is NULL" );
        return NULL;
    }

    returRef = FindAccessPointRefFromSsid( SsidPtr, SsidNumElements );

    // if the access point does not already exist, then create it.
    if ( returRef == NULL )
    {
        FoundAccessPoint_t* createdAccessPointPtr = le_mem_ForceAlloc( AccessPointPool );

        if ( createdAccessPointPtr )
        {
            createdAccessPointPtr->foundInLatestScan = false;

            createdAccessPointPtr->accessPoint.signalStrength = SIGNAL_STRENGTH_DEFAULT;
            createdAccessPointPtr->accessPoint.ssidLength = SsidNumElements;
            memcpy( &createdAccessPointPtr->accessPoint.ssidBytes[0],
                SsidPtr,
                SsidNumElements);

            // Create a Safe Reference for this object.
            returRef = le_ref_CreateRef( ScanApRefMap, createdAccessPointPtr );

            LE_DEBUG( "le_wifiClient_Create AP[%p %p] SignalStrength %d"
                                "SSID length %d SSID: \"%.*s\"",
                createdAccessPointPtr,
                returRef,
                createdAccessPointPtr->accessPoint.signalStrength,
                createdAccessPointPtr->accessPoint.ssidLength,
                createdAccessPointPtr->accessPoint.ssidLength,
                ( char* ) createdAccessPointPtr->accessPoint.ssidBytes
                );
        }
        else
        {
            LE_ERROR( "le_wifiClient_Create le_mem_ForceAlloc failed." );
        }
    }

    return returRef;
}


//--------------------------------------------------------------------------------------------------
/**
 * Deletes an accessPointRef.
 *
 * @note The handle becomes invalid after it has been deleted.
 * @return LE_BAD_PARAMETER accessPointRef was not found.
 * @return LE_BUSY          Function called during scan.
 * @return LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_Delete
(
    le_wifiClient_AccessPointRef_t accessPointRef
        ///< [IN]
        ///< Wifi Access Point reference.
)
{
    le_result_t result = LE_BAD_PARAMETER;
    FoundAccessPoint_t * accessPointPtr = le_ref_Lookup( ScanApRefMap, accessPointRef );
    LE_DEBUG( "le_wifiClient_Delete called");

    if( IsScanRunning() )
    {
        LE_ERROR( "le_wifiClient_Delete ERROR: Scan is running" );
        return LE_BUSY;
    }

    // verify le_ref_Lookup
    if ( NULL !=  accessPointPtr)
    {
        RemoveAccessPoint( accessPointRef );
        result = LE_OK;
    }
    return result;
}
//--------------------------------------------------------------------------------------------------
/**
 * Connect to the Wifi Access Point.
 * All authentication must be set prior to calling this function.
 *
 * @return LE_FAULT         Function failed.
 * @return LE_BAD_PARAMETER Parameter is invalid.
 * @return LE_OK            Function succeeded.
 *
 * @note For PSK credentials see le_wifiClient_SetPassphrase() or le_wifiClient_SetPreSharedKey() .
 * @note For WPA-Enterprise credentials see le_wifiClient_SetUserCredentials()
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_Connect
(
    le_wifiClient_AccessPointRef_t accessPointRef
        ///< [IN]
        ///< Wifi Access Point reference.
)
{
    le_result_t result = LE_BAD_PARAMETER;
    FoundAccessPoint_t * accessPointPtr = le_ref_Lookup( ScanApRefMap, accessPointRef );
    LE_DEBUG( "le_wifiClient_Connect called. SSID length %d SSID: \"%.*s\"",
                accessPointPtr->accessPoint.ssidLength,
                accessPointPtr->accessPoint.ssidLength,
                ( char* ) accessPointPtr->accessPoint.ssidBytes);


    // verify le_ref_Lookup
    if ( NULL !=  accessPointPtr )
    {
        result = pa_wifiClient_Connect( accessPointPtr->accessPoint.ssidBytes,
                                        accessPointPtr->accessPoint.ssidLength );
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Disconnect from the Wifi Access Point.
 *
 * @return LE_FAULT         Function failed.
 * @return LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_Disconnect
(
)
{
    LE_DEBUG( "le_wifiClient_Disconnect");
    // Credentials needs to be set for each new connection.
    pa_wifiClient_ClearAllCredentials();

    return pa_wifiClient_Disconnect();

}


//--------------------------------------------------------------------------------------------------
/**
 *  Wifi Client COMPONENT Init
 */
//--------------------------------------------------------------------------------------------------
void le_wifiClient_Init
(
    void
)
{
    LE_DEBUG( "Wifi Client Service is ready" );

    pa_wifiClient_Init();

     // Create the Access Point object pool.
    AccessPointPool = le_mem_CreatePool( "le_wifi_FoundAccessPointPool",
                                         sizeof( FoundAccessPoint_t ) );
    le_mem_ExpandPool( AccessPointPool, INIT_NBR_OF_AP );

    // Create the Safe Reference Map to use for FoundAccessPoint_t object Safe References.
    ScanApRefMap = le_ref_CreateMap( "le_wifiClient_AccessPoints", INIT_NBR_OF_AP );

    // Create an event Id for new Wifi Events
    NewWifiEventId = le_event_CreateId( "WifiClientEvent", sizeof( le_wifiClient_Event_t ) );

    // register for events from PA.
    pa_wifiClient_AddEventHandler( PaEventHandler, NULL );


    // Add a handler to handle the close
    le_msg_AddServiceCloseHandler( le_wifiClient_GetServiceRef(),
                                   CloseSessionEventHandler,
                                   NULL );


}
