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
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include "legato.h"

#include "interfaces.h"

#include "pa_wifi_ap.h"
#include "stdio.h"

#define WIFI_SCRIPT_PATH "/legato/systems/current/apps/wifiService/read-only/pa_wifi.sh "

#define DNSMASQ_CFG_FILE "/etc/dnsmasq.d/wifiAP.conf"

/** Command to init the hardware */
#define COMMAND_WIFI_HW_START "wlan0 WIFI_START"
#define COMMAND_WIFI_HW_STOP "wlan0 WIFI_STOP" /* not sure that this works */
#define COMMAND_WIFI_WLAN_UP "wlan0 WIFI_WLAN_UP"
#define COMMAND_WIFI_SET_EVENT "wlan0 WIFI_SET_EVENT"
#define COMMAND_WIFIAP_HOSTAPD "wlan0 WIFIAP_HOSTAPD"

/** Maximum numbers of WiFi connections for the TI chip */
#define TI_WIFI_MAX_USERS 10

static le_wifiAp_SecurityProtocol_t SavedSecurityProtocol;

static char SavedSsid[LE_WIFIDEFS_MAX_SSID_BYTES];

static bool SavedDiscoverable = true;
static int8_t SavedChannelNumber = 0;
static int SavedMaxNumClients = TI_WIFI_MAX_USERS;

/** WPA-Personal */
static char SavedPassphrase[LE_WIFIDEFS_MAX_PASSPHRASE_BYTES] = "";
static char SavedPreSharedKey[LE_WIFIDEFS_MAX_PSK_BYTES] = "";

static void* WifiApPaThreadMain(void* contextPtr);
static le_thread_Ref_t  WifiApPaThread = NULL;
static FILE *IwThreadFp = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * WifiAp state event ID used to report WifiAp state events to the registered event handlers.
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t WifiApPaEvent;

//--------------------------------------------------------------------------------------------------
/**
 * Thread destructor
 */
//--------------------------------------------------------------------------------------------------
static void ThreadDestructor
(
    void *context
)
{
    if ( IwThreadFp )
    {
        // And close FP used in created thread
        pclose( IwThreadFp );
        IwThreadFp = NULL;
    }
}

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

//--------------------------------------------------------------------------------------------------
/**
 * Wifi Access Point PA Thread
 *
 */
//--------------------------------------------------------------------------------------------------
static void* WifiApPaThreadMain
(
    void* contextPtr
)
{
    char tmpString[] = (WIFI_SCRIPT_PATH COMMAND_WIFI_SET_EVENT);
    char path[1024];

    LE_INFO( "WifiApPaThreadMain: Started!" );

    // Open the command "iw events" for reading.
    IwThreadFp = popen( tmpString, "r" );

    if ( NULL == IwThreadFp )
    {
        LE_ERROR( "WifiApPaThreadMain: Failed to run command:\"%s\" errno:%d %s",
                        (tmpString),
                        errno,
                        strerror( errno ) );
        return NULL;
    }

    // Read the output a line at a time - output it.
    while ( NULL != fgets(path, sizeof(path)-1, IwThreadFp) )
    {
        LE_INFO( "PARSING:%s: len:%d", path, (int) strnlen( path,sizeof(path)-1 ) );
        if ( NULL != strstr( path,"new station" ) )
        {
            LE_INFO( "FOUND new station" );
            // Report event: LE_WIFIAP_EVENT_CONNECTED
            le_wifiAp_Event_t event = LE_WIFIAP_EVENT_CLIENT_CONNECTED;
            LE_INFO(  "InternalWifiApStateEvent event: %d ", event );
            le_event_Report( WifiApPaEvent , (void*)&event, sizeof( le_wifiAp_Event_t ) );
        }
        else if ( NULL != strstr( path,"del station" ) )
        {
            LE_INFO( "FOUND del station" );
            // Report event: LE_WIFIAP_EVENT_DISCONNECTED
            le_wifiAp_Event_t event = LE_WIFIAP_EVENT_CLIENT_DISCONNECTED;
            LE_INFO( "InternalWifiApStateEvent event: %d ", event );
            le_event_Report( WifiApPaEvent , (void*)&event, sizeof( le_wifiAp_Event_t ) );
        }
    }
    // Run the event loop
    le_event_RunLoop();
    return NULL;
}

