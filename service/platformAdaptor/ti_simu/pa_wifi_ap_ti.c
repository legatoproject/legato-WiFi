#define SIMU 1
// -------------------------------------------------------------------------------------------------
/**
 *  PA for Wifi Access Point
 *
 *  Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 *
 */
// -------------------------------------------------------------------------------------------------
#include <sys/types.h>
#include <sys/wait.h>

#include "legato.h"

#include "interfaces.h"

#include "pa_wifi_ap.h"
#include "stdio.h"


static le_wifiAp_SecurityProtocol_t SavedSecurityProtocol;

static char SavedSsid[LE_WIFIDEFS_MAX_SSID_BYTES];

static bool SavedDiscoverable = true;
static int8_t SavedChannelNumber = 0;
static int SavedMaxNumClients = 10;

/** WPA-Personal */
static char SavedPassphrase[LE_WIFIDEFS_MAX_PASSPHRASE_BYTES] = "";
static char SavedPreSharedKey[LE_WIFIDEFS_MAX_PSK_BYTES] = "";



//--------------------------------------------------------------------------------------------------
/**
 * WifiAp state event ID used to report WifiAp state events to the registered event handlers.
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t WifiApPaEvent;

//--------------------------------------------------------------------------------------------------
/**
 * The first-layer Wifi Client Event Handler.
 *
 */
//--------------------------------------------------------------------------------------------------
static void FirstLayerWifiApEventHandler
(
    void* reportPtr,
    void* secondLayerHandlerFunc
)
{
    pa_wifiAp_NewEventHandlerFunc_t ApHandlerFunc = secondLayerHandlerFunc;

    le_wifiAp_Event_t  * wifiEvent = ( le_wifiAp_Event_t* ) reportPtr;

    if ( NULL != wifiEvent )
    {
        LE_INFO( "FirstLayerWifiApEventHandler event: %d", *wifiEvent );
        ApHandlerFunc( *wifiEvent, le_event_GetContextPtr() );
    }
    else
    {
        LE_ERROR( "FirstLayerWifiApEventHandler event is NULL" );
    }
}


#ifdef SIMU
// SIMU variable for timers
static le_timer_Ref_t SimuClientConnectTimer = NULL;
static le_timer_Ref_t SimuClientDisconnectTimer = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * This function generates a Wifi Access Point Event if callback event handler is set.
 */
//--------------------------------------------------------------------------------------------------
static void GenerateApEvent
(
    le_wifiAp_Event_t event
)
{
    LE_INFO(  "GenerateApEvent event: %d ", event );
    le_event_Report( WifiApPaEvent , (void*)&event, sizeof( le_wifiAp_Event_t ) );
}

//--------------------------------------------------------------------------------------------------
/**
 * SIMULATION! Simulates a LE_WIFIAP_EVENT_CLIENT_CONNECTED event
 */
//--------------------------------------------------------------------------------------------------
static void SimuClientConnectTimeCallBack
(
    le_timer_Ref_t timerRef
)
{
    GenerateApEvent( LE_WIFIAP_EVENT_CLIENT_CONNECTED );
}


static void SimuClientDisconnectTimeCallBack
(
    le_timer_Ref_t timerRef
)
{
    GenerateApEvent( LE_WIFIAP_EVENT_CLIENT_DISCONNECTED );
}

//--------------------------------------------------------------------------------------------------
/**
 * TODO: SIMULATION to remove/move
 * This function activates some timers to simulate client connect/disconnect events.
 */
//--------------------------------------------------------------------------------------------------

