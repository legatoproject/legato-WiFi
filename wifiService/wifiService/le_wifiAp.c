// -------------------------------------------------------------------------------------------------
/**
 *  Legato Wifi Access Point
 *
 *  Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 *
 */
// -------------------------------------------------------------------------------------------------
#include "legato.h"

#include "interfaces.h"

#include "pa_wifi_ap.h"


//--------------------------------------------------------------------------------------------------
/**
 * Event ID for Wifi AccessPoint Event message notification.
 *
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t NewWifiApEventId;

//--------------------------------------------------------------------------------------------------
/**
 * CallBack for PA Access Point Events.
 */
//--------------------------------------------------------------------------------------------------

static void PaEventApHandler
(
    le_wifiAp_Event_t event
)
{

    LE_DEBUG( "PaEventApHandler event: %d", event );

    le_event_Report( NewWifiApEventId, (void*)&event, sizeof( le_wifiAp_Event_t ) );
}


//--------------------------------------------------------------------------------------------------
/**
 * The first-layer Wifi Ap Event Handler.
 *
 */
//--------------------------------------------------------------------------------------------------
static void FirstLayerWifiApEventHandler
(
    void* reportPtr,
    void* secondLayerHandlerFunc
)
{
    le_wifiAp_EventHandlerFunc_t clientHandlerFunc = secondLayerHandlerFunc;

    le_wifiAp_Event_t  * wifiEvent = (le_wifiAp_Event_t*)reportPtr;


    if ( NULL != wifiEvent)
    {
        LE_DEBUG( "FirstLayerWifiApEventHandler event: %d", *wifiEvent );
        clientHandlerFunc( *wifiEvent, le_event_GetContextPtr());
    }
    else
    {
        LE_ERROR( "FirstLayerWifiApEventHandler event is NULL" );
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'le_wifiAp_NewEvent'
 *
 * These events provide information on Wifi Access Point
 */
//--------------------------------------------------------------------------------------------------
le_wifiAp_NewEventHandlerRef_t le_wifiAp_AddNewEventHandler
(
    le_wifiAp_EventHandlerFunc_t handlerFuncPtr,
        ///< [IN]

    void* contextPtr
        ///< [IN]
)
{
    LE_DEBUG( "le_wifiAp_AddNewEventHandler");
    le_event_HandlerRef_t handlerRef;

    if( handlerFuncPtr == NULL )
    {
        LE_KILL_CLIENT("handlerFuncPtr is NULL !" );
        return NULL;
    }

    handlerRef = le_event_AddLayeredHandler( "NewWiFiApMsgHandler",
                    NewWifiApEventId,
                    FirstLayerWifiApEventHandler,
                    ( le_event_HandlerFunc_t ) handlerFuncPtr );

    le_event_SetContextPtr( handlerRef, contextPtr );

    return ( le_wifiAp_NewEventHandlerRef_t )( handlerRef );
}

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'le_wifiAp_NewEvent'
 */
//--------------------------------------------------------------------------------------------------
void le_wifiAp_RemoveNewEventHandler
(
    le_wifiAp_NewEventHandlerRef_t addHandlerRef
        ///< [IN]
)
{
}

//--------------------------------------------------------------------------------------------------
/**
 * This function starts the WIFI Access Point.
 * Note that all settings, if to be used, such as security, username, password must set prior to
 * starting the Access Point.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_BUSY          If the WIFI device is already started.
 * @return LE_OK            The function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiAp_Start
(
    void
)
{
    return pa_wifiAp_Start();
}

//--------------------------------------------------------------------------------------------------
/**
 * This function stops the WIFI Access Point.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_DUPLICATE     If the WIFI device is already stopped.
 * @return LE_OK            The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiAp_Stop
(
    void
)
{
    return pa_wifiAp_Stop();
}



//--------------------------------------------------------------------------------------------------
/**
 * Set the Service set identification (SSID) of the AccessPoint
 * Default value is "LEGATO Access Point"
 * @note that the SSID does not have to be human readable ASCII values, but often has.
 *
 * @return LE_FAULT         Function failed.
 * @return LE_BAD_PARAMETER Some parameter is invalid.
 * @return LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiAp_SetSsid
(
    const uint8_t* ssidPtr,
        ///< [IN]
        ///< The SSID to set as a octet array.

    size_t ssidNumElements
        ///< [IN]
)
{
    return pa_wifiAp_SetSsid( ssidPtr, ssidNumElements );
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the Security protocol to use.
 * Default value is SECURITY_WPA2.
 * @note that the SSID does not have to be human readable ASCII values, but often has.
 *
 * @return LE_FAULT         Function failed.
 * @return LE_BAD_PARAMETER Some parameter is invalid.
 * @return LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiAp_SetSecurityProtocol
(
    le_wifiAp_SecurityProtocol_t securityProtocol
        ///< [IN]
        ///< The security protocol to use.
)
{
    return pa_wifiAp_SetSecurityProtocol( securityProtocol );
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the passphrase used to generate the PSK.
 * Default value is "ChangeThisPassword".
 *
 * @note the difference between le_wifiAp_SetPreSharedKey() and this function
 *
 * @return LE_FAULT         Function failed.
 * @return LE_BAD_PARAMETER Parameter is invalid.
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiAp_SetPassPhrase
(
    const char* passPhrase
        ///< [IN]
        ///< pass-phrase for PSK
)
{
    return pa_wifiAp_SetPassPhrase( passPhrase );
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the Pre Shared Key, PSK.
 * There is no default value, since le_wifiAp_SetPassPhrase is used as default.
 * @note the difference between le_wifiAp_SetPassPhrase() and this function
 *
 * @return LE_FAULT         Function failed.
 * @return LE_BAD_PARAMETER Parameter is invalid.
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiAp_SetPreSharedKey
(
    const char* preSharedKey
        ///< [IN]
        ///< PSK. Note the difference between PSK and Pass Phrase.
)
{
    return pa_wifiAp_SetPreSharedKey( preSharedKey );
}


//--------------------------------------------------------------------------------------------------
/**
 * Set if the Access Point should announce its presence.
 * Default value is TRUE.
 * If the value is set to FALSE, the Access Point will be hidden.
 *
 * @return LE_FAULT         Function failed.
 * @return LE_BAD_PARAMETER Parameter is invalid.
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiAp_SetDiscoverable
(
    bool discoverable
        ///< [IN]
        ///< If TRUE the Access Point shows up on scans, else it is hidden.
)
{
    return pa_wifiAp_SetDiscoverable( discoverable );
}


//--------------------------------------------------------------------------------------------------
/**
 * Set which Wifi Channel to use.
 * Default value is 1.
 * Some legal restrictions for values 12 - 14 might apply for your region.
 *
 * @return LE_FAULT         Function failed.
 * @return LE_BAD_PARAMETER Parameter is invalid.
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiAp_SetChannel
(
    int8_t channelNumber
        ///< [IN]
        ///< the channel number must be between 1 and 14.
)
{
    return LE_FAULT;
}


//--------------------------------------------------------------------------------------------------
/**
 *  Wifi Access Point COMPONENT Init
 */
//--------------------------------------------------------------------------------------------------
void le_wifiAp_Init
(
    void
)
{
    LE_DEBUG( "Wifi Access Point Service is ready" );

    // Create an event Id for new Wifi Events
    NewWifiApEventId = le_event_CreateId( "WiFiApEventId", sizeof( le_wifiAp_Event_t ) );

    // register for events from PA.
    pa_wifiAp_AddEventHandler( PaEventApHandler );

}

//--------------------------------------------------------------------------------------------------
/**
 * Set number of maximally allowed clients to connect to the Access Point at the same time.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_BAD_PARAMETER Parameter is invalid.
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiAp_SetMaxNumberOfClients
(
    int8_t maxNumberOfClient
        ///< [IN]
        ///< the maximum number of clients
)
{
    if( 0 == maxNumberOfClient )
    {
        return LE_BAD_PARAMETER;
    }

    //TODO: Add when exists in PA interface. return pa_wifiAp_SetMaxNumberOfClients( maxNumberOfClient);
    return LE_FAULT;
}
