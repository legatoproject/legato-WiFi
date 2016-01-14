// -------------------------------------------------------------------------------------------------
/**
 *  Wifi Client Platform Adapter
 *
 *  Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 *
 */
// -------------------------------------------------------------------------------------------------
#include <sys/types.h>
#include <sys/wait.h>

#include "legato.h"

#include "interfaces.h"

#include "pa_wifi.h"
#include "stdio.h"

/** Command to init the hardware */
#define COMMAND_WIFI_HW_START "/opt/legato/apps/wifiService/usr/local/bin/pa_wifi.sh wlan0 WIFI_START"
#define COMMAND_WIFI_HW_STOP "/opt/legato/apps/wifiService/usr/local/bin/pa_wifi.sh wlan0 WIFI_STOP" /* not sure that this works */
#define COMMAND_WIFI_WLAN_UP "/opt/legato/apps/wifiService/usr/local/bin/pa_wifi.sh wlan0 WIFI_WLAN_UP"
#define COMMAND_WIFI_SET_EVENT "/opt/legato/apps/wifiService/usr/local/bin/pa_wifi.sh wlan0 WIFI_SET_EVENT"
#define COMMAND_WIFICLIENT_START_SCAN "/opt/legato/apps/wifiService/usr/local/bin/pa_wifi.sh wlan0 WIFICLIENT_START_SCAN"
#define COMMAND_WIFICLIENT_DISCONNECT "/opt/legato/apps/wifiService/usr/local/bin/pa_wifi.sh wlan0 WIFICLIENT_DISCONNECT"
#define COMMAND_WIFICLIENT_CONNECT_SECURITY_NONE "/opt/legato/apps/wifiService/usr/local/bin/pa_wifi.sh wlan0 WIFICLIENT_CONNECT_SECURITY_NONE \"%.*s\""
#define COMMAND_WIFICLIENT_CONNECT_SECURITY_WEP "/opt/legato/apps/wifiService/usr/local/bin/pa_wifi.sh wlan0 WIFICLIENT_CONNECT_SECURITY_WEP \"%.*s\" \"%s\""
#define COMMAND_WIFICLIENT_CONNECT_SECURITY_WPA_PSK_PERSONAL "/opt/legato/apps/wifiService/usr/local/bin/pa_wifi.sh wlan0 WIFICLIENT_CONNECT_SECURITY_WPA_PSK_PERSONAL \"%.*s\" \"%s\""
#define COMMAND_WIFICLIENT_CONNECT_SECURITY_WPA2_PSK_PERSONAL "/opt/legato/apps/wifiService/usr/local/bin/pa_wifi.sh wlan0 WIFICLIENT_CONNECT_SECURITY_WPA2_PSK_PERSONAL \"%.*s\" \"%s\""
#define COMMAND_WIFICLIENT_CONNECT_SECURITY_WPA_EAP_PEAP0_ENTERPRISE "/opt/legato/apps/wifiService/usr/local/bin/pa_wifi.sh wlan0 WIFICLIENT_CONNECT_SECURITY_WPA_EAP_PEAP0_ENTERPRISE \"%.*s\" \"%s\" \"%s\""
#define COMMAND_WIFICLIENT_CONNECT_SECURITY_WPA2_EAP_PEAP0_ENTERPRISE "/opt/legato/apps/wifiService/usr/local/bin/pa_wifi.sh wlan0 WIFICLIENT_CONNECT_SECURITY_WPA2_EAP_PEAP0_ENTERPRISE \"%.*s\" \"%s\" \"%s\""
#define COMMAND_WIFICLIENT_CONNECT_WPA_PASSPHRASE "/opt/legato/apps/wifiService/usr/local/bin/pa_wifi.sh wlan0 WIFICLIENT_CONNECT_WPA_PASSPHRASE \"%.*s\" %s"

static le_wifiClient_SecurityProtocol_t SavedSecurityProtocol;
/**  WEP */
static char SavedWepKey[LE_WIFIDEFS_MAX_WEPKEY_BYTES];
/** WPA-Personal */
static char SavedPassphrase[LE_WIFIDEFS_MAX_PASSPHRASE_BYTES];
static char SavedPreSharedKey[LE_WIFIDEFS_MAX_PSK_BYTES];
/** WPA-Entreprise */
static char SavedUsername[LE_WIFIDEFS_MAX_USERNAME_BYTES];
static char SavedPassword[LE_WIFIDEFS_MAX_PASSWORD_BYTES];


