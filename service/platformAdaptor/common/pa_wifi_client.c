// -------------------------------------------------------------------------------------------------
/**
 *  WiFi Client Platform Adapter
 *
 *  Copyright (C) Sierra Wireless Inc.
 *
 */
// -------------------------------------------------------------------------------------------------
#include <sys/types.h>
#include <sys/wait.h>

#include "legato.h"

#include "interfaces.h"

#include "pa_wifi.h"

//--------------------------------------------------------------------------------------------------
/**
 * WiFi platform adaptor shell script
 */
//--------------------------------------------------------------------------------------------------
//Trailing space is needed to pass argument
#define WIFI_SCRIPT_PATH "/legato/systems/current/apps/wifiService/read-only/pa_wifi "
#define WPA_SUPPLICANT_FILE "/tmp/wpa_supplicant.conf"

// Set of commands to drive the WiFi features.
#define COMMAND_WIFI_HW_START           "WIFI_START"
#define COMMAND_WIFI_HW_STOP            "WIFI_STOP"
#define COMMAND_WIFI_CHECK_HWSTATUS     "WIFI_CHECK_HWSTATUS"
#define COMMAND_WIFI_SET_EVENT          "WIFI_SET_EVENT"
#define COMMAND_WIFI_UNSET_EVENT        "WIFI_UNSET_EVENT"
#define COMMAND_WIFICLIENT_START_SCAN   "WIFICLIENT_START_SCAN"
#define COMMAND_WIFICLIENT_DISCONNECT   "WIFICLIENT_DISCONNECT"
//Trailing space is needed to pass another argument by WIFI_SCRIPT_PATH
#define COMMAND_WIFICLIENT_CONNECT      "WIFICLIENT_CONNECT "

//--------------------------------------------------------------------------------------------------
/**
 * The common wpa configuration.
 */
//--------------------------------------------------------------------------------------------------
#define WPA_SUPPLICANT_CONFIG_COMMON \
"ctrl_interface=/var/run/wpa_supplicant\n \
ctrl_interface_group=0\n \
update_config=1\n \
network={\n \
ssid=\"%.*s\"\n \
scan_ssid=%d\n"

//--------------------------------------------------------------------------------------------------
#define PATH_MAX_BYTES      1024

//--------------------------------------------------------------------------------------------------
/**
 * The current security protocol.
 */
//--------------------------------------------------------------------------------------------------
static le_wifiClient_SecurityProtocol_t SavedSecurityProtocol =
                                                        LE_WIFICLIENT_SECURITY_WPA2_PSK_PERSONAL;

// WEP
//--------------------------------------------------------------------------------------------------
/**
 * WEP key used for authentication. Used only with WEP protocol.
 */
//--------------------------------------------------------------------------------------------------
static char SavedWepKey[LE_WIFIDEFS_MAX_WEPKEY_BYTES] = {0};

// WPA-Personal
//--------------------------------------------------------------------------------------------------
/**
 * Passphrase used for authentication. Used only with WPA/WPA2 protocol.
 */
//--------------------------------------------------------------------------------------------------
static char SavedPassphrase[LE_WIFIDEFS_MAX_PASSPHRASE_BYTES] = {0};
//--------------------------------------------------------------------------------------------------
/**
 * Pre-Shared-Key used for authentication. Used only with WPA/WPA2 protocol.
 */
//--------------------------------------------------------------------------------------------------
static char SavedPreSharedKey[LE_WIFIDEFS_MAX_PSK_BYTES] = {0};

// WPA-Enterprise
//--------------------------------------------------------------------------------------------------
/**
 * Username used for authentication. Used only with WPA/WPA2 protocol with RADIUS server.
 */
//--------------------------------------------------------------------------------------------------
static char SavedUsername[LE_WIFIDEFS_MAX_USERNAME_BYTES] = {0};
//--------------------------------------------------------------------------------------------------
/**
 * Password used for authentication. Used only with WPA/WPA2 protocol with RADIUS server.
 */
//--------------------------------------------------------------------------------------------------
static char SavedPassword[LE_WIFIDEFS_MAX_PASSWORD_BYTES] = {0};

//--------------------------------------------------------------------------------------------------
/**
 * Indicates if the Access Point SSID is hidden from scan or not
 */
//--------------------------------------------------------------------------------------------------
static bool HiddenAccessPoint = false;
//--------------------------------------------------------------------------------------------------
/**
 * The handle of the input pipe used to be notified of the WiFi events during the scan.
 */
//--------------------------------------------------------------------------------------------------
static FILE *IwScanPipePtr    = NULL;
//--------------------------------------------------------------------------------------------------
/**
 * The handle of the input pipe used to be notified of the WiFi events.
 */
//--------------------------------------------------------------------------------------------------
static FILE *IwThreadPipePtr  = NULL;
//--------------------------------------------------------------------------------------------------
/**
 * Flag set when a WiFi scan is in progress.
 */
//--------------------------------------------------------------------------------------------------
static bool  IsScanRunning    = false;

//--------------------------------------------------------------------------------------------------
/**
 * The main thread running the WiFi platform adaptor.
 */
//--------------------------------------------------------------------------------------------------
static void *WifiClientPaThreadMain(void *contextPtr);
//--------------------------------------------------------------------------------------------------
/**
 * The handle of the WiFi platform adaptor thread.
 */
