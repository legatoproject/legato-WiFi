//TODO: place this file in a good subdirectory.
// -------------------------------------------------------------------------------------------------
/**
 *  PA for Wifi Access Point
 *
 *  Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 *
 */
// -------------------------------------------------------------------------------------------------
#include "legato.h"

#include "interfaces.h"

#include "pa_wifi_ap.h"
#include "stdio.h"
// string to find trace in logtrace
#define WIFIDEBUG "PA WIFI AP DEBUG:"


static pa_wifiAp_NewEventHandlerFunc_t CallBackHandlerFuncPtr = NULL;


// SIMU variable for timers
static le_timer_Ref_t SimuClientConnectTimer = NULL;
//static le_timer_Ref_t SimuClientDisconnectTimer = NULL;


//--------------------------------------------------------------------------------------------------
/**
 * This function generates a Wifi Access Point Event if callback event handler is set.
 */
//--------------------------------------------------------------------------------------------------
static void GenerateApEvent
(
    le_wifiAp_Event_t wifiApEvent
)
{
    if( NULL != CallBackHandlerFuncPtr )
    {
        CallBackHandlerFuncPtr( wifiApEvent );
    }
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

//--------------------------------------------------------------------------------------------------
/**
 * TODO: SIMULATION to remove/move
 * This function activates some timers to simulate client connect/disconnect events.
 */
//--------------------------------------------------------------------------------------------------

static void StartApSimulation( void )
{
    // once every 10 seconds...
    le_clk_Time_t interval = { 10, 0 };
    SimuClientConnectTimer = le_timer_Create( "Wifi Simu AP Connect Timer");

    LE_INFO( WIFIDEBUG "StartApSimulation timer: %p", SimuClientConnectTimer );

    if( LE_OK != le_timer_SetHandler ( SimuClientConnectTimer, SimuClientConnectTimeCallBack ))
    {
        LE_ERROR( WIFIDEBUG "ERROR: StartApSimulation le_timer_SetHandler failed.");
    }
    if( LE_OK != le_timer_SetInterval( SimuClientConnectTimer, interval ))
    {
        LE_ERROR( WIFIDEBUG "ERROR: StartApSimulation le_timer_SetInterval failed.");
    }

        //repeat 5 times
    if( LE_OK != le_timer_SetRepeat( SimuClientConnectTimer, 5))
    {
        LE_ERROR( WIFIDEBUG "ERROR: StartApSimulation le_timer_SetInterval failed.");
    }

    if( LE_OK != le_timer_Start( SimuClientConnectTimer ))
    {
        LE_ERROR( WIFIDEBUG "ERROR: StartApSimulation le_timer_SetInterval failed.");
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to initialize the PA WIFI Module.
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiAp_Init
(
    void
)
{
    // TODO: code me.
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to release the PA WIFI Module.
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiAp_Release
(
    void
)
{
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
    // start simulation
    StartApSimulation();// todo: real code.
    return LE_OK;
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
LE_SHARED le_result_t  pa_wifiAp_AddEventHandler
(
    pa_wifiAp_NewEventHandlerFunc_t handlerPtr
        ///< [IN]
)
{
    CallBackHandlerFuncPtr = handlerPtr;
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
    return LE_OK;    //todo: save it
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
    return LE_OK;    //todo: save it
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
le_result_t pa_wifiAp_SetPassPhrase
(
    const char* passPhrase
        ///< [IN]
        ///< pass-phrase for PSK
)
{
    return LE_OK;    //todo: save it
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
le_result_t pa_wifiAp_SetPreSharedKey
(
    const char* preSharedKey
        ///< [IN]
        ///< PSK. Note the difference between PSK and Pass Phrase.
)

{
    return LE_OK;    //todo: save it
}


//--------------------------------------------------------------------------------------------------
/**
 * Set if the Access Point should announce it's presence.
 * Default value is TRUE.
 * If the value is set to FALSE, the Access Point will be hidden.
 *
 * @return LE_FAULT         Function failed.
 * @return LE_BAD_PARAMETER Parameter is invalid.
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
    return LE_OK;    //todo: save it
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
le_result_t pa_wifiAp_SetChannel
(
    int8_t channelNumber
        ///< [IN]
        ///< the channel number must be between 1 and 14.
)
{
    return LE_OK;    //todo: save it
}
