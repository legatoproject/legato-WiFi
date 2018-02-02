// -------------------------------------------------------------------------------------------------
/**
 *  Legato WiFi Client
 *
 *  Copyright (C) Sierra Wireless Inc.
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
#define INIT_AP_COUNT 32

//--------------------------------------------------------------------------------------------------
/**
 * Value of signal strength that indicates that no value was found.
 */
//-------------------------------------------------------------------------------------------------
#define SIGNAL_STRENGTH_DEFAULT  (0xfff)

//--------------------------------------------------------------------------------------------------
/**
 * Struct to hold the AccessPoint from the Scan's data.
 *
 */
//-------------------------------------------------------------------------------------------------
typedef struct
{
    pa_wifiClient_AccessPoint_t accessPoint;
    bool                        foundInLatestScan;
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
static uint32_t FoundWifiApCount = 0;

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
 * Event ID for WiFi Event message notification.
 *
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t NewWifiEventId;

//--------------------------------------------------------------------------------------------------
/**
 * The number of calls to le_wifiClient_Start().
 * This is used to call stop the WiFi hardware, when the last client calls stop.
 */
//--------------------------------------------------------------------------------------------------
static uint32_t ClientStartCount = 0;


//--------------------------------------------------------------------------------------------------
/**
 * CallBack for PA Events.
 */
//--------------------------------------------------------------------------------------------------
static void PaEventHandler
(
    le_wifiClient_Event_t event,
    void *contextPtr
)
{
    LE_DEBUG("Event: %d ", event);

    le_event_Report(NewWifiEventId, (void *)&event, sizeof(le_wifiClient_Event_t));
}

//--------------------------------------------------------------------------------------------------
/**
 * Local function to find an access point reference based on BSSID among the AP found in scan.
 * If not found will return NULL.
 */
//--------------------------------------------------------------------------------------------------
static le_wifiClient_AccessPointRef_t FindAccessPointRefFromBssid
(
    const char* bssidPtr
        ///< [OUT]
        ///< The BSSID returned as a byte array.
)
{
    le_wifiClient_AccessPointRef_t apRef = NULL;
    le_ref_IterRef_t               iter  = le_ref_GetIterator(ScanApRefMap);

    LE_DEBUG("Find AP from BSSID");

    while (le_ref_NextNode(iter) == LE_OK)
    {
        apRef = (le_wifiClient_AccessPointRef_t)le_ref_GetSafeRef(iter);
        if (NULL != apRef)
        {
            FoundAccessPoint_t *apPtr = (FoundAccessPoint_t *)le_ref_Lookup(ScanApRefMap, apRef);
            if (NULL != apPtr)
            {
                if (0 == strncmp(apPtr->accessPoint.bssid, bssidPtr, LE_WIFIDEFS_MAX_BSSID_BYTES))
                {
                    LE_DEBUG("Found apRef %p", apRef);
                    return apRef;
                }
            }
        }
        else
        {
            LE_ERROR("ERROR le_ref_GetSafeRef returned NULL iter:%p", iter);
        }
    }
    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Local function to find an access point reference based on SSID among the AP found in scan.
 * If not found will return NULL.
 */
//--------------------------------------------------------------------------------------------------
static le_wifiClient_AccessPointRef_t FindAccessPointRefFromSsid
(
    const uint8_t* ssidPtr,
        ///< [OUT]
        ///< The SSID returned as a byte array.

    size_t ssidNumElements
        ///< [INOUT]
        ///< SSID length in bytes.
)
{
    le_wifiClient_AccessPointRef_t apRef = NULL;
    le_ref_IterRef_t               iter  = le_ref_GetIterator(ScanApRefMap);

    LE_DEBUG("Find AP from SSID");

    while (le_ref_NextNode(iter) == LE_OK)
    {
        apRef = (le_wifiClient_AccessPointRef_t)le_ref_GetSafeRef(iter);
        if (NULL != apRef)
        {
            FoundAccessPoint_t *apPtr = (FoundAccessPoint_t *)le_ref_Lookup(ScanApRefMap, apRef);
            if (NULL != apPtr)
            {
                if ((apPtr->accessPoint.ssidLength == ssidNumElements))
                {
                    if (0 == memcmp(apPtr->accessPoint.ssidBytes, ssidPtr, ssidNumElements))
                    {
                        LE_DEBUG("Found apRef %p", apRef);
                        return apRef;
                    }
                }
            }
        }
        else
        {
            LE_ERROR("ERROR le_ref_GetSafeRef returned NULL iter:%p", iter);
        }
    }
    return NULL;
}


//--------------------------------------------------------------------------------------------------
/**
 * Local function to add AP:s found during scan to AddRef point interface
 */
//--------------------------------------------------------------------------------------------------

static le_wifiClient_AccessPointRef_t AddAccessPointToApRefMap
(
    pa_wifiClient_AccessPoint_t *apPtr
)
{
    // first see if it alreay exists in our list of reference.
    le_wifiClient_AccessPointRef_t returnedRef = FindAccessPointRefFromBssid(apPtr->bssid);

    if (NULL != returnedRef)
    {
        FoundAccessPoint_t *oldAccessPointPtr =
            (FoundAccessPoint_t *)le_ref_Lookup(ScanApRefMap, returnedRef);


        if (NULL != oldAccessPointPtr)
        {
            LE_DEBUG("Already exists %p. Update  SignalStrength %d",
                returnedRef, apPtr->signalStrength);

            oldAccessPointPtr->accessPoint.signalStrength = apPtr->signalStrength;
            oldAccessPointPtr->foundInLatestScan = true;
        }

        return returnedRef;
    }
    else
    {
        FoundAccessPoint_t *foundAccessPointPtr = le_mem_ForceAlloc(AccessPointPool);

        if (foundAccessPointPtr)
        {
            FoundWifiApCount++;
            LE_DEBUG("New AP[%d] SignalStrength %d | SSID length %d | SSID: \"%.*s\"",
                FoundWifiApCount,
                apPtr->signalStrength,
                apPtr->ssidLength,
                apPtr->ssidLength,
                (char *)apPtr->ssidBytes
               );

            // struct member value copy
            foundAccessPointPtr->accessPoint = *apPtr;
            foundAccessPointPtr->foundInLatestScan = true;

            // Create a Safe Reference for this object.
            returnedRef = le_ref_CreateRef(ScanApRefMap, foundAccessPointPtr);

            LE_DEBUG("le_ref_CreateRef foundAccessPointPtr %p; Ref%p ",
                foundAccessPointPtr, returnedRef);
        }
        else
        {
            LE_ERROR("le_mem_ForceAlloc failed. Count %d", FoundWifiApCount);
        }
    }

    return returnedRef;
}


//--------------------------------------------------------------------------------------------------
/**
 *  Frees one members of the access point and the corresponding access points memory
 */
//--------------------------------------------------------------------------------------------------

static void RemoveAccessPoint
(
    le_wifiClient_AccessPointRef_t apRef
)
{
    FoundAccessPoint_t *apPtr = (FoundAccessPoint_t *)le_ref_Lookup(ScanApRefMap, apRef);

    if (apPtr == NULL)
    {
        LE_ERROR("Bad reference");
        return;
    }

    le_ref_DeleteRef(ScanApRefMap, apRef);
    le_mem_Release(apPtr);
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
    le_wifiClient_AccessPointRef_t apRef = NULL;
    le_ref_IterRef_t               iter  = le_ref_GetIterator(ScanApRefMap);

    LE_DEBUG("Release all AP");

    while (le_ref_NextNode(iter) == LE_OK)
    {
        apRef = (le_wifiClient_AccessPointRef_t)le_ref_GetSafeRef(iter);
        if (NULL != apRef)
        {
            RemoveAccessPoint(apRef);
        }
        else
        {
            LE_ERROR("ERROR le_ref_GetSafeRef returned NULL iter:%p", iter);
            return;
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Marks the current access points as old by changing the values for foundInLatestScan
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
    le_wifiClient_AccessPointRef_t apRef   = NULL;
    le_ref_IterRef_t               iter    = le_ref_GetIterator(ScanApRefMap);
    uint32_t                       counter = 0;

    LE_DEBUG("Mark all AP as old");

    while (le_ref_NextNode(iter) == LE_OK)
    {
        apRef = (le_wifiClient_AccessPointRef_t)le_ref_GetSafeRef(iter);
        if (NULL != apRef)
        {
            FoundAccessPoint_t* apPtr = (FoundAccessPoint_t *)le_ref_Lookup(ScanApRefMap, apRef);

            if (apPtr != NULL)
            {
                apPtr->accessPoint.signalStrength = SIGNAL_STRENGTH_DEFAULT;
                apPtr->foundInLatestScan = false;
                LE_DEBUG("Marking %p as old", apRef);
                counter++;
            }
            else
            {
                LE_ERROR("Bad reference");
                return;
            }

        }
        else
        {
            LE_ERROR("ERROR le_ref_GetSafeRef returned NULL iter:%p", iter);
            break;
        }
    }
    LE_DEBUG("Marked: %d", counter);
}


//--------------------------------------------------------------------------------------------------
/**
 * Start Scanning for WiFi Access points
 * Will result in event LE_WIFICLIENT_EVENT_SCAN_DONE when the scan results are available.
 *
 * @return LE_FAULT         Function failed.
 * @return LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
static void *ScanThread
(
    void *contextPtr
)
{
    pa_wifiClient_AccessPoint_t accessPoint;
    le_result_t                 paResult    = pa_wifiClient_Scan();

    if (LE_OK != paResult)
    {
        LE_ERROR("Scan failed (%d)", paResult);
        return NULL;
    }

    FoundWifiApCount = 0;

    MarkAllAccessPointsOld();

    while (LE_OK == (paResult = pa_wifiClient_GetScanResult(&accessPoint)))
    {
        AddAccessPointToApRefMap(&accessPoint);
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
    LE_DEBUG("Scan thread exited.");
    ScanThreadRef = NULL;
    // use the PA callback to generate the event.
    PaEventHandler(LE_WIFICLIENT_EVENT_SCAN_DONE, NULL);
}


//--------------------------------------------------------------------------------------------------
/**
 * Handler function to the close session service to detect if the application crashed.
 *
 */
//--------------------------------------------------------------------------------------------------
static void CloseSessionEventHandler
(
    le_msg_SessionRef_t  sessionRef,
    void                *contextPtr
)
{
    LE_DEBUG("sessionRef %p GetFirstSessionRef %p", sessionRef, GetFirstSessionRef);

    if (sessionRef == GetFirstSessionRef)
    {
        GetFirstSessionRef = NULL;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Is Scan running. Checks if the ScanThread is still running
 */
//--------------------------------------------------------------------------------------------------
static bool IsScanRunning(void)
{
    LE_DEBUG("IsScanRunning .%d", (NULL != ScanThreadRef));
    return (NULL != ScanThreadRef);
}


//--------------------------------------------------------------------------------------------------
/**
 * The first-layer WiFi Client Event Handler.
 *
 */
//--------------------------------------------------------------------------------------------------
static void FirstLayerWifiClientEventHandler
(
    void *reportPtr,
    void *secondLayerHandlerFunc
)
{
    le_wifiClient_NewEventHandlerFunc_t  clientHandlerFunc = secondLayerHandlerFunc;
    le_wifiClient_Event_t               *wifiEventPtr      = (le_wifiClient_Event_t *)reportPtr;

    if (NULL != wifiEventPtr)
    {
        LE_DEBUG("Event: %d", *wifiEventPtr);
        clientHandlerFunc(*wifiEventPtr, le_event_GetContextPtr());
    }
    else
    {
        LE_ERROR("Event is NULL");
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
    le_wifiClient_AccessPointRef_t apRef
        ///< [IN]
        ///< WiFi Access Point reference.
)
{
    FoundAccessPoint_t *apPtr = (FoundAccessPoint_t *)le_ref_Lookup(ScanApRefMap, apRef);

    if (apPtr == NULL)
    {
        LE_ERROR("Bad reference");
        return false;
    }

    return apPtr->foundInLatestScan;
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
        ///< Event handling function

    void *contextPtr
        ///< [IN]
        ///< Associated event context
)
{
    le_event_HandlerRef_t handlerRef;

    // Note: THIS ONE REGISTERS THE CB function..
    LE_DEBUG("Add new event handler");

    if (handlerFuncPtr == NULL)
    {
        LE_KILL_CLIENT("handlerFuncPtr is NULL !");
        return NULL;
    }

    handlerRef = le_event_AddLayeredHandler("NewWiFiClientMsgHandler",
        NewWifiEventId,
        FirstLayerWifiClientEventHandler,
        (le_event_HandlerFunc_t)handlerFuncPtr);

    le_event_SetContextPtr(handlerRef, contextPtr);

    return (le_wifiClient_NewEventHandlerRef_t)(handlerRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'le_wifiClient_NewEvent'
 */
//--------------------------------------------------------------------------------------------------
void le_wifiClient_RemoveNewEventHandler
(
    le_wifiClient_NewEventHandlerRef_t handlerRef
        ///< [IN]
        ///< Reference of the event handler to remove
)
{
    LE_DEBUG("Remove event handler");
    le_event_RemoveHandler((le_event_HandlerRef_t)handlerRef);
}


//--------------------------------------------------------------------------------------------------
/**
 * This function starts the WiFi device.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_BUSY          If the WiFi device is already started.
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

    LE_DEBUG("Client starts");

    // Count the number of Clients calling start.
    ClientStartCount++;

    // first client starts the hardware
    if (1 == ClientStartCount)
    {
        pa_result = pa_wifiClient_Start();
    }

    if (LE_OK != pa_result)
    {
        LE_ERROR("ERROR: pa_wifiClient_Start returns %d", pa_result);
    }

    return pa_result;
}


//--------------------------------------------------------------------------------------------------
/**
 * This function stops the WiFi device.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_DUPLICATE     If the WiFi device is already stopped.
 * @return LE_OK            The function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_Stop
(
    void
)
{
    if (ClientStartCount)
    {
        ClientStartCount--;

        LE_DEBUG("ClientStartCount %d", ClientStartCount);

        // last client closes the hardware
        if (0 == ClientStartCount)
        {
            LE_INFO("Clearing credentials...");
            pa_wifiClient_ClearAllCredentials();
            LE_INFO("Stopping client...");
            pa_wifiClient_Stop();
            LE_INFO("Releasing all APs.");
            ReleaseAllAccessPoints();
            LE_DEBUG("Last client pa_wifiClient_Release.");
            LE_INFO("Last client pa_wifiClient_Release.");
        }
    }
    else
    {
        LE_INFO("WiFi is already stopped.");
    }

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Start scanning for WiFi access points
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
    if (!IsScanRunning())
    {
        LE_DEBUG("Scan started");

        // Start the thread
        ScanThreadRef = le_thread_Create("WiFi Client Scan Thread", ScanThread, NULL);
        le_thread_AddChildDestructor(ScanThreadRef, ScanThreadDestructor, NULL);

        le_thread_Start(ScanThreadRef);
        return LE_OK;
    }
    else
    {
        LE_DEBUG("ERROR: Scan already running");
        return LE_BUSY;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the first WiFi access point found.
 * It returns the first discovered access point.
 *
 * @return WiFi access point reference if ok.
 * @return NULL if no access point reference available.
 */
//--------------------------------------------------------------------------------------------------
le_wifiClient_AccessPointRef_t le_wifiClient_GetFirstAccessPoint
(
    void
)
{
    le_wifiClient_AccessPointRef_t apRef = NULL;
    bool                           found = false;

    if (IsScanRunning())
    {
        LE_ERROR("ERROR: Scan is running.");
        return NULL;
    }
    IterRef = le_ref_GetIterator(ScanApRefMap);
    GetFirstSessionRef = le_wifiClient_GetClientSessionRef();

    LE_DEBUG("Get first AP");

    while (le_ref_NextNode(IterRef) == LE_OK)
    {
        apRef = (le_wifiClient_AccessPointRef_t)le_ref_GetSafeRef(IterRef);
        if (WasApFoundInLastScan(apRef))
        {
            found = true;
            break;
        }
    }

    if (found)
    {
        LE_DEBUG("AP ref = %p", apRef);
        return apRef;
    }
    else
    {
        LE_DEBUG("AP not found");
        return NULL;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the next WiFi Access Point.
 * Will return the Access Points in the order of found.
 * This function must be called in the same context as the GetFirstAccessPoint
 *
 * @return WiFi Access Point reference if ok.
 * @return NULL if no Access Point reference available.
*/
//--------------------------------------------------------------------------------------------------
le_wifiClient_AccessPointRef_t le_wifiClient_GetNextAccessPoint
(
    void
)
{
    le_wifiClient_AccessPointRef_t apRef = NULL;
    bool                           found = false;

    LE_DEBUG("Get next AP");
    if (IsScanRunning())
    {
        LE_ERROR("ERROR: Scan is running.");
        return NULL;
    }

    /* This check to protect the variable IterRef that shouldn't be called from different contexts*/
    if (le_wifiClient_GetClientSessionRef() != GetFirstSessionRef)
    {
        LE_ERROR("ERROR: Called from different context than GetFirstAccessPoint");
        return NULL;
    }

    while (le_ref_NextNode(IterRef) == LE_OK)
    {
        apRef = (le_wifiClient_AccessPointRef_t)le_ref_GetSafeRef(IterRef);
        LE_DEBUG("AP ref = %p", apRef);
        if (WasApFoundInLastScan(apRef))
        {
            found = true;
            break;
        }
    }

    if (found)
    {
        LE_DEBUG("AP ref = %p", apRef);
        return apRef;
    }
    else
    {
        LE_DEBUG("AP not found");
        GetFirstSessionRef = NULL;
        return NULL;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the signal strength of the Access Point
 *
 * @return
 *  - signal strength in dBm. Example -30 = -30dBm
 *  - if no signal available it will return OxFFFF
 */
//--------------------------------------------------------------------------------------------------
int16_t le_wifiClient_GetSignalStrength
(
    le_wifiClient_AccessPointRef_t apRef
        ///< [IN]
        ///< WiFi Access Point reference.
)
{
    FoundAccessPoint_t *apPtr = le_ref_Lookup(ScanApRefMap, apRef);

    LE_DEBUG("Get signal strength");
    if (NULL == apPtr)
    {
        LE_ERROR("Invalid access point reference.");
        return SIGNAL_STRENGTH_DEFAULT;
    }

    return apPtr->accessPoint.signalStrength;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the BSSID of the AccessPoint
 *
 * @return LE_FAULT         Function failed.
 * @return LE_BAD_PARAMETER Some parameter is invalid.
 * @return LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_GetBssid
(
    le_wifiClient_AccessPointRef_t apRef,
        ///< [IN]
        ///< WiFi Access Point reference.

    char *bssidPtr,
        ///< [OUT]
        ///< The BSSID

    size_t bssidSize
        ///< [IN]
)
{
    FoundAccessPoint_t *apPtr = le_ref_Lookup(ScanApRefMap, apRef);

    LE_DEBUG("AP ref %p", apRef);
    if (NULL == apPtr)
    {
        LE_ERROR("Invalid access point reference.");
        return LE_BAD_PARAMETER;
    }

    if (NULL == bssidPtr)
    {
        LE_ERROR("Invalid parameter BSSID = %p", bssidPtr);
        return LE_BAD_PARAMETER;
    }

    if (strnlen(apPtr->accessPoint.bssid, LE_WIFIDEFS_MAX_BSSID_BYTES) > bssidSize)
    {
        return LE_OVERFLOW;
    }
    strncpy(bssidPtr, apPtr->accessPoint.bssid, LE_WIFIDEFS_MAX_BSSID_BYTES);

    return LE_OK;
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
    le_wifiClient_AccessPointRef_t apRef,
        ///< [IN]
        ///< WiFi Access Point reference.

    uint8_t *ssidPtr,
        ///< [OUT]
        ///< The SSID returned as a octet array.

    size_t *ssidNumElementsPtr
        ///< [INOUT]
        ///< SSID length in octets.
)
{
    FoundAccessPoint_t *apPtr = le_ref_Lookup(ScanApRefMap, apRef);

    LE_DEBUG("AP ref %p", apRef);
    if (NULL == apPtr)
    {
        LE_ERROR("Invalid access point reference.");
        return LE_BAD_PARAMETER;
    }

    if ((NULL == ssidPtr) || (NULL == ssidNumElementsPtr))
    {
        LE_ERROR("Invalid parameter SSID = %p, SSID length = %p", ssidPtr, ssidNumElementsPtr);
        return LE_BAD_PARAMETER;
    }

    *ssidNumElementsPtr = apPtr->accessPoint.ssidLength;
    LE_DEBUG("apPtr->AccessPoint.ssidLength %d", apPtr->accessPoint.ssidLength);

    memcpy(&ssidPtr[0], &apPtr->accessPoint.ssidBytes[0], apPtr->accessPoint.ssidLength);

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the passphrase used to generate the PSK.
 *
 * @note This is one way to authenticate against the access point. The other one is provided by the
 * le_wifiClient_SetPreSharedKey() function. Both ways are exclusive and are effective only when used
 * with WPA-personal authentication.
 *
 * @return LE_BAD_PARAMETER Parameter is invalid.
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_SetPassphrase
(
    le_wifiClient_AccessPointRef_t apRef,
        ///< [IN]
        ///< WiFi Access Point reference.

    const char *passPhrasePtr
        ///< [IN]
        ///< pass-phrase for PSK
)
{
    le_result_t result = LE_BAD_PARAMETER;

    LE_DEBUG("Set passphrase");

    if (NULL == le_ref_Lookup(ScanApRefMap, apRef))
    {
        LE_ERROR("Invalid access point reference.");
        return LE_BAD_PARAMETER;
    }

    if (NULL != passPhrasePtr)
    {
        result = pa_wifiClient_SetPassphrase(passPhrasePtr);
    }

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the pre-shared key (PSK).
 *
 * @note This is one way to authenticate against the access point. The other one is provided by the
 * le_wifiClient_SetPassPhrase() function. Both ways are exclusive and are effective only when used
 * with WPA-personal authentication.
 *
 * @return LE_BAD_PARAMETER Parameter is invalid.
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_SetPreSharedKey
(
    le_wifiClient_AccessPointRef_t apRef,
        ///< [IN]
        ///< WiFi Access Point reference.

    const char *preSharedKeyPtr
        ///< [IN]
        ///< Pre-shared key used to authenticate against the access point.
)
{
    le_result_t result = LE_BAD_PARAMETER;

    LE_DEBUG("Set PSK");
    if (NULL == le_ref_Lookup(ScanApRefMap, apRef))
    {
        LE_ERROR("Invalid access point reference.");
        return LE_BAD_PARAMETER;
    }

    if (NULL != preSharedKeyPtr)
    {
        result = pa_wifiClient_SetPreSharedKey(preSharedKeyPtr);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function specifies whether the target Access Point is hiding its presence from clients or
 * not. When an Access Point is hidden, it cannot be discovered by a scan process.
 *
 * @note By default, this attribute is not set which means that the client is unable to connect to
 * a hidden access point. When enabled, the client will be able to connect to the access point
 * whether it is hidden or not.
 *
 * @return LE_BAD_PARAMETER Parameter is invalid.
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_SetHiddenNetworkAttribute
(
    le_wifiClient_AccessPointRef_t apRef,
        ///< [IN]
        ///< WiFi Access Point reference.

    bool hidden
        ///< [IN]
        ///< If TRUE, the WIFI client will be able to connect to a hidden access point.
)
{
    LE_DEBUG("Set whether access point is hidden or not: %d", hidden);
    if (NULL == le_ref_Lookup(ScanApRefMap, apRef))
    {
        LE_ERROR("Invalid access point reference.");
        return LE_BAD_PARAMETER;
    }

    pa_wifiClient_SetHiddenNetworkAttribute(hidden);
    return LE_OK;
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
    le_wifiClient_AccessPointRef_t apRef,
        ///< [IN]
        ///< WiFi Access Point reference.

    const char *userNamePtr,
        ///< [IN]
        ///< UserName used for WPA-Enterprise authentication protocol.

    const char *passwordPtr
        ///< [IN]
        ///< Password used for WPA-Enterprise authentication protocol.
)
{
    le_result_t result = LE_BAD_PARAMETER;

    LE_DEBUG("Set user credentials");
    if (NULL == le_ref_Lookup(ScanApRefMap, apRef))
    {
        LE_ERROR("Invalid access point reference.");
        return LE_BAD_PARAMETER;
    }

    if ((NULL != userNamePtr) && (NULL != passwordPtr))
    {
        result = pa_wifiClient_SetUserCredentials(userNamePtr, passwordPtr);
    }

    return result;
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
    le_wifiClient_AccessPointRef_t apRef,
        ///< [IN]
        ///< WiFi Access Point reference.

    const char *wepKeyPtr
        ///< [IN]
        ///< Wired Equivalent Privacy (WEP) key used for authentication
)
{
    le_result_t result = LE_BAD_PARAMETER;

    LE_DEBUG("Set WEP key");
    if (NULL == le_ref_Lookup(ScanApRefMap, apRef))
    {
        LE_ERROR("Invalid access point reference.");
        return LE_BAD_PARAMETER;
    }

    if (NULL != wepKeyPtr)
    {
        result = pa_wifiClient_SetWepKey(wepKeyPtr);
    }

    return result;
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
    le_wifiClient_AccessPointRef_t apRef,
        ///< [IN]
        ///< WiFi Access Point reference.

    le_wifiClient_SecurityProtocol_t securityProtocol
        ///< [IN]
        ///< Security Mode
)
{
    LE_DEBUG("Set security protocol");
    if (NULL == le_ref_Lookup(ScanApRefMap, apRef))
    {
        LE_ERROR("Invalid access point reference.");
        return LE_BAD_PARAMETER;
    }

    return pa_wifiClient_SetSecurityProtocol(securityProtocol);
}


//--------------------------------------------------------------------------------------------------
/**
 * This function creates a reference to an Access Point given its SSID.
 * If an Access Point is hidden, it will not show up in the scan. So, its SSID must be known
 * in advance in order to create a reference.
 *
 * @note This function fails if called while scan is running.
 *
 * @return Access Point reference to the current
 */
//--------------------------------------------------------------------------------------------------
le_wifiClient_AccessPointRef_t le_wifiClient_Create
(
    const uint8_t *ssidPtr,
        ///< [IN]
        ///< The SSID as a octet array.

    size_t ssidNumElements
        ///< [IN]
        ///< Length of the SSID in octets.
)
{
    le_wifiClient_AccessPointRef_t returnedRef = NULL;

    if (IsScanRunning())
    {
        LE_ERROR("ERROR: Scan is running");
        return NULL;
    }

    if (NULL == ssidPtr)
    {
        LE_ERROR("ERROR: ssidPtr is NULL");
        return NULL;
    }

    returnedRef = FindAccessPointRefFromSsid(ssidPtr, ssidNumElements);

    // if the access point does not already exist, then create it.
    if (returnedRef == NULL)
    {
        FoundAccessPoint_t* createdAccessPointPtr = le_mem_ForceAlloc(AccessPointPool);

        if (createdAccessPointPtr)
        {
            createdAccessPointPtr->foundInLatestScan = false;

            createdAccessPointPtr->accessPoint.signalStrength = SIGNAL_STRENGTH_DEFAULT;
            createdAccessPointPtr->accessPoint.ssidLength = ssidNumElements;
            memcpy(&createdAccessPointPtr->accessPoint.ssidBytes[0],
                ssidPtr,
                ssidNumElements);

            // Create a Safe Reference for this object.
            returnedRef = le_ref_CreateRef(ScanApRefMap, createdAccessPointPtr);

            LE_DEBUG("AP[%p %p] signal strength %d | SSID length %d | SSID: \"%.*s\"",
                createdAccessPointPtr,
                returnedRef,
                createdAccessPointPtr->accessPoint.signalStrength,
                createdAccessPointPtr->accessPoint.ssidLength,
                createdAccessPointPtr->accessPoint.ssidLength,
                (char *)createdAccessPointPtr->accessPoint.ssidBytes);
        }
        else
        {
            LE_ERROR("le_wifiClient_Create le_mem_ForceAlloc failed.");
        }
    }

    return returnedRef;
}


//--------------------------------------------------------------------------------------------------
/**
 * Deletes an apRef.
 *
 * @note The handle becomes invalid after it has been deleted.
 * @return LE_BAD_PARAMETER apRef was not found.
 * @return LE_BUSY          Function called during scan.
 * @return LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_Delete
(
    le_wifiClient_AccessPointRef_t apRef
        ///< [IN]
        ///< WiFi Access Point reference.
)
{
    le_result_t         result = LE_BAD_PARAMETER;
    FoundAccessPoint_t *apPtr  = le_ref_Lookup(ScanApRefMap, apRef);

    LE_DEBUG("Delete client called");

    if (IsScanRunning())
    {
        LE_ERROR("ERROR: Scan is running");
        return LE_BUSY;
    }

    // verify le_ref_Lookup
    if (NULL != apPtr)
    {
        RemoveAccessPoint(apRef);
        result = LE_OK;
    }
    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Connect to the WiFi Access Point.
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
    le_wifiClient_AccessPointRef_t apRef
        ///< [IN]
        ///< WiFi access point reference.
)
{
    le_result_t         result = LE_BAD_PARAMETER;
    FoundAccessPoint_t *apPtr  = le_ref_Lookup(ScanApRefMap, apRef);

    // verify le_ref_Lookup
    if (NULL !=  apPtr)
    {
        LE_DEBUG("SSID length %d | SSID: \"%.*s\"",
            apPtr->accessPoint.ssidLength,
            apPtr->accessPoint.ssidLength,
            (char *)apPtr->accessPoint.ssidBytes);

        result = pa_wifiClient_Connect(apPtr->accessPoint.ssidBytes, apPtr->accessPoint.ssidLength);
    }
    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Disconnect from the WiFi Access Point.
 *
 * @return LE_FAULT         Function failed.
 * @return LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_Disconnect
(
)
{
    LE_DEBUG("Disconnect");
    return pa_wifiClient_Disconnect();
}


//--------------------------------------------------------------------------------------------------
/**
 *  WiFi Client COMPONENT Init
 */
//--------------------------------------------------------------------------------------------------
void le_wifiClient_Init
(
    void
)
{
    LE_DEBUG("WiFi client service starting...");

    pa_wifiClient_Init();

    // Create the Access Point object pool.
    AccessPointPool = le_mem_CreatePool("le_wifi_FoundAccessPointPool", sizeof(FoundAccessPoint_t));
    le_mem_ExpandPool(AccessPointPool, INIT_AP_COUNT);

    // Create the Safe Reference Map to use for FoundAccessPoint_t object Safe References.
    ScanApRefMap = le_ref_CreateMap("le_wifiClient_AccessPoints", INIT_AP_COUNT);

    // Create an event Id for new WiFi Events
    NewWifiEventId = le_event_CreateId("WifiClientEvent", sizeof(le_wifiClient_Event_t));

    // register for events from PA.
    pa_wifiClient_AddEventHandler(PaEventHandler, NULL);

    // Add a handler to handle the close
    le_msg_AddServiceCloseHandler(le_wifiClient_GetServiceRef(), CloseSessionEventHandler, NULL);
}