static FILE *IwScanFp = NULL;
static FILE *IwConnectFp = NULL;
static FILE *IwThreadFp = NULL;
static bool IsScanRunning = false;

static void* WifiClientPaThreadMain(void* contextPtr);
static le_thread_Ref_t  WifiClientPaThread = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * WifiClient state event ID used to report WifiClient state events to the registered event handlers.
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t WifiClientPaEvent;

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
    pa_wifiClient_NewEventHandlerFunc_t clientHandlerFunc = secondLayerHandlerFunc;

    le_wifiClient_Event_t  * wifiEvent = ( le_wifiClient_Event_t* ) reportPtr;

    if ( NULL != wifiEvent )
    {
        LE_INFO( "FirstLayerWifiClientEventHandler event: %d", *wifiEvent );
        clientHandlerFunc( *wifiEvent, le_event_GetContextPtr() );
    }
    else
    {
        LE_ERROR( "FirstLayerWifiClientEventHandler event is NULL" );
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Wifi Client PA Thread
 *
 */
//--------------------------------------------------------------------------------------------------
static void* WifiClientPaThreadMain
(
    void* contextPtr
)
{
    char tmpString[] = COMMAND_WIFI_SET_EVENT;
    char path[1024];

    LE_INFO( "WifiClientPaThreadMain: Started!" );

    // Open the command "iw events" for reading.
    IwThreadFp = popen( tmpString, "r" );

    if ( NULL == IwThreadFp )
    {
        LE_ERROR( "WifiClientPaThreadMain: Failed to run command:\"%s\" errno:%d %s",
                        (tmpString),
                        errno,
                        strerror( errno ));
        return NULL;
    }

    // Read the output a line at a time - output it.
    while ( NULL != fgets( path, sizeof(path)-1, IwThreadFp ) )
    {
        LE_INFO( "PARSING:%s: len:%d", path, strnlen( path,sizeof(path)-1 ) );
        if ( NULL != strstr( path,"connected to" ) )
        {
            LE_INFO( "FOUND connected" );
            // Report event: LE_WIFICLIENT_EVENT_CONNECTED
            le_wifiClient_Event_t event = LE_WIFICLIENT_EVENT_CONNECTED;
            LE_INFO( "InternalWifiClientStateEvent event: %d ", event );
            le_event_Report( WifiClientPaEvent , (void*)&event, sizeof( le_wifiClient_Event_t ) );
        }
        else if ( NULL != strstr(path,"disconnected") )
        {
            LE_INFO( "FOUND disconnected" );
            // Report event: LE_WIFICLIENT_EVENT_DISCONNECTED
            le_wifiClient_Event_t event = LE_WIFICLIENT_EVENT_DISCONNECTED;
            LE_INFO( "InternalWifiClientStateEvent event: %d ", event );
            le_event_Report( WifiClientPaEvent , (void*)&event, sizeof( le_wifiClient_Event_t ) );
        }
    }
    // Run the event loop
    le_event_RunLoop();
    return NULL;
}

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
le_result_t pa_wifiClient_Init
(
    void
)
{
    LE_INFO( "pa_wifiClient_Init() called" );
    // Create the event for signaling user handlers.
    WifiClientPaEvent = le_event_CreateId( "WifiClientPaEvent", sizeof( le_wifiClient_Event_t ) );

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to release the PA WIFI Module.
 *
 * @return LE_OK     The function succeed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_Release
(
    void
)
{
     LE_INFO( "pa_wifiClient_Release() called" );
     return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Start Wifi Client PA
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.*/
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_Start
(
    void
)
{
    le_result_t result = LE_FAULT;

    LE_INFO( "pa_wifiClient_Start()" );

    // Create WiFi Client PA Thread
    WifiClientPaThread = le_thread_Create( "WifiClientPaThread", WifiClientPaThreadMain, NULL );
    le_thread_SetJoinable( WifiClientPaThread );
    le_thread_Start( WifiClientPaThread);

    int16_t systemResult =  system( COMMAND_WIFI_HW_START );
    /**
     * Return value of -1 means that the fork() has failed (see man system).
     * The script /etc/init.d/tiwifi returns 0 if the kernel modules are loaded correctly
     * and the wlan0 interface is seen,
     * 127 if modules not loaded or interface not seen,
     * 1 if the option passed if unknown (start stop and restart).
    */
    if ( 0 == WEXITSTATUS( systemResult ) )
    {
        LE_INFO( "Wifi Client Command OK:" COMMAND_WIFI_HW_START );
        result = LE_OK;
    }
    else
    {
        LE_ERROR( "Wifi Client Command Failed: (%d)" COMMAND_WIFI_HW_START, systemResult );
        result = LE_FAULT;
    }
    if ( LE_OK == result )
    {
        systemResult = system( COMMAND_WIFI_WLAN_UP );
        if ( 0 == WEXITSTATUS( systemResult ) )
        {
            LE_INFO( "Wifi Client Command OK:" COMMAND_WIFI_WLAN_UP );
            result = LE_OK;
        }
        else
        {
            LE_ERROR( "Wifi Client Command Failed: (%d)" COMMAND_WIFI_WLAN_UP, systemResult );
            result = LE_FAULT;
        }
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Stop Wifi Client PA
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.*/
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_Stop
(
    void
)
{
    le_result_t result = LE_FAULT;
    int16_t systemResult =  system( COMMAND_WIFI_HW_STOP );
    /**
     * Return value of -1 means that the fork() has failed (see man system).
     * The script /etc/init.d/tiwifi returns 0 if the kernel modules are loaded correctly
     * and the wlan0 interface is seen,
     * 127 if modules not loaded or interface not seen,
     * 1 if the option passed if unknown (start stop and restart).
    */
    if ( 0 == WEXITSTATUS( systemResult ) )
    {
        LE_INFO( "Wifi Client Command OK:" COMMAND_WIFI_HW_STOP );
        result = LE_OK;
    }
    else
    {
        LE_ERROR( "Wifi Client Command Failed: (%d)" COMMAND_WIFI_HW_STOP, systemResult );
        result = LE_FAULT;
    }

    if ( LE_OK == result )
    {
        // Must terminate created thread
        le_thread_Cancel( WifiClientPaThread );
        if ( LE_OK == le_thread_Join( WifiClientPaThread,NULL ) )
        {
            // And close FP used in created thread
            pclose( IwThreadFp );
            result = LE_OK;
        }
        else
        {
            result= LE_FAULT;
        }
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function will start a scan and returns when it is done.
 * It should NOT return until the scan is done and results can be read with GetScanResult
 * Results are read via pa_wifiClient_GetScanResult.
 * When the reading is done pa_wifiClient_ScanDone MUST be called.*
 *
 * @return LE_FAULT  The function failed.
 * @return LE_BUSY   The function is already ongoing.
 * @return LE_OK     The function succeed.*/
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_Scan
(
    void
)
{
    le_result_t result = LE_OK;
    LE_INFO( "pa_wifiClient_Scan" );
    if( IsScanRunning )
    {
        LE_ERROR( "pa_wifi_Scan: Scan is already running" );
        return LE_BUSY;
    }

    if( NULL != IwScanFp )
    {
        return LE_BUSY;
    }

    IsScanRunning = true;
    /* Open the command for reading. */
    IwScanFp = popen( COMMAND_WIFICLIENT_START_SCAN, "r" );

    if ( NULL == IwScanFp )
    {
        LE_ERROR( "pa_wifi_Scan: Failed to run command: errno:%d: \"%s\" Cmd:"
                                COMMAND_WIFICLIENT_START_SCAN,
                                errno,
                                strerror( errno )
                                );
        result = LE_FAULT;
    }

    IsScanRunning = false;
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function to find out if scan is currently running
 * @return TRUE  Scan is running.
 * @return FALSE Scan is not running
 */
//--------------------------------------------------------------------------------------------------
bool pa_wifiClient_IsScanRunning
(
)
{
    return IsScanRunning;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function can be called after pa_wifi_Scan.
 * When the reading is done, that it no longer returns LE_OK, pa_wifiClient_ScanDone MUST be called.
 * @return LE_NOT_FOUND  There is no more AP:s found.
 * @return LE_OK     The function succeed.
 * @return LE_FAULT  The function failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_GetScanResult
(
    pa_wifiClient_AccessPoint_t * accessPointPtr
    ///< [IN][OUT]
    ///< Structure provided by calling function.
    ///< Results filled out if resullt was LE_OK
)
{
    char path[1024];

    LE_INFO( "pa_wifi_GetScanResult" );

    if ( NULL == IwScanFp )
    {
       LE_ERROR( "le_wifiClient_Scan: ERROR must call pa_wifi_Scan first" );
       return LE_FAULT;
    }
    if ( NULL == accessPointPtr )
    {
       LE_ERROR( "le_wifiClient_Scan: ERROR : accessPoint == NULL" );
       return LE_BAD_PARAMETER;
    }

    /*default values*/
    accessPointPtr->signalStrength = 0xffff;
    accessPointPtr->ssidLength = 0;

    /* Read the output a line at a time - output it. */
    while ( NULL != fgets( path, sizeof( path )-1, IwScanFp) )
    {

        LE_INFO( "PARSING:%s: len:%zd", path, strnlen( path, sizeof( path )-1) );

        if( 0 == strncmp( "\tSSID: ", path, 7 ) )
        {
            accessPointPtr->ssidLength = strnlen( path, LE_WIFIDEFS_MAX_SSID_BYTES ) - 7 -1;
            LE_INFO( "FOUND SSID:%s  %c%c.. ", path, path[7], path[8] );
            memset( &accessPointPtr->ssidBytes, 0, LE_WIFIDEFS_MAX_SSID_BYTES );
            memcpy ( &accessPointPtr->ssidBytes, &path[7], accessPointPtr->ssidLength );
            LE_INFO( "FOUND SSID: Parsed:\"%s\"", &accessPointPtr->ssidBytes[0] );
            return LE_OK;
        }
        else if( 0 == strncmp( "\tsignal: ", path, 9 ))
        {
            LE_INFO( "FOUND SIGNAL STRENGTH:%s  %c %c ", path, path[10], path[11] );
            accessPointPtr->signalStrength = atoi( &path[9] );
            LE_INFO( "FOUND SIGNAL STRENGTH: signalStrength:%d ",
            accessPointPtr->signalStrength );
        }
    }

    return LE_NOT_FOUND;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called after the pa_wifiClient_Scan() has been done.
 * It signals that the scan results are no longer needed and frees some internal resourses.
 *
 * @return LE_OK     The function succeed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_ScanDone
(
    void
)
{
    if( NULL != IwScanFp )
    {
        pclose( IwScanFp );
        IwScanFp = NULL;
        IsScanRunning = false;
    }
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the security protocol for connection
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.*/
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_SetSecurityProtocol
(
    const le_wifiClient_SecurityProtocol_t securityProtocol
)
{
    le_result_t result = LE_OK;
    LE_INFO( "pa_wifiClient_SetSecurityProtocol: %d",securityProtocol );
    switch ( securityProtocol )
    {
        case LE_WIFICLIENT_SECURITY_NONE:
        case LE_WIFICLIENT_SECURITY_WEP:
        case LE_WIFICLIENT_SECURITY_WPA_PSK_PERSONAL:
        case LE_WIFICLIENT_SECURITY_WPA2_PSK_PERSONAL:
        case LE_WIFICLIENT_SECURITY_WPA_EAP_PEAP0_ENTERPRISE:
        case LE_WIFICLIENT_SECURITY_WPA2_EAP_PEAP0_ENTERPRISE:
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
 * This function connects a wifiClient.
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.*/
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_Connect
(
    uint8_t ssidBytes[LE_WIFIDEFS_MAX_SSID_BYTES], ///< Contains ssidLength number of bytes
    uint8_t ssidLength ///< The number of Bytes in the ssidBytes
)
{
    int16_t systemResult;
    char tmpString[1024], path[4*1024];
    le_result_t result = LE_OK;

    LE_INFO( "pa_wifiClient_Connect SSID length %d SSID: \"%.*s\"",
        ssidLength,
        ssidLength,
        (char*) ssidBytes );

    if ( 0 == ssidLength )
    {
        LE_ERROR( "pa_wifi_Connect: No valid SSID" );
        return LE_FAULT;
    }

    // Which type of Wifi client connection is requested?
    switch (SavedSecurityProtocol)
    {
        case LE_WIFICLIENT_SECURITY_NONE:
            // Opened Wifi hotspot (SSID with no password)
            LE_INFO( "pa_wifiClient_Connect SwitchCase: LE_WIFICLIENT_SECURITY_NONE" );
            // 1. Now WPA_CLI for LE_WIFICLIENT_SECURITY_NONE
            LE_INFO( "pa_wifiClient_Connect Step 2: SH script" );
            snprintf( tmpString,
                sizeof( tmpString ),
                COMMAND_WIFICLIENT_CONNECT_SECURITY_NONE,
                ssidLength,
                (char*) ssidBytes );

            systemResult = system( tmpString );
            // Return value of -1 means that the fork() has failed (see man system).
            if ( 0 == WEXITSTATUS( systemResult ) )
            {
                LE_INFO( "Wifi Client Command OK: %s", tmpString );
                result = LE_OK;
            }
            else
            {
                LE_ERROR( "Wifi Client Command %s Failed: (%d)", tmpString, systemResult );
                result = LE_FAULT;
            }
            break;

        case LE_WIFICLIENT_SECURITY_WEP:
            // Connect to secured network - WEP
            LE_INFO( "pa_wifiClient_Connect SwitchCase: LE_WIFICLIENT_SECURITY_WEP" );
            if ( 0 == SavedWepKey[0] )
            {
                LE_ERROR( "pa_wifi_Connect: No valid WEP key" );
                return LE_FAULT;
            }
            // 1. WPA_CLI for LE_WIFICLIENT_SECURITY_WEP
            if ( LE_OK == result )
            {
                LE_INFO( "pa_wifiClient_Connect Step 2: SH script" );
                snprintf( tmpString,
                    sizeof( tmpString ),
                    COMMAND_WIFICLIENT_CONNECT_SECURITY_WEP,
                    ssidLength,
                    (char*) ssidBytes,
                    SavedWepKey );

                systemResult = system( tmpString );
                // Return value of -1 means that the fork() has failed (see man system).
                if ( 0 == WEXITSTATUS( systemResult ) )
                {
                    LE_INFO("Wifi Client Command OK: %s", tmpString);
                    result = LE_OK;
                }
                else
                {
                    LE_ERROR( "Wifi Client Command %s Failed: (%d)", tmpString, systemResult );
                    result = LE_FAULT;
                }
            }
            break;

        case LE_WIFICLIENT_SECURITY_WPA_PSK_PERSONAL:
            // Connect to secured network - WPA
            LE_INFO( "pa_wifiClient_Connect SwitchCase: LE_WIFICLIENT_SECURITY_WPA_PSK_PERSONAL" );
            if ( (0 == SavedPassphrase[0]) && (0 == SavedPreSharedKey[0]) )
            {
                LE_ERROR( "pa_wifi_Connect: No valid PassPhrase & PreSharedKey" );
                return LE_FAULT;
            }
            if ( 0 != SavedPassphrase[0] )
            {
                // 1. Passphrase/PSK. With PassPhrase generate PSK and read it for next command
                LE_INFO( "pa_wifiClient_Connect Step 1: Generate Passphrase/PSK" );
                snprintf(tmpString,
                    sizeof( tmpString ),
                    COMMAND_WIFICLIENT_CONNECT_WPA_PASSPHRASE,
                    ssidLength,
                    (char*) ssidBytes,
                    SavedPassphrase);

                LE_INFO( "pa_wifiClient_Connect Cmd: %s", tmpString );

                // Open the command for reading.
                IwConnectFp = popen( tmpString, "r" );

                if ( NULL == IwConnectFp )
                {
                    LE_ERROR( "pa_wifi_Connect: Failed to run command:\"%s\" errno:%d %s",
                                    (tmpString),
                                    errno,
                                    strerror(errno) );
                    result = LE_FAULT;
                } else {
                    LE_INFO( "pa_wifiClient_Connect Cmd successful: %s", tmpString );
                    // Read the output a line at a time - output it.
                    while ( NULL != fgets( path, sizeof( path )-1, IwConnectFp ) )
                    {
                        LE_INFO( "PARSING:%s: len:%d", path, strnlen( path,sizeof( path )-1 ) );
                        if ( 0 == strncmp( "\tpsk=", path, strlen( "\tpsk=" ) ) )
                        {
                            LE_INFO( "FOUND :%s", path );
                            memset( SavedPreSharedKey, 0, LE_WIFIDEFS_MAX_PSK_BYTES );
                            char *p1 = strchr(&path[6], '\n');
                            char *p2 = &path[6];
                            int n = p1 - p2;
                            if ( n < LE_WIFIDEFS_MAX_PSK_BYTES )
                            {
                                strncpy( SavedPreSharedKey, &path[6], n );
                                LE_INFO( "PSK=%s length: %d",SavedPreSharedKey, n );
                                result = LE_OK;
                            }
                            else
                            {
                                result = LE_FAULT;
                            }
                        }
                    }
                }
                pclose( IwConnectFp );
            }
            // 2. Now WPA_CLI for LE_WIFICLIENT_SECURITY_WPA_PSK_PERSONAL
            if ( LE_OK == result )
            {
                LE_INFO( "pa_wifiClient_Connect Step 2: SH script" );
                snprintf(tmpString,
                    sizeof( tmpString ),
                    COMMAND_WIFICLIENT_CONNECT_SECURITY_WPA_PSK_PERSONAL,
                    ssidLength,
                    (char*) ssidBytes,
                    SavedPreSharedKey);

                systemResult = system( tmpString );
                // Return value of -1 means that the fork() has failed (see man system).
                if ( 0 == WEXITSTATUS( systemResult ) )
                {
                    LE_INFO( "Wifi Client Command OK: %s", tmpString );
                    result = LE_OK;
                }
                else
                {
                    LE_ERROR( "Wifi Client Command %s Failed: (%d)", tmpString, systemResult );
                    result = LE_FAULT;
                }
            }
            break;

        case LE_WIFICLIENT_SECURITY_WPA2_PSK_PERSONAL:
            // Connect to secured network - WPA2
            LE_INFO( "pa_wifiClient_Connect SwitchCase: LE_WIFICLIENT_SECURITY_WPA2_PSK_PERSONAL" );
            if ( (0 == SavedPassphrase[0]) && (0 == SavedPreSharedKey[0]) )
            {
                LE_ERROR( "pa_wifi_Connect: No valid PassPhrase & PreSharedKey" );
                return LE_FAULT;
            }
            if ( 0 != SavedPassphrase[0] )
            {
                // 1. Passphrase/PSK. With PassPhrase generate PSK and read it for next command
                LE_INFO( "pa_wifiClient_Connect Step 1: Generate Passphrase/PSK" );
                snprintf( tmpString,
                    sizeof( tmpString) ,
                    COMMAND_WIFICLIENT_CONNECT_WPA_PASSPHRASE,
                    ssidLength,
                    (char*) ssidBytes,
                    SavedPassphrase );

                LE_INFO( "pa_wifiClient_Connect Cmd: %s", tmpString );

                // Open the command for reading.
                IwConnectFp = popen( tmpString, "r" );

                if ( NULL == IwConnectFp )
                {
                    LE_ERROR( "pa_wifi_Connect: Failed to run command:\"%s\" errno:%d %s",
                                    (tmpString),
                                    errno,
                                    strerror( errno ) );
                    result = LE_FAULT;
                } else {
                    LE_INFO( "pa_wifiClient_Connect Cmd successful: %s", tmpString );
                    // Read the output a line at a time - output it.
                    while ( NULL != fgets(path, sizeof( path )-1, IwConnectFp) )
                    {
                        LE_INFO( "PARSING:%s: len:%d", path, strnlen( path,sizeof( path )-1) );
                        if ( 0 == strncmp( "\tpsk=", path, strlen( "\tpsk=" ) ) )
                        {
                            LE_INFO( "FOUND :%s", path );
                            memset( SavedPreSharedKey, 0, LE_WIFIDEFS_MAX_PSK_BYTES );
                            char *p1 = strchr(&path[6], '\n');
                            char *p2 = &path[6];
                            int n = p1 - p2;
                            if ( n < LE_WIFIDEFS_MAX_PSK_BYTES )
                            {
                                strncpy( SavedPreSharedKey, &path[6], n );
                                LE_INFO( "PSK=%s length: %d",SavedPreSharedKey, n );
                                result = LE_OK;
                            }
                            else
                            {
                                result = LE_FAULT;
                            }
                        }
                    }
                }
                pclose( IwConnectFp );
            }
            // 2. Now WPA_CLI for LE_WIFICLIENT_SECURITY_WPA2_PSK_PERSONAL
            if ( LE_OK == result )
            {
                LE_INFO( "pa_wifiClient_Connect Step 2: SH script" );
                snprintf( tmpString,
                    sizeof( tmpString ),
                    COMMAND_WIFICLIENT_CONNECT_SECURITY_WPA2_PSK_PERSONAL,
                    ssidLength,
                    (char*) ssidBytes,
                    SavedPreSharedKey );

                systemResult = system( tmpString );
                // Return value of -1 means that the fork() has failed (see man system).
                if ( 0 == WEXITSTATUS( systemResult ) )
                {
                    LE_INFO( "Wifi Client Command OK: %s", tmpString );
                    result = LE_OK;
                }
                else
                {
                    LE_ERROR( "Wifi Client Command %s Failed: (%d)", tmpString, systemResult );
                    result = LE_FAULT;
                }
            }
            break;

        case LE_WIFICLIENT_SECURITY_WPA_EAP_PEAP0_ENTERPRISE:
            // Enterprise environment: connect to WPA EAP PEAP0
            LE_INFO( "pa_wifiClient_Connect SwitchCase: ..._SECURITY_WPA2_EAP_PEAP0_ENTERPRISE" );
            if ( (0 == SavedUsername[0]) && (0 == SavedPassword[0]) )
            {
                LE_ERROR( "pa_wifi_Connect: No valid Username & Password" );
                return LE_FAULT;
            }
            // 2. Now WPA_CLI for LE_WIFICLIENT_SECURITY_WPA_EAP_PEAP0_ENTERPRISE
            if ( LE_OK == result )
            {
                LE_INFO( "pa_wifiClient_Connect Step 2: SH script" );
                snprintf( tmpString,
                    sizeof( tmpString ),
                    COMMAND_WIFICLIENT_CONNECT_SECURITY_WPA_EAP_PEAP0_ENTERPRISE,
                    ssidLength,
                    (char*) ssidBytes,
                    SavedUsername,
                    SavedPassword );

                systemResult = system( tmpString );
                // Return value of -1 means that the fork() has failed (see man system).
                if ( 0 == WEXITSTATUS( systemResult ) )
                {
                    LE_INFO( "Wifi Client Command OK: %s", tmpString );
                    result = LE_OK;
                }
                else
                {
                    LE_ERROR( "Wifi Client Command %s Failed: (%d)", tmpString, systemResult );
                    result = LE_FAULT;
                }
            }
            break;

        case LE_WIFICLIENT_SECURITY_WPA2_EAP_PEAP0_ENTERPRISE:
            // Enterprise environment: connect to WPA2 EAP PEAP0
            LE_INFO( "pa_wifiClient_Connect SwitchCase: ..._SECURITY_WPA2_EAP_PEAP0_ENTERPRISE" );
            if ( (0 == SavedUsername[0]) && (0 == SavedPassword[0]) )
            {
                LE_ERROR( "pa_wifi_Connect: No valid Username & Password" );
                return LE_FAULT;
            }
            // 2. Now WPA_CLI for LE_WIFICLIENT_SECURITY_WPA2_EAP_PEAP0_ENTERPRISE
            if ( LE_OK == result )
            {
                LE_INFO( "pa_wifiClient_Connect Step 2: SH script" );
                snprintf( tmpString,
                    sizeof( tmpString ),
                    COMMAND_WIFICLIENT_CONNECT_SECURITY_WPA2_EAP_PEAP0_ENTERPRISE,
                    ssidLength,
                    (char*) ssidBytes,
                    SavedUsername,
                    SavedPassword );

                systemResult = system( tmpString );
                // Return value of -1 means that the fork() has failed (see man system).
                if ( 0 == WEXITSTATUS(systemResult) )
                {
                    LE_INFO( "Wifi Client Command OK: %s", tmpString );
                    result = LE_OK;
                }
                else
                {
                    LE_ERROR( "Wifi Client Command %s Failed: (%d)", tmpString, systemResult );
                    result = LE_FAULT;
                }
            }
            break;

        default:
            result = LE_FAULT;
            break;
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function disconnects a wifiClient.
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.*/
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_Disconnect
(
    void
)
{
    int16_t systemResult;
    le_result_t result = LE_OK;

    // Terminate connection
    systemResult = system( COMMAND_WIFICLIENT_DISCONNECT );
    if ( 0 == WEXITSTATUS(systemResult) )
    {
        LE_INFO( "Wifi Client Command OK:" COMMAND_WIFICLIENT_DISCONNECT );
        result = LE_OK;
    }
    else
    {
        LE_ERROR( "Wifi Client Command Failed: (%d)" COMMAND_WIFICLIENT_DISCONNECT, systemResult );
        result = LE_FAULT;
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Clears all username, password, PreShared Key, passphrase settings previously made by
 * @see  pa_wifiClient_SetPassPhrase
 * @see  pa_wifiClient_SetPreSharedKey
 * @see  pa_wifiClient_SetUserCredentials
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_ClearAllCredentials
(
    void
)
{
    memset( SavedWepKey,'\0',LE_WIFIDEFS_MAX_WEPKEY_BYTES );
    memset( SavedPassphrase,'\0',LE_WIFIDEFS_MAX_PASSPHRASE_BYTES );
    memset( SavedPreSharedKey,'\0',LE_WIFIDEFS_MAX_PSK_BYTES );
    memset( SavedUsername,'\0',LE_WIFIDEFS_MAX_USERNAME_BYTES );
    memset( SavedPassword,'\0',LE_WIFIDEFS_MAX_PASSWORD_BYTES );
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the WEP key (WEP)
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.*/
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_SetWepKey
(
    const char* wepKey
)
{
    le_result_t result = LE_BAD_PARAMETER;
    LE_INFO( "pa_wifiClient_SetWepKey" );
    if( NULL != wepKey )
    {
       strncpy( &SavedWepKey[0], &wepKey[0], LE_WIFIDEFS_MAX_WEPKEY_LENGTH );
       // Make sure there is a null termination
       SavedWepKey[LE_WIFIDEFS_MAX_WEPKEY_LENGTH] = '\0';
       result = LE_OK;
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the PreSharedKey (WPA-Personal)
 * @see  pa_wifiClient_SetPassPhrase
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.*/
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_SetPreSharedKey
(
    const char* preSharedKey
)
{
    le_result_t result = LE_BAD_PARAMETER;
    LE_INFO( "pa_wifiClient_SetPreSharedKey" );
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
 * Set the PassPhrase used to create PSK (WPA-Personal).
 * @see  pa_wifiClient_SetPreSharedKey
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.*/
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_SetPassphrase
(
    const char* passphrase
)
{
    // Store Passphrase to be used later during connection procedure
    le_result_t result = LE_BAD_PARAMETER;
    LE_INFO( "pa_wifiClient_SetPassphrase" );
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
 * Set the username and password (WPA-Entreprise).
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.*/
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_SetUserCredentials
(
    const char* username,
    const char* password
)
{
    // Store User Credentials to be used later during connection procedure
    le_result_t result = LE_BAD_PARAMETER;
    LE_INFO( "pa_wifiClient_SetUserCredentials" );
    if( NULL != username )
    {
       strncpy( &SavedUsername[0], &username[0], LE_WIFIDEFS_MAX_USERNAME_LENGTH );
       // Make sure there is a null termination
       SavedUsername[LE_WIFIDEFS_MAX_USERNAME_LENGTH] = '\0';
       result = LE_OK;
    }
    else
    {
        return LE_BAD_PARAMETER;
    }
    if( NULL != password )
    {
       strncpy( &SavedPassword[0], &password[0], LE_WIFIDEFS_MAX_PASSWORD_LENGTH );
       // Make sure there is a null termination
       SavedPassword[LE_WIFIDEFS_MAX_PASSWORD_LENGTH] = '\0';
       result = LE_OK;
    }
    else
    {
        return LE_BAD_PARAMETER;
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for PA EVENT 'le_wifiClient_Event_t'
 *
 * This event provide information on PA Wifi Client event changes.
 */
//--------------------------------------------------------------------------------------------------
le_result_t  pa_wifiClient_AddEventHandler
(
    pa_wifiClient_NewEventHandlerFunc_t handlerPtr,
        ///< [IN]

    void* contextPtr
        ///< [IN]
)
{
    le_event_HandlerRef_t handlerRef;
    handlerRef = le_event_AddLayeredHandler( "WifiClientPaHandler",
                    WifiClientPaEvent,
                    FirstLayerWifiClientEventHandler,
                    (le_event_HandlerFunc_t)handlerPtr );

    le_event_SetContextPtr( handlerRef, contextPtr );
    return LE_OK;
}