#ifdef SIMU
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

    //repeat 5 times
    if ( LE_OK != le_timer_SetRepeat( SimuClientConnectTimer, 5 ) )
    {
        LE_ERROR( "ERROR: StartApSimulation le_timer_SetInterval failed." );
    }

    if( LE_OK != le_timer_Start( SimuClientConnectTimer ))
    {
        LE_ERROR( "ERROR: StartApSimulation le_timer_SetInterval failed." );
    }
}
#endif

//--------------------------------------------------------------------------------------------------
// Public declarations
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to initialize the PA WIFI Module.
 *
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

    // ********************* Temporary fix **********************
    system("chmod 755 /legato/systems/current/apps/wifiService/read-only/pa_wifi.sh");
    // ********************* Temporary fix **********************

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to release the PA WIFI Module.
 *
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
 * @return LE_OK            The function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_Start
(
    void
)
{
    le_result_t result = LE_FAULT;
    char tmpString[256];
    int16_t systemResult;

    LE_INFO( "pa_wifiAp_Start()" );

    // Create WiFi AP PA Thread
    WifiApPaThread = le_thread_Create( "WifiApPaThread", WifiApPaThreadMain, NULL );
    le_thread_SetJoinable( WifiApPaThread );
    le_thread_AddDestructor ( ThreadDestructor, NULL );
    le_thread_Start( WifiApPaThread );

    // Check parameters before start
    if ( '\0' == SavedSsid[0] )
    {
        LE_ERROR( "pa_wifiAp_Start: No valid SSID" );
        return LE_FAULT;
    } else {
        LE_INFO( "pa_wifiAp_Start: SSID=%s", SavedSsid );
        result = LE_OK;
    }

    systemResult =  system( WIFI_SCRIPT_PATH COMMAND_WIFI_HW_START );
    /**
     * Return value of -1 means that the fork() has failed (see man system).
     * The script /etc/init.d/tiwifi returns 0 if the kernel modules are loaded correctly
     * and the wlan0 interface is seen,
     * 127 if modules not loaded or interface not seen,
     * 1 if the option passed if unknown (start stop and restart).
    */
    if ( 0 == WEXITSTATUS ( systemResult ) )
    {
        LE_INFO( "Wifi Access Point Command OK:" COMMAND_WIFI_HW_START );
        result = LE_OK;
    }
    else
    {
        LE_ERROR( "Wifi Access Point Command Failed: (%d)" COMMAND_WIFI_HW_START, systemResult );
        result = LE_FAULT;
    }
    if ( LE_OK == result )
    {
        // COMMAND_WIFI_WLAN_UP
        systemResult = system( WIFI_SCRIPT_PATH COMMAND_WIFI_WLAN_UP );
        if ( 0 == WEXITSTATUS( systemResult ) )
        {
            LE_INFO( "Wifi Access Point Command OK:" COMMAND_WIFI_WLAN_UP );
            result = LE_OK;
        }
        else
        {
            LE_ERROR( "Wifi Access Point Command Failed: (%d)" COMMAND_WIFI_WLAN_UP, systemResult );
            result = LE_FAULT;
        }
    }

    // Create /etc/hostapd.conf
    FILE * configFilePtr = fopen( "/tmp/hostapd.conf", "w" );
    fwrite( "interface=wlan0\n"\
            "driver=nl80211\n"\
            "hw_mode=g\n"\
            "beacon_int=100\n"\
            "dtim_period=2\n"\
            "rts_threshold=2347\n"\
            "fragm_threshold=2346\n"\
            "ctrl_interface=/var/run/hostapd\n"\
            "ctrl_interface_group=0\n",
            1,
     strlen("interface=wlan0\n"\
            "driver=nl80211\n"\
            "hw_mode=g\n"\
            "beacon_int=100\n"\
            "dtim_period=2\n"\
            "rts_threshold=2347\n"\
            "fragm_threshold=2346\n"\
            "ctrl_interface=/var/run/hostapd\n"\
            "ctrl_interface_group=0\n"),
     configFilePtr );
    snprintf( tmpString, sizeof(tmpString), "ssid=%s\nchannel=%d\nmax_num_sta=%d\n",
        (char *) SavedSsid,
        SavedChannelNumber,
        SavedMaxNumClients );
    fwrite( tmpString, 1, strlen(tmpString), configFilePtr );
    LE_INFO( "pa_wifiAp_Start hostapd.conf: %s", tmpString );
    // Security Protocol
    switch ( SavedSecurityProtocol )
    {
        case LE_WIFIAP_SECURITY_NONE:
            LE_INFO( "pa_wifiAp_Start LE_WIFIAP_SECURITY_NONE" );
            fwrite( "auth_algs=1\n"\
                    "eap_server=0\n"\
                    "eapol_key_index_workaround=0\n"\
                    "wmm_enabled=1\n"\
                    "macaddr_acl=0\n",
                1,
                strlen("auth_algs=1\n"\
                    "eap_server=0\n"\
                    "eapol_key_index_workaround=0\n"\
                    "wmm_enabled=1\n"\
                    "macaddr_acl=0\n"), configFilePtr );
            result = LE_OK;
            break;

        case LE_WIFIAP_SECURITY_WPA2:
            LE_INFO( "pa_wifiAp_Start LE_WIFIAP_SECURITY_WPA2" );
            fwrite( "wpa=2\n"\
                    "wpa_key_mgmt=WPA-PSK\n"\
                    "wpa_pairwise=CCMP\n"\
                    "rsn_pairwise=CCMP\n",
                1,
                strlen("wpa=2\n"\
                    "wpa_key_mgmt=WPA-PSK\n"\
                    "wpa_pairwise=CCMP\n"\
                    "rsn_pairwise=CCMP\n"), configFilePtr );
            if ( '\0' != SavedPassphrase[0] )
            {
                snprintf( tmpString, sizeof(tmpString), "wpa_passphrase=%s\n", SavedPassphrase );
                fwrite( tmpString, 1, strlen(tmpString), configFilePtr );
                LE_INFO( "pa_wifiAp_Start hostapd.conf: %s", tmpString );
                result = LE_OK;
            }
            else if ('\0' != SavedPreSharedKey[0] )
            {
                snprintf( tmpString, sizeof(tmpString), "wpa_psk=%s\n", SavedPreSharedKey );
                fwrite( tmpString, 1, strlen(tmpString), configFilePtr );
                LE_INFO( "pa_wifiAp_Start hostapd.conf: %s", tmpString );
                result = LE_OK;
            }
            else
            {
                LE_ERROR( "pa_wifiAp_Start Security Protocol is missing!" );
                result = LE_FAULT;
            }
            break;

        default:
            result = LE_FAULT;
            break;
    }
    if ( LE_OK == result )
    {
        if ( true == SavedDiscoverable )
        {
            // Annonce SSID
            snprintf( tmpString, sizeof(tmpString), "ignore_broadcast_ssid=0\n" );
            fwrite( tmpString, 1, strlen(tmpString), configFilePtr );
            LE_INFO( "pa_wifiAp_Start hostapd.conf: %s", tmpString );
        }
        else
        {
            // Do not annonce SSID
            snprintf( tmpString, sizeof(tmpString), "ignore_broadcast_ssid=1\n" );
            fwrite( tmpString, 1, strlen(tmpString), configFilePtr );
            LE_INFO( "pa_wifiAp_Start hostapd.conf: %s", tmpString );
        }
    }
    // Close file
    fclose( configFilePtr );
    LE_INFO( "Waiting 15s for WiFi driver availability ..." );
    sleep( 15 );   // Sleep 15s
    // Start Access Point cmd: /bin/hostapd /etc/hostapd.conf
    if ( LE_OK == result )
    {
        LE_INFO( "Start Access Point Cmd: /bin/hostapd /tmp/hostapd.conf" );
        systemResult = system( WIFI_SCRIPT_PATH COMMAND_WIFIAP_HOSTAPD );
        if ( 0 != WEXITSTATUS( systemResult ) )
        {
            LE_ERROR( "Wifi Client Command Failed: (%d)" COMMAND_WIFIAP_HOSTAPD, systemResult );
            result = LE_FAULT;
        }
        else
        {
            LE_INFO( "Wifi Access Point Command OK:" COMMAND_WIFIAP_HOSTAPD );
            result = LE_OK;
        }
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function stops the WIFI Access Point.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_OK            The function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_Stop
(
    void
)
{
    le_result_t result = LE_FAULT;
    int16_t systemResult =  system( WIFI_SCRIPT_PATH COMMAND_WIFI_HW_STOP );
    /**
     * Return value of -1 means that the fork() has failed (see man system).
     * The script /etc/init.d/tiwifi returns 0 if the kernel modules are loaded correctly
     * and the wlan0 interface is seen,
     * 127 if modules not loaded or interface not seen,
     * 1 if the option passed if unknown (start stop and restart).
    */
    if ( 0 == WEXITSTATUS( systemResult ) )
    {
        LE_INFO( "Wifi Access Point Command OK:" COMMAND_WIFI_HW_STOP );
        result = LE_OK;
    }
    else
    {
        LE_ERROR( "Wifi Access Point Command Failed: (%d)" COMMAND_WIFI_HW_STOP, systemResult );
        result = LE_FAULT;
    }

    if ( LE_OK == result )
    {
        // Must terminate created thread
        le_thread_Cancel( WifiApPaThread );
        if ( LE_OK == le_thread_Join( WifiApPaThread,NULL ) )
        {
            result = LE_OK;
        }
        else
        {
            result = LE_FAULT;
        }
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for events connect/disconnect
 *
 * These events provide information on Wifi Access Point
 *
 * @return LE_OK Function succeeded.
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

    le_event_SetContextPtr( handlerRef, contextPtr );
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the Service set identification (SSID) of the AccessPoint
 * Default value is "LEGATO Access Point"
 * @note that the SSID does not have to be human readable ASCII values, but often has.
 *
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
 * @return LE_OUT_OF_RANGE Requested channel number is out of range.
 * @return LE_OK           Function succeeded.
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
    le_result_t result = LE_OUT_OF_RANGE;
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
 * @return LE_OUT_OF_RANGE  Requested number of users exceeds the capabilities of the Access Point.
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetMaxNumberClients
(
    int maxNumberClients
        ///< [IN]
        ///< the maximum number of clients regarding the WiFi driver capabilities.
)
{
    // Store maxNumberClients to be used later during startup procedure
    le_result_t result = LE_OUT_OF_RANGE;
    LE_INFO( "pa_wifiAp_SetMaxNumberClients" );
    if ((maxNumberClients >= 1) && (maxNumberClients <= TI_WIFI_MAX_USERS))
    {
       SavedMaxNumClients = maxNumberClients;
       result = LE_OK;
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Defines the IP adresses range for the host AP.
 *
 * @return LE_BAD_PARAMETER At least, one of the given IP addresses is invalid.
 * @return LE_FAULT         A system call has failed.
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetIpRange
(
    const char *ipAp,
        ///< [IN]
        ///< the IP address of the Access Point.
    const char *ipStart,
        ///< [IN]
        ///< the start IP address of the Access Point.
    const char *ipStop
        ///< [IN]
        ///< the stop IP address of the Access Point.
)
{
    struct sockaddr_in saAp;
    struct sockaddr_in saStart;
    struct sockaddr_in saStop;
    const char *parameter = 0;

    // Check the parameters
    if ((ipAp == NULL) || (ipStart == NULL) || (ipStop == NULL))
        return LE_BAD_PARAMETER;

    if ((!strlen(ipAp)) || (!strlen(ipStart)) || (!strlen(ipStop)))
        return LE_BAD_PARAMETER;

    if (inet_pton(AF_INET, ipAp, &(saAp.sin_addr)) <= 0)
    {
        parameter = "AP";
    }
    else if (inet_pton(AF_INET, ipStart, &(saStart.sin_addr)) <= 0)
    {
        parameter = "start";
    }
    else if (inet_pton(AF_INET, ipStop, &(saStop.sin_addr)) <= 0)
    {
        parameter = "stop";
    }

    {
        unsigned int ap = ntohl( saAp.sin_addr.s_addr );
        unsigned int start = ntohl( saStart.sin_addr.s_addr );
        unsigned int stop = ntohl( saStop.sin_addr.s_addr );

        LE_INFO(">>>>>>>>>>>>>>  pa_wifiAp_SetIpRange: @AP=%x, @APstart=%x, @APstop=%x",
                ap, start, stop);
    }

    if (parameter != 0)
    {
        LE_ERROR("pa_wifiAp_SetIpRange: Invalid %s IP address", parameter);
        return LE_BAD_PARAMETER;
    }
    else
    {
        unsigned int ap = ntohl( saAp.sin_addr.s_addr );
        unsigned int start = ntohl( saStart.sin_addr.s_addr );
        unsigned int stop = ntohl( saStop.sin_addr.s_addr );

        LE_INFO("pa_wifiAp_SetIpRange: @AP=%x, @APstart=%x, @APstop=%x",
                ap, start, stop);

        if (start > stop)
        {
            LE_INFO("pa_wifiAp_SetIpRange: Need to swap start & stop IP addresses.");
            start = start ^ stop;
            stop = stop ^ start;
            start = start ^ stop;
        }

        if ((ap >= start) && (ap <= stop))
        {
            LE_ERROR("pa_wifiAp_SetIpRange: AP IP address is within the range.");
            return LE_BAD_PARAMETER;
        }
    }

    putenv("PATH=/legato/systems/current/bin:/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin");

    {
        char cmd[256];

        snprintf((char *)&cmd, sizeof(cmd), "%s %s %s %s", "/sbin/ifconfig", "wlan0", ipAp, "up");

        if (system(cmd) < 0)
        {
            LE_ERROR("pa_wifiAp_SetIpRange: Unable to mount the network interface.");
            return LE_FAULT;
        }
        else
        {
            FILE * fp;
            int16_t systemResult;

            LE_INFO("pa_wifiAp_SetIpRange: Creation of dnsmasq configuration file (%s)", DNSMASQ_CFG_FILE);

            fp = fopen (DNSMASQ_CFG_FILE, "w");
            if (fp != NULL)
            {
                fprintf(fp, "dhcp-range=%s,%s,%s,%dh\n", "wlan0", ipStart, ipStop, 24);
                fclose(fp);
            }
            else
            {
                LE_ERROR("pa_wifiAp_SetIpRange: Unable to create the dnsmasq configuration file.");
                return LE_FAULT;
            }

            LE_INFO("pa_wifiAp_SetIpRange: @AP=%s, @APstart=%s, @APstop=%s",
                    ipAp, ipStart, ipStop);

            systemResult = system("/etc/init.d/dnsmasq restart");
            if ( 0 != WEXITSTATUS ( systemResult ) )
            {
                LE_ERROR("pa_wifiAp_SetIpRange: Unable to restart the DHCP server.");
                return LE_FAULT;
            }
        }
    }

    return LE_OK;
}
