// -------------------------------------------------------------------------------------------------
/**
 *  PA for WiFi Access Point
 *
 *  Copyright (C) Sierra Wireless Inc.
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

// Set of commands to drive the WiFi features.
#define COMMAND_WIFI_HW_START        "wlan0 WIFI_START"
#define COMMAND_WIFI_HW_STOP         "wlan0 WIFI_STOP"
#define COMMAND_WIFI_WLAN_UP         "wlan0 WIFI_WLAN_UP"
#define COMMAND_WIFI_SET_EVENT       "wlan0 WIFI_SET_EVENT"
#define COMMAND_WIFIAP_HOSTAPD_START "wlan0 WIFIAP_HOSTAPD_START"
#define COMMAND_WIFIAP_HOSTAPD_STOP  "wlan0 WIFIAP_HOSTAPD_STOP"

// iptables rule to allow/disallow the DHCP port on wlan0
#define COMMAND_IPTABLE_DHCP_WLAN0   "INPUT -i wlan0 -p udp -m udp " \
                                     "--sport 67:68 --dport 67:68 -j ACCEPT"

//--------------------------------------------------------------------------------------------------
/**
 * WiFi platform adaptor shell script
 */
//--------------------------------------------------------------------------------------------------
#define WIFI_SCRIPT_PATH "/legato/systems/current/apps/wifiService/read-only/pa_wifi.sh "

//--------------------------------------------------------------------------------------------------
/**
 * Link to the dnsmasq related configuration file
 */
//--------------------------------------------------------------------------------------------------
#define DNSMASQ_CFG_LINK "/etc/dnsmasq.d/dnsmasq.wlan.conf"
//--------------------------------------------------------------------------------------------------
/**
 * dnsmasq related configuration file
 */
//--------------------------------------------------------------------------------------------------
#define DNSMASQ_CFG_FILE "/tmp/dnsmasq.wlan.conf"

//--------------------------------------------------------------------------------------------------
/**
 * WiFi access point configuration file
 */
//--------------------------------------------------------------------------------------------------
#define WIFI_HOSTAPD_FILE "/tmp/hostapd.conf"

// WiFi access point configuration.
//--------------------------------------------------------------------------------------------------
/**
 * Host access point global configuration.
 */
//--------------------------------------------------------------------------------------------------
#define WIFI_AP_CONFIG_HOSTAPD \
    "interface=wlan0\n"\
    "driver=nl80211\n"\
    "hw_mode=g\n"\
    "beacon_int=100\n"\
    "dtim_period=2\n"\
    "rts_threshold=2347\n"\
    "fragm_threshold=2346\n"\
    "ctrl_interface=/var/run/hostapd\n"\
    "ctrl_interface_group=0\n"
//--------------------------------------------------------------------------------------------------
/**
 * Host access point configuration with security disabled.
 */
//--------------------------------------------------------------------------------------------------
#define WIFI_AP_CONFIG_SECURITY_NONE \
    "auth_algs=1\n"\
    "eap_server=0\n"\
    "eapol_key_index_workaround=0\n"\
    "wmm_enabled=1\n"\
    "macaddr_acl=0\n"
//--------------------------------------------------------------------------------------------------
/**
 * Host access point configuration with security enabled.
 */
//--------------------------------------------------------------------------------------------------
#define WIFI_AP_CONFIG_SECURITY_WPA2 \
    "wpa=2\n"\
    "wpa_key_mgmt=WPA-PSK\n"\
    "wpa_pairwise=CCMP\n"\
    "rsn_pairwise=CCMP\n"

//--------------------------------------------------------------------------------------------------
/**
 * Maximum numbers of WiFi connections for the TI chip
 */
//--------------------------------------------------------------------------------------------------
#define TI_WIFI_MAX_USERS 10

//--------------------------------------------------------------------------------------------------
/**
 * The current security protocol
 */
//--------------------------------------------------------------------------------------------------
static le_wifiAp_SecurityProtocol_t SavedSecurityProtocol;
//--------------------------------------------------------------------------------------------------
/**
 * The current SSID
 */
