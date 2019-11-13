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

// Set of commands to drive the WiFi features.
#define COMMAND_WIFI_HW_START        "WIFI_START"
#define COMMAND_WIFI_HW_STOP         "WIFI_STOP"
#define COMMAND_WIFI_SET_EVENT       "WIFI_SET_EVENT"
#define COMMAND_WIFI_UNSET_EVENT     "WIFI_UNSET_EVENT"
#define COMMAND_WIFIAP_HOSTAPD_START "WIFIAP_HOSTAPD_START"
#define COMMAND_WIFIAP_HOSTAPD_STOP  "WIFIAP_HOSTAPD_STOP"
#define COMMAND_WIFIAP_WLAN_UP       "WIFIAP_WLAN_UP"

// iptables rule to allow/disallow the DHCP port on WLAN interface
#define COMMAND_IPTABLE_DHCP_INSERT  "IPTABLE_DHCP_INSERT"
#define COMMAND_IPTABLE_DHCP_DELETE  "IPTABLE_DHCP_DELETE"
#define COMMAND_DNSMASQ_RESTART       "DNSMASQ_RESTART"

//--------------------------------------------------------------------------------------------------
/**
 * WiFi platform adaptor shell script
 */
//--------------------------------------------------------------------------------------------------
#define WIFI_SCRIPT_PATH "/legato/systems/current/apps/wifiService/read-only/pa_wifi "

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

//--------------------------------------------------------------------------------------------------
/**
 * Name servers configuration file
 */
//--------------------------------------------------------------------------------------------------
#define RESOLV_CFG_FILE "/etc/resolv.conf"

// WiFi access point configuration.
//--------------------------------------------------------------------------------------------------
/**
 * Host access point global configuration.
 */
//--------------------------------------------------------------------------------------------------
#define HOSTAPD_CONFIG_COMMON \
    "driver=nl80211\n"\
    "wmm_enabled=1\n"\
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
#define HOSTAPD_CONFIG_SECURITY_NONE \
    "auth_algs=1\n"\
    "eap_server=0\n"\
    "eapol_key_index_workaround=0\n"\
    "macaddr_acl=0\n"
//--------------------------------------------------------------------------------------------------
/**
 * Host access point configuration with security enabled.
 */
//--------------------------------------------------------------------------------------------------
#define HOSTAPD_CONFIG_SECURITY_WPA2 \
    "wpa=2\n"\
    "wpa_key_mgmt=WPA-PSK\n"\
    "wpa_pairwise=CCMP\n"\
    "rsn_pairwise=CCMP\n"

//--------------------------------------------------------------------------------------------------
/**
 * Maximum numbers of WiFi connections for the TI chip
 */
//--------------------------------------------------------------------------------------------------
#define WIFI_MAX_USERS 10

//--------------------------------------------------------------------------------------------------
/**
 * Hardware mode mask
 */
//--------------------------------------------------------------------------------------------------
#define HARDWARE_MODE_MASK 0x000F

//--------------------------------------------------------------------------------------------------
/**
 * Maximum numbers of bytes in temporary string written to hostapd.conf
 */
//--------------------------------------------------------------------------------------------------
#define TEMP_STRING_MAX_BYTES 1024
//--------------------------------------------------------------------------------------------------
/**
 * The current security protocol
 */
//--------------------------------------------------------------------------------------------------
static le_wifiAp_SecurityProtocol_t SavedSecurityProtocol =
                                                    LE_WIFICLIENT_SECURITY_WPA2_PSK_PERSONAL;
//--------------------------------------------------------------------------------------------------
/**
 * The current SSID
 */
//--------------------------------------------------------------------------------------------------
static char          SavedSsid[LE_WIFIDEFS_MAX_SSID_BYTES] = "";
//--------------------------------------------------------------------------------------------------
/**
 * The current country code
 */