//--------------------------------------------------------------------------------------------------
static le_thread_Ref_t WifiClientPaThread = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * WifiClient state event ID.
 * Used to report WifiClient state events to the registered event handlers.
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t WifiClientPaEvent;
//--------------------------------------------------------------------------------------------------
/**
 * WifiClient state event ID.
 * Used to report WifiClient state events to the registered event handlers.
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t WifiClientPaEventId;

//--------------------------------------------------------------------------------------------------
/**
 * Pool for WifiClient state events reporting.
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t WifiPaEventPool;

//--------------------------------------------------------------------------------------------------
/**
 * Maximum numbers of bytes in temparatory string
 */
//--------------------------------------------------------------------------------------------------
#define TEMP_STRING_MAX_BYTES 128

//--------------------------------------------------------------------------------------------------
/**
 * Maximum numbers of bytes in temparatory config string written to wpa_supplicant.conf
 */
//--------------------------------------------------------------------------------------------------
#define TEMP_CONFIG_MAX_BYTES 1024

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
    int systemResult;

    // Kill the script launched by popen() in Client thread
    systemResult = system(WIFI_SCRIPT_PATH COMMAND_WIFI_UNSET_EVENT);

    if ((!WIFEXITED(systemResult)) || (0 != WEXITSTATUS(systemResult)))
    {
        LE_WARN("Unable to kill the WIFI events script");
    }

    if (IwThreadPipePtr)
    {
        // And close FP used in created thread
        pclose(IwThreadPipePtr);
        IwThreadPipePtr = NULL;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * The first-layer WiFi Client Event Handler.
 * @deprecated pa_wifiClient_AddEventHandler() should not be used anymore.
 * It has been replaced by pa_wifiClient_AddEventIndHandler().
 *
 */
//--------------------------------------------------------------------------------------------------
static void FirstLayerWifiClientEventHandler
(
    void *reportPtr,
    void *secondLayerHandlerFuncPtr
)
{
    pa_wifiClient_NewEventHandlerFunc_t  clientHandlerFunc = secondLayerHandlerFuncPtr;
    le_wifiClient_Event_t    *wifiEventPtr = (le_wifiClient_Event_t *)reportPtr;

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
 * The first-layer WiFi Client Event Indication Handler.
 *
 */
//--------------------------------------------------------------------------------------------------
static void FirstLayerWifiClientEventIndHandler
(
    void *reportPtr,
    void *secondLayerHandlerFuncPtr
)
{
    pa_wifiClient_EventIndHandlerFunc_t  clientHandlerFunc = secondLayerHandlerFuncPtr;
    le_wifiClient_EventInd_t*  wifiEventIndPtr = reportPtr;

    if (NULL != wifiEventIndPtr)
    {
        LE_DEBUG("WiFi event: %d, interface: %s, bssid: %s",
                wifiEventIndPtr->event,
                wifiEventIndPtr->ifName,
                wifiEventIndPtr->apBssid);

        if (LE_WIFICLIENT_EVENT_DISCONNECTED == wifiEventIndPtr->event)
        {
            LE_DEBUG("disconnectCause: %d", wifiEventIndPtr->disconnectionCause);
        }

        clientHandlerFunc(wifiEventIndPtr, le_event_GetContextPtr());
    }
    else
    {
        LE_ERROR("wifiEventIndPtr is NULL");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * WiFi Client PA Thread
 *
 */
//--------------------------------------------------------------------------------------------------
static void *WifiClientPaThreadMain
(
    void *contextPtr
)
{
    le_wifiClient_DisconnectionCause_t cause;
    le_wifiClient_Event_t              event;
    char path[PATH_MAX_BYTES];
    char apBssid[LE_WIFIDEFS_MAX_BSSID_BYTES];
    char *ret;
    char *pathReentrant;
    int  systemResult;

    LE_INFO("Wifi event report thread started!");

    IwThreadPipePtr = popen(WIFI_SCRIPT_PATH COMMAND_WIFI_SET_EVENT, "r");

    if (NULL == IwThreadPipePtr)
    {
        LE_ERROR("Failed to run command:\"%s\" errno:%d %s",
                COMMAND_WIFI_SET_EVENT,
                errno,
                strerror(errno));
        return NULL;
    }

    memset(apBssid, 0, LE_WIFIDEFS_MAX_BSSID_BYTES);
    cause = LE_WIFICLIENT_UNKNOWN_CAUSE;
    // Read the output one line at a time - output it.
    while (NULL != fgets(path, sizeof(path) - 1, IwThreadPipePtr))
    {
        LE_DEBUG("PARSING:%s: len:%d", path, (int) strnlen(path, sizeof(path) - 1));

        if (NULL != strstr(path, "Beacon loss"))
        {
            cause = LE_WIFICLIENT_BEACON_LOSS;
        }
        if (NULL != (ret = strstr(path, "del station")))
        {
            memset(apBssid, 0, LE_WIFIDEFS_MAX_BSSID_BYTES);
            memcpy(apBssid, &ret[sizeof("del station")],
                   LE_WIFIDEFS_MAX_BSSID_LENGTH);
            apBssid[LE_WIFIDEFS_MAX_BSSID_LENGTH] = '\0';
        }
        if (NULL != (ret = strstr(path, "connected to")))
        {
            LE_INFO("FOUND connected");

            cause = LE_WIFICLIENT_UNKNOWN_CAUSE;
            le_wifiClient_EventInd_t* WifiClientPaEventPtr = le_mem_ForceAlloc(WifiPaEventPool);
            memset(WifiClientPaEventPtr, 0, sizeof(le_wifiClient_EventInd_t));
            WifiClientPaEventPtr->event = LE_WIFICLIENT_EVENT_CONNECTED;
            WifiClientPaEventPtr->disconnectionCause = cause;
            // Retrieve AP BSSID
            memcpy(WifiClientPaEventPtr->apBssid, &ret[sizeof("connected to")],
                    LE_WIFIDEFS_MAX_BSSID_LENGTH);
            WifiClientPaEventPtr->apBssid[LE_WIFIDEFS_MAX_BSSID_LENGTH] = '\0';
            // Retrieve WLAN interface name
            pathReentrant = path;
            ret = strtok_r(pathReentrant, " ", &pathReentrant);
            if (NULL == ret)
            {
                LE_WARN("Failed to retrieve WLAN interface");
                WifiClientPaEventPtr->ifName[0] = '\0';
            }
            else
            {
                memcpy(WifiClientPaEventPtr->ifName, ret, strlen(ret));
                WifiClientPaEventPtr->ifName[LE_WIFIDEFS_MAX_IFNAME_LENGTH] = '\0';
            }
            // Report event: LE_WIFICLIENT_EVENT_CONNECTED
            LE_DEBUG("WiFi event: %d, interface: %s, bssid: %s",
                     WifiClientPaEventPtr->event,
                     WifiClientPaEventPtr->ifName,
                     WifiClientPaEventPtr->apBssid);

            le_event_ReportWithRefCounting(WifiClientPaEventId, WifiClientPaEventPtr);

            // Report event: LE_WIFICLIENT_EVENT_CONNECTED (will be deprecated)
            event = LE_WIFICLIENT_EVENT_CONNECTED;
            le_event_Report(WifiClientPaEvent, (void *)&event, sizeof(le_wifiClient_Event_t));

        }
        else if (NULL != strstr(path, "disconnected"))
        {
            LE_INFO("FOUND disconnected");

            if (LE_WIFICLIENT_BEACON_LOSS != cause)
            {
                if (NULL != strstr(path, "local request"))
                {
                    // Check WLAN interface, not available means hardware removed
                    systemResult = system(WIFI_SCRIPT_PATH COMMAND_WIFI_CHECK_HWSTATUS);

                    switch (WEXITSTATUS(systemResult))
                    {
                        case 0:
                            // WLAN interface is up, local request
                            cause = LE_WIFICLIENT_CLIENT_REQUEST;
                            break;
                        case PA_NOT_POSSIBLE:
                            // Driver removed, WiFi stop called
                            cause = LE_WIFICLIENT_HARDWARE_STOP;
                            break;
                        case PA_NOT_FOUND:
                            // WLAN interface is gone, WiFi hardware is removed
                            cause = LE_WIFICLIENT_HARDWARE_DETACHED;
                            break;
                        default:
                            LE_WARN("WiFi Client Command \"%s\" Failed: (%d)",
                                COMMAND_WIFI_CHECK_HWSTATUS, systemResult);
                            cause = LE_WIFICLIENT_CLIENT_REQUEST;

                    }
                }
                // AP terminated connection
                else if (NULL != strstr(path, "by AP"))
                {
                    cause = LE_WIFICLIENT_BY_AP;
                }
            }

            le_wifiClient_EventInd_t* WifiClientPaEventPtr = le_mem_ForceAlloc(WifiPaEventPool);
            memset(WifiClientPaEventPtr, 0, sizeof(le_wifiClient_EventInd_t));
            WifiClientPaEventPtr->event = LE_WIFICLIENT_EVENT_DISCONNECTED;
            //Disconnection cause
            WifiClientPaEventPtr->disconnectionCause = cause;
            // Retrieve WLAN interface name
            pathReentrant = path;
            ret = strtok_r(pathReentrant, " ", &pathReentrant);
            if (NULL == ret)
            {
                LE_WARN("Failed to retrieve WLAN interface");
                WifiClientPaEventPtr->ifName[0] = '\0';
            }
            else
            {
                memcpy(WifiClientPaEventPtr->ifName, ret, strlen(ret));
                WifiClientPaEventPtr->ifName[LE_WIFIDEFS_MAX_IFNAME_LENGTH] = '\0';
            }
            // Retrieve AP BSSID
            memcpy(WifiClientPaEventPtr->apBssid, apBssid, LE_WIFIDEFS_MAX_BSSID_LENGTH);
            WifiClientPaEventPtr->apBssid[LE_WIFIDEFS_MAX_BSSID_LENGTH] = '\0';

            // Report event: LE_WIFICLIENT_EVENT_DISCONNECTED
            LE_DEBUG("WiFi event: %d, disconnectCause: %d, interface: %s, bssid: %s",
                     WifiClientPaEventPtr->event,
                     WifiClientPaEventPtr->disconnectionCause,
                     WifiClientPaEventPtr->ifName,
                     WifiClientPaEventPtr->apBssid);

            // Report event: LE_WIFICLIENT_EVENT_DISCONNECTED (will be deprecated)
            event = LE_WIFICLIENT_EVENT_DISCONNECTED;
            le_event_Report(WifiClientPaEvent, (void *)&event, sizeof(le_wifiClient_Event_t));

            // Restore to default value
            cause = LE_WIFICLIENT_UNKNOWN_CAUSE;
            memset(apBssid, 0, LE_WIFIDEFS_MAX_BSSID_BYTES);
            le_event_ReportWithRefCounting(WifiClientPaEventId, WifiClientPaEventPtr);
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
 * This function must be called to initialize the PA WiFi Module.
 *
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_Init
(
    void
)
{
    LE_INFO("Init called");
    // pa_wifiClient_AddEventHandler() will be deprecated
    WifiClientPaEvent = le_event_CreateId("WifiClientPaEvent", sizeof(le_wifiClient_Event_t));
    // Create the event for signaling user handlers.
    WifiClientPaEventId = le_event_CreateIdWithRefCounting("WifiConnectEvent");
    WifiPaEventPool = le_mem_CreatePool("WifiPaEventPool", sizeof(le_wifiClient_EventInd_t));

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to release the PA WiFi Module.
 *
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_Release
(
    void
)
{
    LE_INFO("Release called");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Start WiFi Client PA
 *
 * @return LE_FAULT         The function failed.
 * @return LE_OK            The function succeeded.
 * @return LE_NOT_FOUND     The WiFi card is absent.
 * @return LE_NOT_POSSIBLE  The WiFi card may not work.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_Start
(
    void
)
{
    int systemResult;
    le_result_t result = LE_OK;

    systemResult = system(WIFI_SCRIPT_PATH COMMAND_WIFI_HW_START);
    /**
     * Returned values:
     *   0: if the interface is correctly moutned
     *  50: if WiFi card is not inserted
     * 100: if WiFi card may not work
     * 127: if driver can not be installed
     *  -1: if the fork() has failed (see man system)
     */
    // Return value of 0 means WLAN interface is up.
    if (0 == WEXITSTATUS(systemResult))
    {
        LE_DEBUG("WiFi client started correctly");

        /* Create WiFi Client PA Thread */
        WifiClientPaThread = le_thread_Create("WifiClientPaThread", WifiClientPaThreadMain, NULL);
        le_thread_SetJoinable(WifiClientPaThread);
        le_thread_AddChildDestructor(WifiClientPaThread, ThreadDestructor, NULL);
        le_thread_Start(WifiClientPaThread);
        return LE_OK;
    }
    // Return value of 50 means WiFi card is not inserted.
    else if ( PA_NOT_FOUND == WEXITSTATUS(systemResult))
    {
        LE_ERROR("WiFi card is not inserted");
        result = LE_NOT_FOUND;
    }
    // Return value of 100 means WiFi card may not work.
    else if ( PA_NOT_POSSIBLE == WEXITSTATUS(systemResult))
    {
        LE_ERROR("Unable to reset WiFi card");
        result = LE_NOT_POSSIBLE;
    }
    // WiFi card failed to start.
    else
    {
        LE_WARN("Failed to start WiFi client command \"%s\" systemResult (%d)",
                COMMAND_WIFI_HW_START, systemResult);
        result = LE_FAULT;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Stop WiFi Client PA
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_Stop
(
    void
)
{
    int systemResult = system(WIFI_SCRIPT_PATH COMMAND_WIFI_HW_STOP);
    /**
     * Returned values:
     *  0: if the interface is correctly unmounted
     * -1: if the fork() has failed (see man system)
     * 92: if unable to stop the interface
     */
    if ((!WIFEXITED(systemResult)) || (0 != WEXITSTATUS(systemResult)))
    {
        LE_ERROR("WiFi Client Command \"%s\" Failed: (%d)",
                COMMAND_WIFI_HW_STOP, systemResult);
        return LE_FAULT;
    }

    /* Terminate the created thread */
    le_thread_Cancel(WifiClientPaThread);
    if (LE_OK != le_thread_Join(WifiClientPaThread, NULL))
    {
        return LE_FAULT;
    }

    LE_DEBUG("WiFi client stopped correctly");
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function will start a scan and returns when it is done.
 * It should NOT return until the scan is done.
 * Results are read via pa_wifiClient_GetScanResult.
 * When the reading is done pa_wifiClient_ScanDone MUST be called.
 *
 * @return LE_FAULT  The function failed.
 * @return LE_BUSY   The function is already ongoing.
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_Scan
(
    void
)
{
    le_result_t result = LE_OK;

    LE_INFO("Scanning");
    if (IsScanRunning)
    {
        LE_ERROR("Scan is already running");
        return LE_BUSY;
    }

    if (NULL != IwScanPipePtr)
    {
        return LE_BUSY;
    }

    IsScanRunning = true;
    /* Open the command for reading. */
    IwScanPipePtr = popen(WIFI_SCRIPT_PATH COMMAND_WIFICLIENT_START_SCAN, "r");

    if (NULL == IwScanPipePtr)
    {
        LE_ERROR("Failed to run command \"%s\": errno:%d: \"%s\" ",
                COMMAND_WIFICLIENT_START_SCAN,
                errno,
                strerror(errno));
        result = LE_FAULT;
    }

    IsScanRunning = false;
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function is used to find out if a scan is currently running.
 *
 * @return TRUE  Scan is running.
 * @return FALSE Scan is not running
 */
//--------------------------------------------------------------------------------------------------
bool pa_wifiClient_IsScanRunning
(
    void
)
{
    return IsScanRunning;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function can be called after pa_wifi_Scan.
 * When the reading is done, it no longer returns LE_OK,
 * pa_wifiClient_ScanDone MUST be called.
 *
 * @return LE_NOT_FOUND  There is no more AP:s found.
 * @return LE_OK     The function succeeded.
 * @return LE_FAULT  The function failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_GetScanResult
(
    pa_wifiClient_AccessPoint_t *accessPointPtr,
    ///< [IN][OUT]
    ///< Structure provided by calling function.
    ///< Results filled out if result was LE_OK.
    char scanIfName[]
    ///< [IN][OUT]
    ///< Array provided by calling function.
    ///< Store WLAN interface used for scan.
)
{
    const char bssidPrefix[] = "BSS ";
    const char ssidPrefix[] = "\tSSID: ";
    const char signalPrefix[] = "\tsignal: ";
    const unsigned int bssidPrefixLen = NUM_ARRAY_MEMBERS(bssidPrefix) - 1;
    const unsigned int ssidPrefixLen = NUM_ARRAY_MEMBERS(ssidPrefix) - 1;
    const unsigned int signalPrefixLen = NUM_ARRAY_MEMBERS(signalPrefix) - 1;
    char path[PATH_MAX_BYTES];
    struct timeval tv;
    fd_set fds;
    time_t start = time(NULL);
    le_result_t ret = LE_NOT_FOUND;
    int err;
    char *retStart;
    char *retEnd;

    LE_INFO("Scan results");

    if (NULL == IwScanPipePtr)
    {
       LE_ERROR("ERROR must call pa_wifi_Scan first");
       return LE_FAULT;
    }
    if (NULL == accessPointPtr)
    {
       LE_ERROR("ERROR : accessPoint == NULL");
       return LE_BAD_PARAMETER;
    }

    /* Default values */
    accessPointPtr->signalStrength = LE_WIFICLIENT_NO_SIGNAL_STRENGTH;
    accessPointPtr->ssidLength = 0;
    memset(&accessPointPtr->ssidBytes, 0, LE_WIFIDEFS_MAX_SSID_BYTES);
    memset(&accessPointPtr->bssid, 0, LE_WIFIDEFS_MAX_BSSID_BYTES);

    /* Read the output a line at a time - output it. */
    while (IwScanPipePtr)
    {
        // Set up the timeout.  here we can wait for 1 second
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        FD_ZERO(&fds);
        FD_SET(fileno(IwScanPipePtr), &fds);
        err = select(fileno(IwScanPipePtr) + 1, &fds, NULL, NULL, &tv);
        if (!err)
        {
            LE_DEBUG("loop=%lu", time(NULL) - start);
            if ((time(NULL) - start) >= 5)
            {
                LE_WARN("Scan timeout");
                goto cleanup;
            }

            continue;
        }
        else if (err < 0)
        {
            LE_ERROR("select() failed(%d)", errno);
            goto cleanup;
        }
        else if (FD_ISSET(fileno(IwScanPipePtr), &fds))
        {
            LE_DEBUG("Read next scan result");
            if (NULL != fgets(path, sizeof(path), IwScanPipePtr))
            {
                LE_DEBUG("PARSING: '%s'", path);

                if (0 == strncmp(ssidPrefix, path, ssidPrefixLen))
                {
                    accessPointPtr->ssidLength =
                    strnlen(path, LE_WIFIDEFS_MAX_SSID_BYTES + ssidPrefixLen) - ssidPrefixLen - 1;

                    LE_DEBUG("FOUND SSID: '%s'", &path[ssidPrefixLen]);

                    memcpy(&accessPointPtr->ssidBytes, &path[ssidPrefixLen],
                           accessPointPtr->ssidLength);
                    LE_DEBUG("SSID: '%s'", &accessPointPtr->ssidBytes[0]);
                    ret = LE_OK;
                    goto cleanup;
                }
                else if (0 == strncmp(signalPrefix, path, signalPrefixLen))
                {
                    LE_DEBUG("FOUND SIGNAL STRENGTH: '%s'", &path[signalPrefixLen]);
                    accessPointPtr->signalStrength = strtol(&path[signalPrefixLen], NULL, 10);
                    LE_DEBUG("signal(%d)", accessPointPtr->signalStrength);
                }
                else if (0 == strncmp(bssidPrefix, path, bssidPrefixLen))
                {
                    LE_DEBUG("FOUND BSSID: '%s'", &path[bssidPrefixLen]);
                    memcpy(&accessPointPtr->bssid, &path[bssidPrefixLen],
                           LE_WIFIDEFS_MAX_BSSID_LENGTH);
                    LE_DEBUG("BSSID: '%s'", &accessPointPtr->bssid[0]);
                    if ('\0' == scanIfName[0])
                    {
                        if (NULL != (retStart = strstr(path, "wlan")) &&
                            NULL != (retEnd = strchr(path, ')')))
                            {
                                strncpy(scanIfName, retStart, retEnd - retStart);
                                scanIfName[LE_WIFIDEFS_MAX_IFNAME_LENGTH] = '\0';
                                LE_DEBUG("Interface: '%s'", scanIfName);
                            }

                    }
                }
            }
            else
            {
                LE_DEBUG("End of scan results");
                goto cleanup;
            }
        }
    }

cleanup:
    return ret;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called after the pa_wifiClient_Scan() has been done.
 * It signals that the scan results are no longer needed and frees some internal resources.
 *
 * @return LE_OK     The function succeeded.
 * @return LE_FAULT  The scan failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_ScanDone
(
    void
)
{
    le_result_t res = LE_OK;

    if (NULL != IwScanPipePtr)
    {
        int st = pclose(IwScanPipePtr);
        if (WIFEXITED(st))
        {
            LE_DEBUG("Scan exit status(%d)", WEXITSTATUS(st));
            res = WEXITSTATUS(st) ? LE_FAULT:LE_OK;
            if (res != LE_OK)
            {
                LE_ERROR("Scan failed(%d)", WEXITSTATUS(st));
                res = LE_FAULT;
            }
        }

        IwScanPipePtr = NULL;
        IsScanRunning = false;
    }

    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the security protocol for communication.
 *
 * @return LE_BAD_PARAMETER  The function failed due to an invalid parameter.
 * @return LE_OK             The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_SetSecurityProtocol
(
    const le_wifiClient_SecurityProtocol_t securityProtocol
        ///< [IN]
        ///< Security protocol used for communication.
)
{
    le_result_t result = LE_OK;

    LE_DEBUG("Security protocol: %d", securityProtocol);
    switch (securityProtocol)
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
 * This function generates the WPA supplicant configuration file.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t GenerateWpaSupplicant
(
    char *ssidPtr,
    const uint32_t ssidLength
)
{
    FILE    *filePtr;
    char     tmpConfig[TEMP_CONFIG_MAX_BYTES];
    char     tmpString[TEMP_STRING_MAX_BYTES];
    uint8_t  length;

    LE_DEBUG("Generate Wpa Supplicant");

    // Check SSID
    if ((NULL == ssidPtr) || (0 == ssidLength) || (ssidLength > LE_WIFIDEFS_MAX_SSID_LENGTH))
    {
        LE_ERROR("Invalid SSID");
        return LE_FAULT;
    }

    // Create the WPA supplicant file
    filePtr = fopen(WPA_SUPPLICANT_FILE, "w");
    if (filePtr == NULL)
    {
        LE_ERROR("Unable to create \"%s\" file.", WPA_SUPPLICANT_FILE);
        return LE_FAULT;
    }

    //used to store configuration to be written into wpa_supplicant.conf
    memset(tmpConfig, '\0', sizeof(tmpConfig));
    //used to store optional elements to be appended
    memset(tmpString, '\0', sizeof(tmpString));
    //common contents of wpa_supplicant.conf
    snprintf(tmpConfig, sizeof(tmpConfig), WPA_SUPPLICANT_CONFIG_COMMON,
             ssidLength, (char *)ssidPtr, HiddenAccessPoint);

    switch (SavedSecurityProtocol)
    {
        case LE_WIFICLIENT_SECURITY_NONE:
            strncat(tmpConfig, "key_mgmt=NONE\n", sizeof("key_mgmt=NONE\n"));
            break;

        case LE_WIFICLIENT_SECURITY_WEP:

            if (0 == SavedWepKey[0])
            {
                LE_ERROR("No valid WEP key");
                goto WRONG_CONFIG;
            }
            strncat(tmpConfig, "key_mgmt=NONE\n", sizeof("key_mgmt=NONE\n"));
            snprintf(tmpString, sizeof(tmpString), "wep_key0=\"%s\"\n", SavedWepKey);
            strncat(tmpConfig, tmpString, sizeof(tmpString));
            break;

        case LE_WIFICLIENT_SECURITY_WPA_PSK_PERSONAL:
        case LE_WIFICLIENT_SECURITY_WPA2_PSK_PERSONAL:

            if ((0 == SavedPassphrase[0]) && (0 == SavedPreSharedKey[0]))
            {
                LE_ERROR("No valid PassPhrase or PreSharedKey");
                goto WRONG_CONFIG;
            }
            // Passphrase is set, generate psk here
            if (0 != SavedPassphrase[0])
            {
                snprintf(tmpString, sizeof(tmpString), "psk=\"%s\"\n", SavedPassphrase);
            }
            else
            {
                snprintf(tmpString, sizeof(tmpString), "psk=%s\n", SavedPreSharedKey);
            }
            strncat(tmpConfig, tmpString, sizeof(tmpString));
            break;

        case LE_WIFICLIENT_SECURITY_WPA_EAP_PEAP0_ENTERPRISE:
        case LE_WIFICLIENT_SECURITY_WPA2_EAP_PEAP0_ENTERPRISE:

            if ((0 == SavedUsername[0]) && (0 == SavedPassword[0]))
            {
                LE_ERROR("No valid Username or Password");
                goto WRONG_CONFIG;
            }

            snprintf(tmpString, sizeof(tmpString), "identity=\"%s\"\npassword=\"%s\"\n",
                     SavedUsername,
                     SavedPassword);
            strncat(tmpConfig, tmpString, sizeof(tmpString));
            strncat(tmpConfig, "phase1=\"peaplabel=0\"\n", sizeof("phase1=\"peaplabel=1\"\n"));
            strncat(tmpConfig, "phase2=\"auth=MSCHAPV2\"\n", sizeof("phase2=\"auth=MSCHAPV2\"\n"));
            break;

        default:
            LE_ERROR("No valid Security Protocol");;
            goto WRONG_CONFIG;
    }

    //Append "}" to complete the network block
    strncat(tmpConfig, "}\n", sizeof("}\n"));

    length = strlen(tmpConfig);
    if ( length > (TEMP_CONFIG_MAX_BYTES - 1) )
    {
        goto WRONG_CONFIG;
    }

    if ( length == fwrite(&tmpConfig, 1, length, filePtr))
    {
        fclose(filePtr);
        return LE_OK;
    }

WRONG_CONFIG:

    LE_ERROR("Unable to generate the WPA supplicant file \"%s\".", WPA_SUPPLICANT_FILE);
    fclose(filePtr);
    return LE_FAULT;
}


//--------------------------------------------------------------------------------------------------
/**
 * This function connects a wifiClient.
 *
 * @return LE_FAULT             The function failed.
 * @return LE_BAD_PARAMETER     Invalid parameter.
 * @return LE_DUPLICATE         Duplicated request.
 * @return LE_TIMEOUT           Connection request time out.
 * @return LE_OK                The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_Connect
(
    uint8_t ssidBytes[LE_WIFIDEFS_MAX_SSID_BYTES],
        ///< [IN]
        ///< Contains ssidLength number of bytes
    uint8_t ssidLength
        ///< [IN]
        ///< The number of Bytes in the ssidBytes
)
{
    int         systemResult;
    char        tmpString[TEMP_STRING_MAX_BYTES];
    le_result_t result  = LE_OK;

    // Check SSID
    if (( 0 == ssidLength) || (ssidLength > LE_WIFIDEFS_MAX_SSID_LENGTH))
    {
        LE_ERROR("Invalid SSID");
        return LE_BAD_PARAMETER;
    }

    LE_INFO("Connecting over SSID length %d SSID: \"%.*s\"", ssidLength, ssidLength,
            (char *)ssidBytes);

    if (LE_OK != GenerateWpaSupplicant((char *)&ssidBytes[0], ssidLength))
    {
        return LE_BAD_PARAMETER;
    }
    memset(tmpString, '\0', sizeof(tmpString));
    strncpy(tmpString, WIFI_SCRIPT_PATH, sizeof(WIFI_SCRIPT_PATH));
    strncat(tmpString, COMMAND_WIFICLIENT_CONNECT, sizeof(COMMAND_WIFICLIENT_CONNECT));
    strncat(tmpString, WPA_SUPPLICANT_FILE, sizeof(WPA_SUPPLICANT_FILE));

    systemResult = system(tmpString);
    // Return value of -1 means that the fork() has failed (see man system).
    if (0 == WEXITSTATUS(systemResult))
    {
        LE_DEBUG("WiFi Client connected");
        result = LE_OK;
    }
    // Return value of 8 means connection time out.
    else if ( PA_TIMEOUT == WEXITSTATUS(systemResult))
    {
        LE_DEBUG("Connection time out");
        result = LE_TIMEOUT;
    }
    // Return value of 14 means wpa_supplicant is running.
    else if ( PA_DUPLICATE == WEXITSTATUS(systemResult))
    {
        LE_WARN("WPA_SUPPLICANT is running already");
        result = LE_DUPLICATE;
    }
    // Return value of -1 means that the fork() has failed (see man system).
    else
    {
        LE_ERROR("WiFi Client Command %s Failed: (%d)", tmpString, systemResult);
        result = LE_FAULT;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function disconnects a wifiClient.
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_Disconnect
(
    void
)
{
    int         systemResult;
    le_result_t result       = LE_OK;

    // Terminate connection
    systemResult = system(WIFI_SCRIPT_PATH COMMAND_WIFICLIENT_DISCONNECT);
    if (0 == WEXITSTATUS(systemResult))
    {
        LE_INFO("WiFi Client Command \"%s\" OK:", COMMAND_WIFICLIENT_DISCONNECT);
        result = LE_OK;
    }
    else
    {
        LE_ERROR("WiFi Client Command \"%s\" Failed: (%d)",
                COMMAND_WIFICLIENT_DISCONNECT, systemResult);
        result = LE_FAULT;
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Clears all username, password, PreShared Key, passphrase settings previously made by
 *
 * @see  pa_wifiClient_SetPassPhrase
 * @see  pa_wifiClient_SetPreSharedKey
 * @see  pa_wifiClient_SetUserCredentials
 *
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_ClearAllCredentials
(
    void
)
{
    memset(SavedWepKey, '\0', LE_WIFIDEFS_MAX_WEPKEY_BYTES);
    memset(SavedPassphrase, '\0', LE_WIFIDEFS_MAX_PASSPHRASE_BYTES);
    memset(SavedPreSharedKey, '\0', LE_WIFIDEFS_MAX_PSK_BYTES);
    memset(SavedUsername, '\0', LE_WIFIDEFS_MAX_USERNAME_BYTES);
    memset(SavedPassword, '\0', LE_WIFIDEFS_MAX_PASSWORD_BYTES);
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the WEP key (Wired Equivalent Privacy)
 *
 * @note WEP is unsecure and has been deprecated by the WiFi alliance. Still, it can be used in
 * user mode.
 *
 * @return LE_BAD_PARAMETER  The function failed due to an invalid parameter.
 * @return LE_OK             The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_SetWepKey
(
    const char *wepKeyPtr
        ///< [IN]
        ///< Wired Equivalent Privacy (WEP) key used for authentication
)
{
    le_result_t result = LE_BAD_PARAMETER;

    LE_DEBUG("Set WEP key");
    if (NULL != wepKeyPtr)
    {
       strncpy(&SavedWepKey[0], &wepKeyPtr[0], LE_WIFIDEFS_MAX_WEPKEY_LENGTH);
       // Make sure there is a null termination
       SavedWepKey[LE_WIFIDEFS_MAX_WEPKEY_LENGTH] = '\0';
       result = LE_OK;
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the WiFi pre-shared key (WPA-Personal)
 *
 * @see  pa_wifiClient_SetPassPhrase
 *
 * @return LE_BAD_PARAMETER  The function failed due to an invalid parameter.
 * @return LE_OK             The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_SetPreSharedKey
(
    const char *preSharedKeyPtr
        ///< [IN]
        ///< Pre-shared key (PSK) used for authentication
)
{
    le_result_t result = LE_BAD_PARAMETER;

    LE_DEBUG("Set PSK");
    if (NULL != preSharedKeyPtr)
    {
       strncpy(&SavedPreSharedKey[0], &preSharedKeyPtr[0], LE_WIFIDEFS_MAX_PSK_LENGTH);
       // Make sure there is a null termination
       SavedPreSharedKey[LE_WIFIDEFS_MAX_PSK_LENGTH] = '\0';
       // Clear the passphrase because PSK and passphrase are exlusive.
       SavedPassphrase[0] = '\0';
       result = LE_OK;
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
 */
//--------------------------------------------------------------------------------------------------
void pa_wifiClient_SetHiddenNetworkAttribute
(
    bool hidden
        ///< [IN]
        ///< If TRUE, the WIFI client will be able to connect to a hidden access point.
)
{
    LE_DEBUG("Set whether Access Point is hidden or not: %d", hidden);
    HiddenAccessPoint = hidden;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the PassPhrase used to create PSK (WPA-Personal).
 *
 * @see  pa_wifiClient_SetPreSharedKey
 *
 * @return LE_BAD_PARAMETER  The function failed due to an invalid parameter.
 * @return LE_OK             The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_SetPassphrase
(
    const char *passphrasePtr
        ///< [IN]
        ///< Passphrase used for authentication
)
{
    uint32_t length;
    // Store Passphrase to be used later during connection procedure
    le_result_t result = LE_BAD_PARAMETER;

    LE_DEBUG("Set passphrase");
    if (NULL != passphrasePtr)
    {
        length = strlen(passphrasePtr);

        LE_DEBUG("Set passphrase");
        if ((LE_WIFIDEFS_MIN_PASSPHRASE_LENGTH <= length) &&
            (length <= LE_WIFIDEFS_MAX_PASSPHRASE_LENGTH))
        {
           strncpy(&SavedPassphrase[0], &passphrasePtr[0], LE_WIFIDEFS_MAX_PASSPHRASE_LENGTH);
           // Make sure there is a null termination
           SavedPassphrase[LE_WIFIDEFS_MAX_PASSPHRASE_LENGTH] = '\0';
           // Clear the PSK because PSK and passphrase are exlusive.
           SavedPreSharedKey[0] = '\0';
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
 * Set the username and password (WPA-Entreprise).
 *
 * @return LE_BAD_PARAMETER  The function failed due to an invalid parameter.
 * @return LE_OK             The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_SetUserCredentials
(
    const char *usernamePtr,
        ///< [IN]
        ///< Username used for authentication
    const char *passwordPtr
        ///< [IN]
        ///< Password used for authentication
)
{
    // Store User Credentials to be used later during connection procedure
    LE_DEBUG("Set user credentials");
    if (NULL != usernamePtr)
    {
       strncpy(&SavedUsername[0], &usernamePtr[0], LE_WIFIDEFS_MAX_USERNAME_LENGTH);
       // Make sure there is a null termination
       SavedUsername[LE_WIFIDEFS_MAX_USERNAME_LENGTH] = '\0';
    }
    else
    {
        return LE_BAD_PARAMETER;
    }

    if (NULL != passwordPtr)
    {
       strncpy(&SavedPassword[0], &passwordPtr[0], LE_WIFIDEFS_MAX_PASSWORD_LENGTH);
       // Make sure there is a null termination
       SavedPassword[LE_WIFIDEFS_MAX_PASSWORD_LENGTH] = '\0';
    }
    else
    {
        return LE_BAD_PARAMETER;
    }
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for PA EVENT 'le_wifiClient_Event_t'
 * This event provide information on PA WiFi Client event changes.
 *
 * @deprecated pa_wifiClient_AddEventHandler() should not be used anymore.
 * It has been replaced by pa_wifiClient_AddEventIndHandler().
 *
 * @return LE_BAD_PARAMETER  The function failed due to an invalid parameter.
 * @return LE_OK             The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_AddEventHandler
(
    pa_wifiClient_NewEventHandlerFunc_t handlerPtr,
        ///< [IN]
        ///< Event handler function pointer.

    void *contextPtr
        ///< [IN]
        ///< Associated event context.
)
{
    le_event_HandlerRef_t handlerRef;

    handlerRef = le_event_AddLayeredHandler("WifiClientPaHandler",
                                    WifiClientPaEvent,
                                    FirstLayerWifiClientEventHandler,
                                    (le_event_HandlerFunc_t)handlerPtr);
    if (NULL == handlerRef)
    {
        LE_ERROR("le_event_AddLayeredHandler returned NULL");
        return LE_BAD_PARAMETER;
    }
    le_event_SetContextPtr(handlerRef, contextPtr);
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for PA EVENT 'le_wifiClient_EventInd_t'
 *
 * This event provide information on PA WiFi Client event changes.
 *
 * @return LE_BAD_PARAMETER  The function failed due to an invalid parameter.
 * @return LE_OK             The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_AddEventIndHandler
(
    pa_wifiClient_EventIndHandlerFunc_t handlerPtr,
        ///< [IN]
        ///< Event handler function pointer.

    void *contextPtr
        ///< [IN]
        ///< Associated event context.
)
{
    le_event_HandlerRef_t handlerRef;

    handlerRef = le_event_AddLayeredHandler("WifiClientPaHandler",
                                    WifiClientPaEventId,
                                    FirstLayerWifiClientEventIndHandler,
                                   (le_event_HandlerFunc_t)handlerPtr);
    if (NULL == handlerRef)
    {
        LE_ERROR("le_event_AddLayeredHandler returned NULL");
        return LE_BAD_PARAMETER;
    }
    le_event_SetContextPtr(handlerRef, contextPtr);
    return LE_OK;
}