//--------------------------------------------------------------------------------------------------
static char                         SavedSsid[LE_WIFIDEFS_MAX_SSID_BYTES] = "";
//--------------------------------------------------------------------------------------------------
/**
 * Defines whether the SSID is hidden or not
 */
//--------------------------------------------------------------------------------------------------
static bool                         SavedDiscoverable                     = true;
//--------------------------------------------------------------------------------------------------
/**
 * The WiFi channel associated with the SSID
 */
//--------------------------------------------------------------------------------------------------
static uint32_t                     SavedChannelNumber                    = 1;
//--------------------------------------------------------------------------------------------------
/**
 * The maximum numbers of clients the AP is able to manage
 */
//--------------------------------------------------------------------------------------------------
static uint32_t                     SavedMaxNumClients                    = TI_WIFI_MAX_USERS;

// WPA-Personal
//--------------------------------------------------------------------------------------------------
/**
 * Passphrase used for authentication. Used only with WPA/WPA2 protocol.
 */
//--------------------------------------------------------------------------------------------------
static char SavedPassphrase[LE_WIFIDEFS_MAX_PASSPHRASE_BYTES] = "";
//--------------------------------------------------------------------------------------------------
/**
 * Pre-Shared-Key used for authentication. Used only with WPA/WPA2 protocol.
 */
//--------------------------------------------------------------------------------------------------
static char SavedPreSharedKey[LE_WIFIDEFS_MAX_PSK_BYTES]      = "";

//--------------------------------------------------------------------------------------------------
/**
 * The main thread running the WiFi service
 */
//--------------------------------------------------------------------------------------------------
static void            *WifiApPaThreadMain(void *contextPtr);
//--------------------------------------------------------------------------------------------------
/**
 * The handle of the WiFi service thread
 */
//--------------------------------------------------------------------------------------------------
static le_thread_Ref_t  WifiApPaThread  = NULL;
//--------------------------------------------------------------------------------------------------
/**
 * The handle of the input pipe used to be notified of the WiFi related events.
 */
