// -------------------------------------------------------------------------------------------------
/**
 *  PA for WiFi Access Point
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
static char SavedSsid[LE_WIFIDEFS_MAX_SSID_BYTES];

//--------------------------------------------------------------------------------------------------
/**
 * Defines whether the SSID is hidden or not
 */
//--------------------------------------------------------------------------------------------------
static bool     SavedDiscoverable  = true;
//--------------------------------------------------------------------------------------------------
/**
 * The WiFi channel associated with the SSID
 */
//--------------------------------------------------------------------------------------------------
static uint32_t SavedChannelNumber = 0;
//--------------------------------------------------------------------------------------------------
/**
 * The maximum numbers of clients the AP is able to manage
 */
//--------------------------------------------------------------------------------------------------
static uint32_t SavedMaxNumClients = 10;

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
 * WifiAp state event ID used to report WifiAp state events to the registered event handlers.
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t WifiApPaEvent;

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


// SIMU variable for timers
static le_timer_Ref_t SimuClientConnectTimer    = NULL;
static le_timer_Ref_t SimuClientDisconnectTimer = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * This function generates a WiFi Access Point Event if callback event handler is set.
 */
//--------------------------------------------------------------------------------------------------
static void GenerateApEvent
(
    le_wifiAp_Event_t event
)
{
    LE_INFO("Event: %d ", event);
    le_event_Report(WifiApPaEvent , (void *)&event, sizeof(le_wifiAp_Event_t));
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
 * SIMULATION! Simulates a LE_WIFIAP_EVENT_CLIENT_DISCONNECTED event
 */
//--------------------------------------------------------------------------------------------------
static void SimuClientDisconnectTimeCallBack
(
    le_timer_Ref_t timerRef
)
{
    GenerateApEvent(LE_WIFIAP_EVENT_CLIENT_DISCONNECTED);
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

    //repeat "repeat" times
    if (LE_OK != le_timer_SetRepeat(SimuClientConnectTimer, repeat))
    {
        LE_ERROR("ERROR: StartApSimulation le_timer_SetInterval failed.");
    }

    if (LE_OK != le_timer_Start(SimuClientConnectTimer))
    {
        LE_ERROR("ERROR: StartApSimulation le_timer_SetInterval failed.");
    }
}

//--------------------------------------------------------------------------------------------------
/**
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

    SimuClientDisconnectTimer = le_timer_Create("WiFi Simu AP DisConnect Timer");

    LE_INFO("Timer: %p", SimuClientDisconnectTimer);

    if (LE_OK != le_timer_SetHandler(SimuClientDisconnectTimer, SimuClientDisconnectTimeCallBack))
    {
        LE_ERROR("ERROR: le_timer_SetHandler failed.");
    }
    if (LE_OK != le_timer_SetInterval(SimuClientDisconnectTimer, interval))
    {
        LE_ERROR("ERROR: le_timer_SetInterval failed.");
    }

    //repeat "repeat" times
    if (LE_OK != le_timer_SetRepeat(SimuClientDisconnectTimer, repeat))
    {
        LE_ERROR("ERROR: le_timer_SetRepeat failed.");
    }

    if (LE_OK != le_timer_Start(SimuClientDisconnectTimer))
    {
        LE_ERROR("ERROR: le_timer_Start failed.");
    }
}

//--------------------------------------------------------------------------------------------------
/**
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
    LE_INFO("AP init called");
    // Create the event for signaling user handlers.
    WifiApPaEvent = le_event_CreateId("WifiApPaEvent", sizeof(le_wifiAp_Event_t));

    if (NULL != le_event_CreateId)
    {
        return LE_OK;
    }
    else
    {
        return LE_FAULT;
    }
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

    LE_INFO("Simulated SSID: \"%s\"", (char *)SavedSsid);

    StartApSimulation();

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function stops the WiFi Access Point.
 *

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
    if (NULL != handlerRef)
    {
        LE_INFO("ERROR: le_event_AddLayeredHandler returned NULL");
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

    LE_INFO("SSID length %d SSID: \"%.*s\"",
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

    LE_INFO("Security protocol: %d", securityProtocol);
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
    LE_INFO("Set maximum clients");
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

            systemResult = system("/etc/init.d/dnsmasq stop; /etc/init.d/dnsmasq start");
            if (0 != WEXITSTATUS (systemResult))
            {
                LE_ERROR("Unable to restart the DHCP server.");
                return LE_FAULT;
            }
        }
    }

    return LE_OK;
}