//--------------------------------------------------------------------------------------------------
static char          SavedCountryCode[LE_WIFIDEFS_MAX_COUNTRYCODE_BYTES] = { 'U', 'S', '\0'};
//--------------------------------------------------------------------------------------------------
/**
 * The current IEEE mask
 * IEEE 802.11g is set by default
 */
//--------------------------------------------------------------------------------------------------
static le_wifiAp_IeeeStdBitMask_t   SavedIeeeStdMask = 0x0004;
//--------------------------------------------------------------------------------------------------
/**
 * The current MAX and MIN channel
 * Different MAX/MIN channel for different hardware mode
 * IEEE 802.11g is set by default
 */
//--------------------------------------------------------------------------------------------------
static uint16_t MIN_CHANNEL_VALUE = LE_WIFIDEFS_MIN_CHANNEL_VALUE;
static uint16_t MAX_CHANNEL_VALUE = LE_WIFIDEFS_MAX_CHANNEL_VALUE;
//--------------------------------------------------------------------------------------------------
/**
 * Defines whether the SSID is hidden or not
 */
//--------------------------------------------------------------------------------------------------
static bool                         SavedDiscoverable                     = true;
//--------------------------------------------------------------------------------------------------
/**
 * The WiFi channel associated with the SSID.
 * In the 2.4 GHz band, 1, 6, and 11 are the only non-overlapping channels.
 * Default channel is set to 6
 */
//--------------------------------------------------------------------------------------------------
static uint16_t                     SavedChannelNumber                    = 6;
//--------------------------------------------------------------------------------------------------
/**
 * The maximum numbers of clients the AP is able to manage
 */