static void StartConnectSimulation
(
    int onceEvery,
    int repeat
)
{
    // once every "onceEvery" seconds...
    le_clk_Time_t interval = { onceEvery, 0 };
    SimuClientConnectTimer = le_timer_Create( "Wifi Simu AP Connect Timer" );

    LE_INFO( "StartApSimulation timer: %p", SimuClientConnectTimer );

    if ( LE_OK != le_timer_SetHandler ( SimuClientConnectTimer, SimuClientConnectTimeCallBack ))
    {
        LE_ERROR( "ERROR: StartApSimulation le_timer_SetHandler failed." );
    }
    if ( LE_OK != le_timer_SetInterval( SimuClientConnectTimer, interval ))
    {
        LE_ERROR( "ERROR: StartApSimulation le_timer_SetInterval failed." );
    }

    //repeat "repeat" times
    if ( LE_OK != le_timer_SetRepeat( SimuClientConnectTimer, repeat ) )
    {
        LE_ERROR( "ERROR: StartApSimulation le_timer_SetInterval failed." );
    }

    if( LE_OK != le_timer_Start( SimuClientConnectTimer ))
    {
        LE_ERROR( "ERROR: StartApSimulation le_timer_SetInterval failed." );
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * SIMULATION to remove/move
 * This function activates some timers to simulate client connect/disconnect events.
 */
//--------------------------------------------------------------------------------------------------

static void StartDisconnectSimulation
(
    int onceEvery,
    int repeat
)
{
    // once every "onceEvery" seconds...
    le_clk_Time_t interval = { onceEvery, 0 };
    SimuClientDisconnectTimer = le_timer_Create( "Wifi Simu AP DisConnect Timer" );

    LE_INFO( "StartDisconnectSimulation timer: %p", SimuClientDisconnectTimer );

    if ( LE_OK != le_timer_SetHandler ( SimuClientDisconnectTimer, SimuClientDisconnectTimeCallBack ))
    {
        LE_ERROR( "ERROR: StartDisconnectSimulation le_timer_SetHandler failed." );
    }
    if ( LE_OK != le_timer_SetInterval( SimuClientDisconnectTimer, interval ))
    {
        LE_ERROR( "ERROR: StartDisconnectSimulation le_timer_SetInterval failed." );
    }

    //repeat "repeat" times
    if ( LE_OK != le_timer_SetRepeat( SimuClientDisconnectTimer, repeat ) )
    {
        LE_ERROR( "ERROR: StartDisconnectSimulation le_timer_SetInterval failed." );
    }

    if( LE_OK != le_timer_Start( SimuClientDisconnectTimer ))
    {
        LE_ERROR( "ERROR: StartDisconnectSimulation le_timer_SetInterval failed." );
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * SIMULATION to remove/move
 * This function activates some timers to simulate client connect/disconnect events.
 */
//--------------------------------------------------------------------------------------------------

static void StartApSimulation
(
    void
)
{
    StartConnectSimulation(10, 10);
    StartDisconnectSimulation(14, 10);
}


#endif

//--------------------------------------------------------------------------------------------------
// Public declarations
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to initialize the PA WIFI Module.
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_Init
(
    void
)
{
    LE_INFO( "pa_wifiAp_Init() called" );
    // Create the event for signaling user handlers.
    WifiApPaEvent = le_event_CreateId( "WifiApPaEvent", sizeof( le_wifiAp_Event_t ) );

    if( NULL != le_event_CreateId)
    {
        return LE_OK;
    }
    else
    {
        return LE_ERROR;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to release the PA WIFI Module.
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_Release
(
    void
)
{
     LE_INFO( "pa_wifiAp_Release() called" );
     return LE_OK;
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
le_result_t pa_wifiAp_Start
(
    void
)
{
    le_result_t result = LE_OK;


    LE_INFO( "pa_wifiAp_Start() Simulated SSID: \"%s\"",
        (char*) SavedSsid );

    StartApSimulation();

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function stops the WIFI Access Point.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_DUPLICATE     If the WIFI device is already stopped.
 * @return LE_OK            The function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_Stop
(
    void
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for events connect/disconnect
 *
 * These events provide information on Wifi Access Point
 */
//--------------------------------------------------------------------------------------------------
le_result_t  pa_wifiAp_AddEventHandler
(
    pa_wifiAp_NewEventHandlerFunc_t handlerPtr,
        ///< [IN]

    void* contextPtr
        ///< [IN]
)
{
    le_event_HandlerRef_t handlerRef;
    handlerRef = le_event_AddLayeredHandler( "WifiApPaHandler",
                    WifiApPaEvent,
                    FirstLayerWifiApEventHandler,
                    (le_event_HandlerFunc_t)handlerPtr );
    if( NULL != handlerRef )
    {
        LE_INFO( "pa_wifiAp_AddEventHandler() ERROR: le_event_AddLayeredHandler returned NULL" );
        return LE_ERRROR;
    }

    le_event_SetContextPtr( handlerRef, contextPtr );
    return LE_OK;
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
le_result_t pa_wifiAp_SetSsid
(
    const uint8_t* ssidPtr,
        ///< [IN]
        ///< The SSID to set as a octet array.

    size_t ssidNumElements
        ///< [IN]
)
{
    // Store Passphrase to be used later during startup procedure
    le_result_t result = LE_BAD_PARAMETER;
    LE_INFO( "pa_wifiAp_SetSsid SSID length %d SSID: \"%.*s\"",
        (int) ssidNumElements,
        (int) ssidNumElements,
        (char*) ssidPtr );

    if ( 0 != ssidNumElements )
    {
       strncpy( &SavedSsid[0], (const char *) &ssidPtr[0], ssidNumElements );
       // Make sure there is a null termination
       SavedSsid[ssidNumElements] = '\0';
       result = LE_OK;
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the Security protocol to use.
 * Default value is SECURITY_WPA2.
 * @note that the SSID does not have to be human readable ASCII values, but often has.
 *
 * @return LE_BAD_PARAMETER Some parameter is invalid.
 * @return LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetSecurityProtocol
(
    le_wifiAp_SecurityProtocol_t securityProtocol
        ///< [IN]
        ///< The security protocol to use.
)
{
    le_result_t result;
    LE_INFO( "pa_wifiAp_SetSecurityProtocol: %d",securityProtocol );
    switch ( securityProtocol )
    {
        case LE_WIFIAP_SECURITY_NONE:
        case LE_WIFIAP_SECURITY_WPA2:
            SavedSecurityProtocol = securityProtocol;
            result = LE_OK;
            break;

        default:
            result = LE_BAD_PARAMETER;
            break;
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the passphrase used to generate the PSK.
 * Default value is "ChangeThisPassword".
 *
 * @note the difference between le_wifiAp_SetPreSharedKey() and this function
 *
 * @return LE_BAD_PARAMETER Parameter is invalid.
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetPassPhrase
(
    const char* passphrase
        ///< [IN]
        ///< pass-phrase for PSK
)
{
    // Store Passphrase to be used later during startup procedure
    le_result_t result = LE_BAD_PARAMETER;
    LE_INFO( "pa_wifiAp_SetPassPhrase" );
    if( NULL != passphrase )
    {
       strncpy( &SavedPassphrase[0], &passphrase[0], LE_WIFIDEFS_MAX_PASSPHRASE_LENGTH );
       // Make sure there is a null termination
       SavedPassphrase[LE_WIFIDEFS_MAX_PASSPHRASE_LENGTH] = '\0';
       result = LE_OK;
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the Pre Shared Key, PSK.
 * There is no default value, since le_wifiAp_SetPassPhrase is used as default.
 * @note the difference between le_wifiAp_SetPassPhrase() and this function
 *
 * @return LE_BAD_PARAMETER Parameter is invalid.
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetPreSharedKey
(
    const char* preSharedKey
        ///< [IN]
        ///< PSK. Note the difference between PSK and Pass Phrase.
)
{
    // Store PreSharedKey to be used later during startup procedure
    le_result_t result = LE_BAD_PARAMETER;
    LE_INFO( "pa_wifiAp_SetPreSharedKey" );
    if( NULL != preSharedKey )
    {
       strncpy( &SavedPreSharedKey[0], &preSharedKey[0], LE_WIFIDEFS_MAX_PSK_LENGTH );
       // Make sure there is a null termination
       SavedPreSharedKey[LE_WIFIDEFS_MAX_PSK_LENGTH] = '\0';
       result = LE_OK;
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set if the Access Point should announce it's presence.
 * Default value is TRUE.
 * If the value is set to FALSE, the Access Point will be hidden.
 *
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetDiscoverable
(
    bool discoverable
        ///< [IN]
        ///< If TRUE the Acces Point annonce will it's SSID, else it's hidden.
)
{
    // Store Discoverable to be used later during startup procedure
    LE_INFO( "pa_wifiAp_SetDiscoverable" );
    SavedDiscoverable = discoverable;
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set which Wifi Channel to use.
 * Default value is 1.
 * Some legal restrictions for values 12 - 14 might apply for your region.
 *
 * @return LE_BAD_PARAMETER Parameter is invalid.
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetChannel
(
    int8_t channelNumber
        ///< [IN]
        ///< the channel number must be between 1 and 14.
)
{
    // Store PreSharedKey to be used later during startup procedure
    le_result_t result = LE_BAD_PARAMETER;
    LE_INFO(  "pa_wifiAp_SetChannel" );
    if ((channelNumber >= 1) && (channelNumber <= 14))
    {
       SavedChannelNumber = channelNumber;
       result = LE_OK;
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the maximum number of clients allowed to be connected to WiFi Access Point.
 * Default value is 10.
 *
 * @return LE_FAULT         Function failed.
 * @return LE_BAD_PARAMETER Parameter is invalid.
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetMaxNumberClients
(
    int maxNumberClients
        ///< [IN]
        ///< the maximum number of clients (1-256).
)
{
    // Store maxNumberClients to be used later during startup procedure
    le_result_t result = LE_BAD_PARAMETER;
    LE_INFO( "pa_wifiAp_SetMaxNumberClients" );
    if ((maxNumberClients >= 1) && (maxNumberClients <= 256))
    {
       SavedMaxNumClients = maxNumberClients;
       result = LE_OK;
    }
    return result;
}