//--------------------------------------------------------------------------------------------------
static FILE            *IwThreadPipePtr = NULL;

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
    void *contextPtr
)
{
    if (IwThreadPipePtr)
    {
        // And close the communication pipe used in created thread.
        pclose(IwThreadPipePtr);
        IwThreadPipePtr = NULL;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * The first-layer WiFi Client Event Handler.
 *
 */
//--------------------------------------------------------------------------------------------------
static void FirstLayerWifiApEventHandler
(
    void *reportPtr,
    void *secondLayerHandlerFunc
)
{
    pa_wifiAp_NewEventHandlerFunc_t  ApHandlerFunc = secondLayerHandlerFunc;
    le_wifiAp_Event_t               *wifiEventPtr  = (le_wifiAp_Event_t *)reportPtr;

    if (NULL != wifiEventPtr)
    {
        LE_INFO("Event: %d", *wifiEventPtr);
        ApHandlerFunc(*wifiEventPtr, le_event_GetContextPtr());
    }
    else
    {
        LE_ERROR("Event is NULL");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * WiFi access point platform adaptor thread
 *
 */
//--------------------------------------------------------------------------------------------------
static void *WifiApPaThreadMain
(
    void *contextPtr
)
{
    char tmpString[] = (WIFI_SCRIPT_PATH COMMAND_WIFI_SET_EVENT);
    char path[1024];

    LE_INFO("Started!");

    // Open the command "iw events" for reading.
    IwThreadPipePtr = popen(tmpString, "r");

    if (NULL == IwThreadPipePtr)
    {
        LE_ERROR("Failed to run command:\"%s\" errno:%d %s",
            (tmpString),
            errno,
            strerror(errno));
        return NULL;
    }

    // Read the output a line at a time - output it.
    while (NULL != fgets(path, sizeof(path)-1, IwThreadPipePtr))
    {
        LE_INFO("PARSING:%s: len:%d", path, (int) strnlen(path, sizeof(path)-1));
        if (NULL != strstr(path, "new station"))
        {
            LE_INFO("FOUND new station");
            // Report event: LE_WIFIAP_EVENT_CONNECTED
            le_wifiAp_Event_t event = LE_WIFIAP_EVENT_CLIENT_CONNECTED;
            LE_INFO( "InternalWifiApStateEvent event: %d ", event);
            le_event_Report(WifiApPaEvent , (void *)&event, sizeof(le_wifiAp_Event_t));
        }
        else if (NULL != strstr(path, "del station"))
        {
            LE_INFO("FOUND del station");
            // Report event: LE_WIFIAP_EVENT_DISCONNECTED
            le_wifiAp_Event_t event = LE_WIFIAP_EVENT_CLIENT_DISCONNECTED;
            LE_INFO("InternalWifiApStateEvent event: %d ", event);
            le_event_Report(WifiApPaEvent , (void *)&event, sizeof(le_wifiAp_Event_t));
        }
    }
    // Run the event loop
    le_event_RunLoop();
    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function handles AP configuration file related operations.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t WriteApCfgFile
(
    const char *dataPtr,
    FILE *filePtr
)
{
    size_t length;

    if ((NULL == filePtr) || (NULL == dataPtr))
    {
        LE_ERROR("Invalid parameter(s)");
        return LE_FAULT;
    }

    length = strlen(dataPtr);
    if (length > 0)
    {
        if (fwrite(dataPtr, 1, length, filePtr) != length)
        {
            LE_ERROR("Unable to generate the hostapd file.");
            return LE_FAULT;
        }
    }

    return LE_OK;
}

#ifdef SIMU
// SIMU variable for timers
static le_timer_Ref_t SimuClientConnectTimer = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * This function generates a WiFi Access Point Event if callback event handler is set.
 */
//--------------------------------------------------------------------------------------------------
static void GenerateApEvent
(
    le_wifiAp_Event_t wifiApEvent
)
{
    if (NULL != CallBackHandlerFuncPtr)
    {
        CallBackHandlerFuncPtr(wifiApEvent);
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
    GenerateApEvent(LE_WIFIAP_EVENT_CLIENT_CONNECTED);
}

//--------------------------------------------------------------------------------------------------
/**
 * TODO: SIMULATION to remove/move
 * This function activates some timers to simulate client connect/disconnect events.
 */
//--------------------------------------------------------------------------------------------------

static void StartApSimulation(void)
{
    // once every 10 seconds...
    le_clk_Time_t interval = { 10, 0 };
    SimuClientConnectTimer = le_timer_Create("WiFi Simu AP Connect Timer");

    LE_INFO("Timer: %p", SimuClientConnectTimer);

    if (LE_OK != le_timer_SetHandler(SimuClientConnectTimer, SimuClientConnectTimeCallBack))
    {
        LE_ERROR("ERROR: StartApSimulation le_timer_SetHandler failed.");
    }
    if (LE_OK != le_timer_SetInterval(SimuClientConnectTimer, interval))
    {
        LE_ERROR("ERROR: StartApSimulation le_timer_SetInterval failed.");
    }

    //repeat 5 times
    if (LE_OK != le_timer_SetRepeat(SimuClientConnectTimer, 5))
    {
        LE_ERROR("ERROR: StartApSimulation le_timer_SetInterval failed.");
    }
    if (LE_OK != le_timer_Start(SimuClientConnectTimer))
    {
        LE_ERROR("ERROR: StartApSimulation le_timer_SetInterval failed.");
    }
}
#endif

//--------------------------------------------------------------------------------------------------
// Public declarations
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to initialize the WiFi module platform adapter (PA).
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_Init
(
    void
)
{
    le_result_t result       = LE_FAULT;
    int         systemResult;

    LE_INFO("pa_wifiAp_Init() called");
    // Create the event for signaling user handlers.
    WifiApPaEvent = le_event_CreateId("WifiApPaEvent", sizeof(le_wifiAp_Event_t));

    // TODO: Temporary fix
    systemResult = system("chmod 755 " WIFI_SCRIPT_PATH);

    if (0 == WEXITSTATUS (systemResult))
    {
        LE_INFO("WiFi Access Point Command OK: chmod");
        result = LE_OK;
    }
    else
    {
        LE_ERROR("WiFi Access Point Command Failed: chmod (%d)", systemResult);
        result = LE_FAULT;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to release the WiFi module platform adapter (PA).
 *
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_Release
(
    void
)
{
    LE_INFO("AP release called");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function starts the WiFi access point.
 * Note that all settings, if to be used, such as security, username, password must set prior to
 * starting the access point.
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
    le_result_t  result         = LE_FAULT;
    char         tmpString[256];
    int          systemResult;
    FILE        *configFilePtr  = NULL;

    LE_INFO("AP starts");

    // Create WiFi AP PA Thread
    WifiApPaThread = le_thread_Create("WifiApPaThread", WifiApPaThreadMain, NULL);
    le_thread_SetJoinable(WifiApPaThread);
    le_thread_AddDestructor(ThreadDestructor, NULL);
    le_thread_Start(WifiApPaThread);

    // Check parameters before start
    if ('\0' == SavedSsid[0])
    {
        LE_ERROR("No valid SSID");
        return LE_FAULT;
    } else {
        LE_INFO("SSID = %s", SavedSsid);
        result = LE_OK;
    }

    if (LE_OK == result)
    {
        systemResult = system(WIFI_SCRIPT_PATH COMMAND_WIFI_HW_START);
        /**
         * Return value of -1 means that the fork() has failed (see man system).
         * The script /etc/init.d/tiwifi returns 0 if the kernel modules are loaded correctly
         * and the wlan0 interface is seen,
         * 127 if modules not loaded or interface not seen,
         * 1 if the option passed if unknown (start stop and restart).
        */
        if (0 == WEXITSTATUS (systemResult))
        {
            LE_INFO("WiFi Access Point Command OK:" COMMAND_WIFI_HW_START);
            result = LE_OK;
        }
        else
        {
            LE_ERROR("WiFi Access Point Command Failed: (%d)" COMMAND_WIFI_HW_START, systemResult);
            result = LE_FAULT;
        }
    }

    if (LE_OK == result)
    {
        // COMMAND_WIFI_WLAN_UP
        systemResult = system(WIFI_SCRIPT_PATH COMMAND_WIFI_WLAN_UP);
        if (0 == WEXITSTATUS(systemResult))
        {
            LE_INFO("WiFi Access Point Command OK:" COMMAND_WIFI_WLAN_UP);
            result = LE_OK;
        }
        else
        {
            LE_ERROR("WiFi Access Point Command Failed: (%d)" COMMAND_WIFI_WLAN_UP, systemResult);
            result = LE_FAULT;
        }
    }

    if (LE_OK == result)
    {
        // Create /etc/hostapd.conf
        configFilePtr = fopen(WIFI_HOSTAPD_FILE, "w");
        if (configFilePtr == NULL)
        {
            LE_ERROR("WiFi Access Point Command Failed: Unable to create hostapd.conf file.");
            result = LE_FAULT;
        }
        else
        {
            result = WriteApCfgFile(WIFI_AP_CONFIG_HOSTAPD, configFilePtr);
        }

        if (result == LE_OK)
        {
            snprintf(tmpString, sizeof(tmpString), "ssid=%s\nchannel=%d\nmax_num_sta=%d\n",
                (char *)SavedSsid,
                SavedChannelNumber,
                SavedMaxNumClients);
            result = WriteApCfgFile(tmpString, configFilePtr);
        }

        if (result == LE_OK)
        {
            LE_INFO("hostapd.conf: %s", tmpString);
            // Security Protocol
            switch (SavedSecurityProtocol)
            {
                case LE_WIFIAP_SECURITY_NONE:
                    LE_INFO("LE_WIFIAP_SECURITY_NONE");
                    result = WriteApCfgFile(WIFI_AP_CONFIG_SECURITY_NONE, configFilePtr);
                    break;

                case LE_WIFIAP_SECURITY_WPA2:
                    LE_INFO("LE_WIFIAP_SECURITY_WPA2");
                    result = WriteApCfgFile(WIFI_AP_CONFIG_SECURITY_WPA2, configFilePtr);
                    if ('\0' != SavedPassphrase[0])
                    {
                        snprintf(tmpString, sizeof(tmpString), "wpa_passphrase=%s\n", SavedPassphrase);
                        LE_INFO("hostapd.conf: %s", tmpString);
                        result = WriteApCfgFile(tmpString, configFilePtr);
                    }
                    else if ('\0' != SavedPreSharedKey[0])
                    {
                        snprintf(tmpString, sizeof(tmpString), "wpa_psk=%s\n", SavedPreSharedKey);
                        LE_INFO("hostapd.conf: %s", tmpString);
                        result = WriteApCfgFile(tmpString, configFilePtr);
                    }
                    else
                    {
                        LE_ERROR("Security protocol is missing!");
                        result = LE_FAULT;
                    }
                    break;

                default:
                    result = LE_FAULT;
                    break;
            }
        }
    }

    if (LE_OK == result)
    {
        if (true == SavedDiscoverable)
        {
            // Announce SSID
            snprintf(tmpString, sizeof(tmpString), "ignore_broadcast_ssid=0\n");
            LE_INFO("hostapd.conf: %s", tmpString);
            result = WriteApCfgFile(tmpString, configFilePtr);
        }
        else
        {
            // Do not announce SSID
            snprintf(tmpString, sizeof(tmpString), "ignore_broadcast_ssid=1\n");
            LE_INFO("hostapd.conf: %s", tmpString);
            result = WriteApCfgFile(tmpString, configFilePtr);
        }
    }

    // Close file
    if (configFilePtr)
        fclose(configFilePtr);

    if (LE_OK == result)
    {
        LE_INFO("Waiting 15s for WiFi driver availability ...");
        sleep(15);   // Sleep 15s
        // Start Access Point cmd: /bin/hostapd /etc/hostapd.conf
        if (LE_OK == result)
        {
            LE_INFO("Start Access Point Cmd: /bin/hostapd /tmp/hostapd.conf");
            systemResult = system(WIFI_SCRIPT_PATH COMMAND_WIFIAP_HOSTAPD_START);
            if (0 != WEXITSTATUS(systemResult))
            {
                LE_ERROR("WiFi Client Command Failed: (%d)" COMMAND_WIFIAP_HOSTAPD_START, systemResult);
                result = LE_FAULT;
            }
            else
            {
                LE_INFO("WiFi Access Point Command OK:" COMMAND_WIFIAP_HOSTAPD_START);
                result = LE_OK;
            }
        }
    }
    else
    {
        // Config file is incomplete, so it is removed.
        LE_ERROR("WiFi Client Command Failed: As a result, configuration file (%s) is removed.", WIFI_HOSTAPD_FILE);
        remove(WIFI_HOSTAPD_FILE);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function stops the WiFi access point.
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
    le_result_t result       = LE_FAULT;
    int         systemResult;

    LE_INFO("Stop Access Point Cmd: kill hostap");

    // Try to delete the rule allowing the DHCP ports on wlan0. Ignore if it fails
    systemResult = system("iptables -D " COMMAND_IPTABLE_DHCP_WLAN0);
    if (0 != WEXITSTATUS(systemResult))
    {
        LE_WARN("Deleting rule for DHCP port fails");
    }

    systemResult = system(WIFI_SCRIPT_PATH COMMAND_WIFIAP_HOSTAPD_STOP);
    if (0 != WEXITSTATUS(systemResult))
    {
        LE_ERROR("WiFi Client Command Failed: (%d)" COMMAND_WIFIAP_HOSTAPD_STOP, systemResult);
        result = LE_FAULT;
    }
    else
    {
        LE_INFO("WiFi Access Point Command OK:" COMMAND_WIFIAP_HOSTAPD_STOP);
        result = LE_OK;
    }

    systemResult = system(WIFI_SCRIPT_PATH COMMAND_WIFI_HW_STOP);
    /**
     * Return value of -1 means that the fork() has failed (see man system).
     * The script /etc/init.d/tiwifi returns 0 if the kernel modules are loaded correctly
     * WIFI_SCRIPT_PATH "/legato/systems/current/apps/wifiService/read-only/pa_wifi.sh "
     * and the wlan0 interface is seen,
     * 127 if modules not loaded or interface not seen,
     * 1 if the option passed if unknown (start stop and restart).
    */
    if (0 == WEXITSTATUS(systemResult))
    {
        LE_INFO("WiFi Access Point Command OK:" COMMAND_WIFI_HW_STOP);
        result = LE_OK;
    }
    else
    {
        LE_ERROR("WiFi Access Point Command Failed: (%d)" COMMAND_WIFI_HW_STOP, systemResult);
        result = LE_FAULT;
    }

    if (LE_OK == result)
    {
        // Must terminate created thread
        le_thread_Cancel(WifiApPaThread);
        if (LE_OK == le_thread_Join(WifiApPaThread, NULL))
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
 * Add handler function for WiFi related events.
 *
 * These events provide information on WiFi access point status.
 *
 * @return LE_BAD_PARAMETER Some parameter is invalid.
 * @return LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t  pa_wifiAp_AddEventHandler
(
    pa_wifiAp_NewEventHandlerFunc_t handlerPtr,
        ///< [IN]
        ///< Event handler function pointer

    void *contextPtr
        ///< [IN]
        ///< Associated event context
)
{
    le_event_HandlerRef_t handlerRef;

    handlerRef = le_event_AddLayeredHandler("WifiApPaHandler",
        WifiApPaEvent,
        FirstLayerWifiApEventHandler,
        (le_event_HandlerFunc_t)handlerPtr);
    if (NULL == handlerRef)
    {
        LE_ERROR("ERROR: le_event_AddLayeredHandler returned NULL");
        return LE_BAD_PARAMETER;
    }

    le_event_SetContextPtr(handlerRef, contextPtr);
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the Service Set IDentification (SSID) of the access point
 *
 * @note The SSID does not have to be human readable ASCII values, but often is.
 *
 * @return LE_BAD_PARAMETER Some parameter is invalid.
 * @return LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetSsid
(
    const uint8_t *ssidPtr,
        ///< [IN]
        ///< The SSID to set as an octet array.

    size_t ssidNumElements
        ///< [IN]
        ///< The length of the SSID in bytes.
)
{
    le_result_t result = LE_BAD_PARAMETER;

    LE_INFO("SSID length %d | SSID: \"%.*s\"",
        (int)ssidNumElements,
        (int)ssidNumElements,
        (char *)ssidPtr);

    if ((0 < ssidNumElements) && (ssidNumElements <= LE_WIFIDEFS_MAX_SSID_LENGTH))
    {
        // Store SSID to be used later during startup procedure
        memcpy(&SavedSsid[0], (const char *)&ssidPtr[0], ssidNumElements);
        // Make sure there is a null termination
        SavedSsid[ssidNumElements] = '\0';
        result = LE_OK;
    }
    else
    {
        LE_ERROR("ERROR: SSID length exceeds %d bytes\n", LE_WIFIDEFS_MAX_SSID_LENGTH);
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the security protocol used to communicate.
 *
 * @note WEP is not supported as it is unsecure and has been deprecated in favor of WPA/WPA2.
 * This limitation applies to access point mode only.
 *
 * @return LE_BAD_PARAMETER Some parameter is invalid.
 * @return LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetSecurityProtocol
(
    le_wifiAp_SecurityProtocol_t securityProtocol
        ///< [IN]
        ///< The security protocol used to communicate with the access point.
)
{
    le_result_t result;

    LE_INFO("Security protocol : %d", securityProtocol);
    switch (securityProtocol)
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
 *
 * @note This is one way to authenticate against the access point. The other one is provided by the
 * pa_wifiAp_SetPreSharedKey() function. Both ways are exclusive and are effective only when used
 * with WPA-personal authentication.
 *
 * @return LE_BAD_PARAMETER Parameter is invalid.
 * @return LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetPassPhrase
(
    const char *passphrasePtr
        ///< [IN]
        ///< Passphrase to authenticate against the access point.
)
{
    le_result_t result = LE_BAD_PARAMETER;

    LE_INFO("Set passphrase");
    if (NULL != passphrasePtr)
    {
        uint32_t length = strlen(passphrasePtr);

        if ((length >= LE_WIFIDEFS_MIN_PASSPHRASE_LENGTH) &&
            (length <= LE_WIFIDEFS_MAX_PASSPHRASE_LENGTH))
        {
            // Store Passphrase to be used later during startup procedure
            strncpy(&SavedPassphrase[0], &passphrasePtr[0], length);
            // Make sure there is a null termination
            SavedPassphrase[length] = '\0';
            result = LE_OK;
        }
        else
        {
            LE_ERROR("Invalid passphrase length (%d) [%d..%d]",
                    length,
                    LE_WIFIDEFS_MIN_PASSPHRASE_LENGTH,
                    LE_WIFIDEFS_MAX_PASSPHRASE_LENGTH);
        }
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the pre-shared key (PSK).
 *
 * @note This is one way to authenticate against the access point. The other one is provided by the
 * pa_wifiAp_SetPassPhrase() function. Both ways are exclusive and are effective only when used
 * with WPA-personal authentication.
 *
 * @return LE_BAD_PARAMETER Parameter is invalid.
 * @return LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetPreSharedKey
(
    const char *preSharedKeyPtr
        ///< [IN]
        ///< Pre-shared key used to authenticate against the access point.
)
{
    le_result_t result = LE_BAD_PARAMETER;

    LE_INFO("Set PSK");
    if (NULL != preSharedKeyPtr)
    {
        uint32_t length = strlen(preSharedKeyPtr);

        if (length <= LE_WIFIDEFS_MAX_PSK_LENGTH)
        {
            // Store PSK to be used later during startup procedure
            strncpy( &SavedPreSharedKey[0], &preSharedKeyPtr[0], length );
            // Make sure there is a null termination
            SavedPreSharedKey[length] = '\0';
            result = LE_OK;
        }
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set if the access point should announce its presence.
 * Default value is TRUE.
 * If the value is set to FALSE, the access point will be hidden.
 *
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetDiscoverable
(
    bool isDiscoverable
        ///< [IN]
        ///< If TRUE, the access point SSID is visible by the clients otherwise it is hidden.
)
{
    // Store Discoverable to be used later during startup procedure
    LE_INFO("Set discoverability");
    SavedDiscoverable = isDiscoverable;
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the WiFi channel to use.
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

    LE_INFO("Set channel");
    if ((channelNumber >= LE_WIFIDEFS_MIN_CHANNEL_VALUE) &&
        (channelNumber <= LE_WIFIDEFS_MAX_CHANNEL_VALUE))
    {
       SavedChannelNumber = channelNumber;
       result = LE_OK;
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the maximum number of clients allowed to be connected to WiFi access point at the same time.
 * Default value is 10.
 *
 * @note This value depends on the hardware/software capabilities of the WiFi module used.
 *
 * @return LE_OUT_OF_RANGE  Requested number of users exceeds the capabilities of the Access Point.
 * @return LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetMaxNumberClients
(
    int maxNumberClients
        ///< [IN]
        ///< The maximum number of clients. This depends on the hardware/software capabilities.
)
{
    // Store maxNumberClients to be used later during startup procedure
    le_result_t result = LE_OUT_OF_RANGE;
    LE_INFO("Set max clients");
    if ((maxNumberClients >= 1) && (maxNumberClients <= TI_WIFI_MAX_USERS))
    {
       SavedMaxNumClients = maxNumberClients;
       result = LE_OK;
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Define the access point IP address and the client IP addresses range.
 *
 * @note The access point IP address must be defined outside the client IP addresses range.
 *
 * @return LE_BAD_PARAMETER At least, one of the given IP addresses is invalid.
 * @return LE_FAULT         A system call has failed.
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetIpRange
(
    const char *ipApPtr,
        ///< [IN]
        ///< the IP address of the Access Point.
    const char *ipStartPtr,
        ///< [IN]
        ///< the start IP address of the Access Point.
    const char *ipStopPtr
        ///< [IN]
        ///< the stop IP address of the Access Point.
)
{
    struct sockaddr_in  saApPtr;
    struct sockaddr_in  saStartPtr;
    struct sockaddr_in  saStopPtr;
    const char         *parameterPtr = 0;

    // Check the parameters
    if ((ipApPtr == NULL) || (ipStartPtr == NULL) || (ipStopPtr == NULL))
        return LE_BAD_PARAMETER;

    if ((!strlen(ipApPtr)) || (!strlen(ipStartPtr)) || (!strlen(ipStopPtr)))
        return LE_BAD_PARAMETER;

    if (inet_pton(AF_INET, ipApPtr, &(saApPtr.sin_addr)) <= 0)
    {
        parameterPtr = "AP";
    }
    else if (inet_pton(AF_INET, ipStartPtr, &(saStartPtr.sin_addr)) <= 0)
    {
        parameterPtr = "start";
    }
    else if (inet_pton(AF_INET, ipStopPtr, &(saStopPtr.sin_addr)) <= 0)
    {
        parameterPtr = "stop";
    }

    if (NULL != parameterPtr)
    {
        LE_ERROR("Invalid %s IP address", parameterPtr);
        return LE_BAD_PARAMETER;
    }
    else
    {
        unsigned int ap = ntohl(saApPtr.sin_addr.s_addr);
        unsigned int start = ntohl(saStartPtr.sin_addr.s_addr);
        unsigned int stop = ntohl(saStopPtr.sin_addr.s_addr);

        LE_INFO("@AP=%x, @APstart=%x, @APstop=%x",
                ap, start, stop);

        if (start > stop)
        {
            LE_INFO("Need to swap start & stop IP addresses.");
            start = start ^ stop;
            stop = stop ^ start;
            start = start ^ stop;
        }

        if ((ap >= start) && (ap <= stop))
        {
            LE_ERROR("AP IP address is within the range.");
            return LE_BAD_PARAMETER;
        }
    }

    putenv("PATH=/legato/systems/current/bin:/usr/local/bin:"
        "/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin");

    {
        char cmd[256];
        int  systemResult;

        snprintf((char *)&cmd, sizeof(cmd), "%s %s %s %s", "/sbin/ifconfig", "wlan0", ipApPtr, "up");

        systemResult = system(cmd);
        if (0 != WEXITSTATUS (systemResult))
        {
            LE_ERROR("Unable to mount the network interface.");
            return LE_FAULT;
        }
        else
        {
            FILE *filePtr;

            LE_INFO("Creation of dnsmasq configuration file (%s)", DNSMASQ_CFG_FILE);

            if (symlink(DNSMASQ_CFG_FILE, DNSMASQ_CFG_LINK) && (EEXIST != errno))
            {
                LE_ERROR("Unable to create link to dnsmasq configuration file: %m.");
                return LE_FAULT;
            }
            filePtr = fopen (DNSMASQ_CFG_FILE, "w");
            if (filePtr != NULL)
            {
                fprintf(filePtr, "dhcp-range=%s, %s,%s,%dh\n", "wlan0", ipStartPtr, ipStopPtr, 24);
                fprintf(filePtr, "dhcp-option=%s, %d,%s\n","wlan0", 3, ipApPtr);
                fprintf(filePtr, "dhcp-option=%s, %d,%s\n","wlan0", 6, ipApPtr);
                fclose(filePtr);
            }
            else
            {
                LE_ERROR("Unable to open the dnsmasq configuration file: %m.");
                return LE_FAULT;
            }

            LE_INFO("@AP=%s, @APstart=%s, @APstop=%s", ipApPtr, ipStartPtr, ipStopPtr);

            // Insert the rule allowing the DHCP ports on wlan0
            systemResult = system("iptables -I " COMMAND_IPTABLE_DHCP_WLAN0);
            if (0 != WEXITSTATUS (systemResult))
            {
                LE_ERROR("Unable to allow DHCP ports.");
                return LE_FAULT;
            }

            systemResult = system("/etc/init.d/dnsmasq stop; "
                                  "pkill -9 dnsmasq; "
                                  "/etc/init.d/dnsmasq start");
            if (0 != WEXITSTATUS (systemResult))
            {
                LE_ERROR("Unable to restart the DHCP server.");
                return LE_FAULT;
            }
        }
    }

    return LE_OK;
}