//--------------------------------------------------------------------------------------------------
static uint32_t                     SavedMaxNumClients                    = WIFI_MAX_USERS;

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
static le_event_Id_t    WifiApPaEvent;

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
    int status;

    // Kill the script launched by popen() in PA thread
    status = system(WIFI_SCRIPT_PATH COMMAND_WIFI_UNSET_EVENT);

    if ((!WIFEXITED(status)) || (0 != WEXITSTATUS(status)))
    {
        LE_ERROR("Unable to kill the WIFI events script");
    }

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
    char path[1024];

    LE_INFO("Wifi event report thread started!");

    // Open the command "iw events" for reading.
    IwThreadPipePtr = popen(WIFI_SCRIPT_PATH COMMAND_WIFI_SET_EVENT, "r");

    if (NULL == IwThreadPipePtr)
    {
        LE_ERROR("Failed to run command:\"%s\" errno:%d %s",
                COMMAND_WIFI_SET_EVENT,
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

//--------------------------------------------------------------------------------------------------
/**
 * This function writes configuration to hostapd.conf.
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t GenerateHostapdConf
(
    void
)
{
    char        tmpConfig[TEMP_STRING_MAX_BYTES];
    le_result_t result = LE_FAULT;
    FILE        *configFilePtr  = NULL;

    configFilePtr = fopen(WIFI_HOSTAPD_FILE, "w");
    if (NULL ==  configFilePtr)
    {
        LE_ERROR("Unable to create hostapd.conf file.");
        return LE_FAULT;
    }

    memset(tmpConfig, '\0', sizeof(tmpConfig));
    // prepare SSID, channel, country code etc in hostapd.conf
    snprintf(tmpConfig, sizeof(tmpConfig), (HOSTAPD_CONFIG_COMMON
            "ssid=%s\nchannel=%d\nmax_num_sta=%d\ncountry_code=%s\nignore_broadcast_ssid=%d\n"),
            (char *)SavedSsid,
            SavedChannelNumber,
            SavedMaxNumClients,
            (char *)SavedCountryCode,
            !SavedDiscoverable);
    // Write common config such as SSID, channel, country code, etc in hostapd.conf
    tmpConfig[TEMP_STRING_MAX_BYTES - 1] = '\0';
    if (LE_OK != WriteApCfgFile(tmpConfig, configFilePtr))
    {
        LE_ERROR("Unable to set SSID, channel, etc in hostapd.conf");
        goto error;
    }

    memset(tmpConfig, '\0', sizeof(tmpConfig));
    // Write security parameters in hostapd.conf
    switch (SavedSecurityProtocol)
    {
        case LE_WIFIAP_SECURITY_NONE:
            LE_DEBUG("LE_WIFIAP_SECURITY_NONE");
            result = WriteApCfgFile(HOSTAPD_CONFIG_SECURITY_NONE, configFilePtr);
            break;

        case LE_WIFIAP_SECURITY_WPA2:
            LE_DEBUG("LE_WIFIAP_SECURITY_WPA2");
            if ('\0' != SavedPassphrase[0])
            {
                snprintf(tmpConfig, sizeof(tmpConfig), (HOSTAPD_CONFIG_SECURITY_WPA2
                        "wpa_passphrase=%s\n"), SavedPassphrase);
                tmpConfig[TEMP_STRING_MAX_BYTES - 1] = '\0';
                result = WriteApCfgFile(tmpConfig, configFilePtr);
            }
            else if ('\0' != SavedPreSharedKey[0])
            {
                snprintf(tmpConfig, sizeof(tmpConfig), (HOSTAPD_CONFIG_SECURITY_WPA2
                        "wpa_psk=%s\n"), SavedPreSharedKey);
                tmpConfig[TEMP_STRING_MAX_BYTES - 1] = '\0';
                result = WriteApCfgFile(tmpConfig, configFilePtr);
            }
            else
            {
                LE_ERROR("Security protocol is missing!");
                result = LE_FAULT;
            }
            break;

        default:
            LE_ERROR("Unsupported security protocol!");
            result = LE_FAULT;
            break;
    }

    // Write security parameters in hostapd.conf
    if (LE_OK != result)
    {
        LE_ERROR("Unable to set security parameters in hostapd.conf");
        goto error;
    }

    // prepare IEEE std including hardware mode into hostapd.conf
    memset(tmpConfig, '\0', sizeof(tmpConfig));
    switch( SavedIeeeStdMask & HARDWARE_MODE_MASK )
    {
        case LE_WIFIAP_BITMASK_IEEE_STD_A:
            le_utf8_Copy(tmpConfig, "hw_mode=a\n", sizeof(tmpConfig), NULL);
            break;
        case LE_WIFIAP_BITMASK_IEEE_STD_B:
            le_utf8_Copy(tmpConfig, "hw_mode=b\n", sizeof(tmpConfig), NULL);
            break;
        case LE_WIFIAP_BITMASK_IEEE_STD_G:
            le_utf8_Copy(tmpConfig, "hw_mode=g\n", sizeof(tmpConfig), NULL);
            break;
        case LE_WIFIAP_BITMASK_IEEE_STD_AD:
            le_utf8_Copy(tmpConfig, "hw_mode=ad\n", sizeof(tmpConfig), NULL);
            break;
        default:
            le_utf8_Copy(tmpConfig, "hw_mode=g\n", sizeof(tmpConfig), NULL);
            break;
    }

    if ( SavedIeeeStdMask & LE_WIFIAP_BITMASK_IEEE_STD_D )
    {
        le_utf8_Append(tmpConfig, "ieee80211d=1\n", sizeof(tmpConfig), NULL);
    }
    if ( SavedIeeeStdMask & LE_WIFIAP_BITMASK_IEEE_STD_H )
    {
        le_utf8_Append(tmpConfig, "ieee80211h=1\n", sizeof(tmpConfig), NULL);
    }
    if ( SavedIeeeStdMask & LE_WIFIAP_BITMASK_IEEE_STD_N )
    {
        // hw_mode=b does not support ieee80211n, but driver can handle it
        le_utf8_Append(tmpConfig, "ieee80211n=1\n", sizeof(tmpConfig), NULL);
    }
    if ( SavedIeeeStdMask & LE_WIFIAP_BITMASK_IEEE_STD_AC )
    {
        le_utf8_Append(tmpConfig, "ieee80211ac=1\n", sizeof(tmpConfig), NULL);
    }
    if ( SavedIeeeStdMask & LE_WIFIAP_BITMASK_IEEE_STD_AX )
    {
        le_utf8_Append(tmpConfig, "ieee80211ax=1\n", sizeof(tmpConfig), NULL);
    }
    if ( SavedIeeeStdMask & LE_WIFIAP_BITMASK_IEEE_STD_W )
    {
        le_utf8_Append(tmpConfig, "ieee80211w=1\n", sizeof(tmpConfig), NULL);
    }
    // Write IEEE std in hostapd.conf
    tmpConfig[TEMP_STRING_MAX_BYTES - 1] = '\0';
    if (LE_OK != WriteApCfgFile(tmpConfig, configFilePtr))
    {
        LE_ERROR("Unable to set IEEE std in hostapd.conf");
        goto error;
    }
    fclose(configFilePtr);
    return LE_OK;

error:
    fclose(configFilePtr);
    // Remove generated hostapd.conf file
    remove(WIFI_HOSTAPD_FILE);
    return LE_FAULT;
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
    le_result_t result = LE_OK;

    LE_INFO("pa_wifiAp_Init() called");
    // Create the event for signaling user handlers.
    WifiApPaEvent = le_event_CreateId("WifiApPaEvent", sizeof(le_wifiAp_Event_t));

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
 * @return LE_NOT_FOUND     The WiFi card is absent.
 * @return LE_NOT_POSSIBLE  The WiFi card may not work.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_Start
(
    void
)
{
    int     systemResult;

    // Check that an SSID is provided before starting
    if ('\0' == SavedSsid[0])
    {
        LE_ERROR("Unable to start AP because no valid SSID provided");
        return LE_FAULT;
    }

    // Check channel number is properly set before starting
    if ((SavedChannelNumber < MIN_CHANNEL_VALUE) ||
            (SavedChannelNumber > MAX_CHANNEL_VALUE))
    {
        LE_ERROR("Unable to start AP because no valid channel number provided");
        return LE_FAULT;
    }

    LE_DEBUG("Starting AP, SSID: %s", SavedSsid);

    // Create hostapd.conf file in /tmp
    if ( LE_OK != GenerateHostapdConf())
    {
        LE_ERROR("Failed to generate hostapd.conf");
        return LE_FAULT;
    }

    systemResult = system(WIFI_SCRIPT_PATH COMMAND_WIFI_HW_START);
    /**
     * Returned values:
     *   0: if the interface is correctly moutned
     *  50: if WiFi card is not inserted
     * 100: if WiFi card may not work
     * 127: if driver can not be installed
     *  -1: if the fork() has failed (see man system)
     */

    if (0 == WEXITSTATUS(systemResult))
    {
        LE_DEBUG("WiFi hardware started correctly");
        // Create WiFi AP PA Thread
        WifiApPaThread = le_thread_Create("WifiApPaThread", WifiApPaThreadMain, NULL);
        le_thread_SetJoinable(WifiApPaThread);
        le_thread_AddChildDestructor(WifiApPaThread, ThreadDestructor, NULL);
        le_thread_Start(WifiApPaThread);
    }
    // Return value of 50 means WiFi card is not inserted.
    else if ( PA_NOT_FOUND == WEXITSTATUS(systemResult))
    {
        LE_ERROR("WiFi card is not inserted");
        return LE_NOT_FOUND;
    }
    // Return value of 100 means WiFi card may not work.
    else if ( PA_NOT_POSSIBLE == WEXITSTATUS(systemResult))
    {
        LE_ERROR("Unable to reset WiFi card");
        return LE_NOT_POSSIBLE;
    }
    // WiFi card failed to start.
    else
    {
        LE_WARN("Failed to start WiFi AP command \"%s\" systemResult (%d)",
                COMMAND_WIFI_HW_START, systemResult);
        return LE_FAULT;
    }

    // Start Access Point cmd: /bin/hostapd /etc/hostapd.conf
    systemResult = system(WIFI_SCRIPT_PATH COMMAND_WIFIAP_HOSTAPD_START);
    if ((!WIFEXITED(systemResult)) || (0 != WEXITSTATUS(systemResult)))
    {
        LE_ERROR("WiFi Client Command \"%s\" Failed: (%d)",
                COMMAND_WIFIAP_HOSTAPD_START,
                systemResult);
        // Remove generated hostapd.conf file
        remove(WIFI_HOSTAPD_FILE);
        goto error;
    }

    LE_INFO("WiFi AP started correclty");
    return LE_OK;

error:
    le_thread_Cancel(WifiApPaThread);
    le_thread_Join(WifiApPaThread, NULL);
    return LE_FAULT;
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
    int status;

    // Try to delete the rule allowing the DHCP ports on WLAN. Ignore if it fails
    status = system(WIFI_SCRIPT_PATH COMMAND_IPTABLE_DHCP_DELETE);
    if ((!WIFEXITED(status)) || (0 != WEXITSTATUS(status)))
    {
        LE_WARN("Deleting rule for DHCP port fails");
    }

    status = system(WIFI_SCRIPT_PATH COMMAND_WIFIAP_HOSTAPD_STOP);
    if ((!WIFEXITED(status)) || (0 != WEXITSTATUS(status)))
    {
        LE_ERROR("WiFi AP Command \"%s\" Failed: (%d)",
                COMMAND_WIFIAP_HOSTAPD_STOP,
                status);
        return LE_FAULT;
    }

    status = system(WIFI_SCRIPT_PATH COMMAND_WIFI_HW_STOP);
    if ((!WIFEXITED(status)) || (0 != WEXITSTATUS(status)))
    {
        LE_ERROR("WiFi AP Command \"%s\" Failed: (%d)", COMMAND_WIFI_HW_STOP, status);
        return LE_FAULT;
    }

    // Cancel the previously created thread
    le_thread_Cancel(WifiApPaThread);
    if (LE_OK != le_thread_Join(WifiApPaThread, NULL))
    {
        return LE_FAULT;
    }

    // Remove the previously created hostapd.conf file in /tmp
    remove(WIFI_HOSTAPD_FILE);

    LE_INFO("WiFi AP stopped correclty");
    return LE_OK;
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
            le_utf8_Copy(SavedPassphrase, passphrasePtr, sizeof(SavedPassphrase), NULL);
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
            le_utf8_Copy(SavedPreSharedKey, preSharedKeyPtr, sizeof(SavedPreSharedKey), NULL);
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
 * Set which WiFi channel to use.
 * Default number is 7.
 * Some legal restrictions might apply for your region.
 * The channel number must be between 1 and 14 for IEEE 802.11b/g.
 * The channel number must be between 7 and 196 for IEEE 802.11a.
 * The channel number must be between 1 and 6 for IEEE 802.11ad.
 * @return
 *      - LE_OUT_OF_RANGE if requested channel number is out of range.
 *      - LE_OK if the function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetChannel
(
    uint16_t channelNumber
        ///< [IN]
        ///< the channel number.
)
{
    le_result_t result = LE_OUT_OF_RANGE;
    int8_t hwMode = SavedIeeeStdMask & 0x0F;

    LE_INFO("Set channel");
    switch (hwMode)
    {
        case LE_WIFIAP_BITMASK_IEEE_STD_A:
            MIN_CHANNEL_VALUE = LE_WIFIDEFS_MIN_CHANNEL_STD_A;
            MAX_CHANNEL_VALUE = LE_WIFIDEFS_MAX_CHANNEL_STD_A;
            break;
        case LE_WIFIAP_BITMASK_IEEE_STD_B:
        case LE_WIFIAP_BITMASK_IEEE_STD_G:
            MIN_CHANNEL_VALUE = LE_WIFIDEFS_MIN_CHANNEL_VALUE;
            MAX_CHANNEL_VALUE = LE_WIFIDEFS_MAX_CHANNEL_VALUE;
            break;
        case LE_WIFIAP_BITMASK_IEEE_STD_AD:
            MIN_CHANNEL_VALUE = LE_WIFIDEFS_MIN_CHANNEL_STD_AD;
            MAX_CHANNEL_VALUE = LE_WIFIDEFS_MAX_CHANNEL_STD_AD;
            break;
        default:
            LE_WARN("Invalid hardware mode");
    }

    if ((channelNumber >= MIN_CHANNEL_VALUE) &&
        (channelNumber <= MAX_CHANNEL_VALUE))
    {
       SavedChannelNumber = channelNumber;
       result = LE_OK;
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set which IEEE standard to use.
 * Default hardware mode is IEEE 802.11g.
 *
 * @return
 *      - LE_BAD_PARAMETER if invalid IEEE standard is set.
 *      - LE_OK if the function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetIeeeStandard
(
    le_wifiAp_IeeeStdBitMask_t stdMask
        ///< [IN]
        ///< Bit mask for the IEEE standard.
)
{
    int8_t hwMode = stdMask & 0x0F;
    int8_t numCheck = (hwMode & 0x1) + ((hwMode >> 1) & 0x1) +
                       ((hwMode >> 2) & 0x1) + ((hwMode >> 3) & 0x1);

    LE_INFO("Set IeeeStdBitMask : 0x%X", stdMask);
    //Hardware mode should be set.
    if ( 0 == numCheck )
    {
        LE_WARN("No hardware mode is set.");
        return LE_BAD_PARAMETER;
    }
    //Hardware mode should be exclusive.
    if ( numCheck > 1 )
    {
        LE_WARN("Only one hardware mode can be set.");
        return LE_BAD_PARAMETER;
    }

    if ( stdMask & LE_WIFIAP_BITMASK_IEEE_STD_AC )
    {
        // ieee80211ac=1 only works with hw_mode=a
        if ( 0 == (stdMask & LE_WIFIAP_BITMASK_IEEE_STD_A) )
        {
            LE_WARN("ieee80211ac=1 only works with hw_mode=a.");
            return LE_BAD_PARAMETER;
        }
    }

    if ( stdMask & LE_WIFIAP_BITMASK_IEEE_STD_H )
    {
        // ieee80211h=1 can be used only with ieee80211d=1
        if ( 0 == (stdMask & LE_WIFIAP_BITMASK_IEEE_STD_D) )
        {
            LE_WARN("ieee80211h=1 only works with ieee80211d=1.");
            return LE_BAD_PARAMETER;
        }
    }

    SavedIeeeStdMask = stdMask;
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get which IEEE standard was set.
 * Default hardware mode is IEEE 802.11g.
 *
 * @return
 *      - LE_FAULT if the function failed.
 *      - LE_OK if the function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_GetIeeeStandard
(
    le_wifiAp_IeeeStdBitMask_t *stdMaskPtr
        ///< [OUT]
        ///< Bit mask for the IEEE standard.
)
{
    if ( NULL == stdMaskPtr )
    {
        LE_WARN("stdMaskPtr == NULL");
        return LE_FAULT;
    }

    *stdMaskPtr = SavedIeeeStdMask;
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set what country code to use for regulatory domain.
 * ISO/IEC 3166-1 Alpha-2 code is used.
 * Default country code is US.
 * @return
 *      - LE_FAULT if the function failed.
 *      - LE_OK if the function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetCountryCode
(
    const char *countryCodePtr
        /// [IN]
        ///< the country code.
)
{
    le_result_t result = LE_FAULT;

    LE_INFO("Set countryCode");
    if (NULL != countryCodePtr)
    {
        uint32_t length = strlen(countryCodePtr);

        if (length == LE_WIFIDEFS_ISO_COUNTRYCODE_LENGTH)
        {
            strncpy(&SavedCountryCode[0], &countryCodePtr[0], length );
            SavedCountryCode[length] = '\0';
            result = LE_OK;
        }
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
    if ((maxNumberClients >= 1) && (maxNumberClients <= WIFI_MAX_USERS))
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
    if ((NULL == ipApPtr) || (NULL == ipStartPtr) || (NULL == ipStopPtr))
    {
        return LE_BAD_PARAMETER;
    }

    if ((!strlen(ipApPtr)) || (!strlen(ipStartPtr)) || (!strlen(ipStopPtr)))
    {
        return LE_BAD_PARAMETER;
    }

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

        snprintf((char *)&cmd, sizeof(cmd), "%s %s %s",
                WIFI_SCRIPT_PATH,
                COMMAND_WIFIAP_WLAN_UP,
                ipApPtr);

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
                //Interface is generated when COMMAND_DNSMASQ_RESTART called
                fprintf(filePtr, "dhcp-range=%s,%s,%dh\n", ipStartPtr, ipStopPtr, 24);
                fprintf(filePtr, "dhcp-option=%d,%s\n", 3, ipApPtr);

                FILE *resolvConfPtr = fopen(RESOLV_CFG_FILE, "r");
                char buf[128];
                char dns[256] = "";
                char *dPtr;
                int len, remain;

                remain = 256;
                if (resolvConfPtr)
                {
                    // Retrieve DNS IP addresses configured behind the
                    // nameserver keyword on all lines of RESOLV_CFG_FILE
                    while (fgets(buf, 128, resolvConfPtr))
                    {
                        if (strstr(buf, "nameserver"))
                        {
                            // if dns is not empty, we need to add a comma to
                            // separate each dns
                            if (strlen(dns))
                            {
                                strncat(dns, ",", 1);
                                --remain;
                            }

                            dPtr = strchr(buf, ' ');
                            ++dPtr; // move the pointer to the next character (current
                                    // is pointing at the ' ')
                            //determine the actual copying length
                            len = strlen(dPtr);
                            len = ((dPtr[len-1] == '\n') ? len-1 : len);

                            // if dns array is full but more DNS servers, then
                            // skip for the remaining.
                            // we have to skip if remain == len, because we need
                            // 1 more char for null character
                            if (remain <= len)
                            {
                                break;
                            }

                            // concat dPtr to dns
                            strncat(dns, dPtr, len);
                            remain -= len;
                        }
                        // will skip the current line if it doesn't contain
                        // the tag "nameserver"
                    }

                    fclose(resolvConfPtr);

                    LE_DEBUG("dns to be written: %s", dns);

                    fprintf(filePtr, "dhcp-option=%d,%s\n", 6, dns);
                }
                else
                {
                    LE_WARN("Cannot retrieve DNS address for dhcp-option 6, "
                            "using device's own IP instead");
                    fprintf(filePtr, "dhcp-option=%d,%s\n", 6, ipApPtr);
                }

                fclose(filePtr);
            }
            else
            {
                LE_ERROR("Unable to open the dnsmasq configuration file: %m.");
                return LE_FAULT;
            }

            LE_INFO("@AP=%s, @APstart=%s, @APstop=%s", ipApPtr, ipStartPtr, ipStopPtr);

            // Insert the rule allowing the DHCP ports on WLAN
            systemResult = system(WIFI_SCRIPT_PATH COMMAND_IPTABLE_DHCP_INSERT);
            if (0 != WEXITSTATUS (systemResult))
            {
                LE_ERROR("Unable to allow DHCP ports.");
                return LE_FAULT;
            }

            systemResult = system(WIFI_SCRIPT_PATH COMMAND_DNSMASQ_RESTART);
            if (0 != WEXITSTATUS (systemResult))
            {
                LE_ERROR("Unable to restart the DHCP server.");
                return LE_FAULT;
            }
        }
    }

    return LE_OK;
}
