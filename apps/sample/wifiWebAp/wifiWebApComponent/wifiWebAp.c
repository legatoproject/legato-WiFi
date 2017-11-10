 /**
  * This module implements a Web interface for setting up the WiFi access point
  *
  * Copyright (C) Sierra Wireless Inc.
  *
  * It starts the webserver and
  * it subscribes to the WiFi access point events and logs them to a logfile.
  * The page index.html provides an interface page with two parts.
  *   The first part is an interface to setup the WiFi access point.
  *   The second part is the content of the logfile provided by this app.
  */

#include <time.h>
#include <signal.h>
#include "legato.h"
#include "interfaces.h"

#define LOGFILE                "/tmp/wifi_http.log"
#define BUF_SIZE               256
#define STR_SIZE               1024
#define HTTP_PORT_NUMBER       "8080"
#define HTTP_SYS_CMD           "/usr/sbin/httpd -v -p " HTTP_PORT_NUMBER \
    " -u root -h /legato/systems/current/apps/wifiWebAp/read-only/var/www/ 2>&1"
#define HTTP_CONNECTION_REPORT "<font color=\"black\" >%s:</font>" \
    " Total clients connected: %d</br>\r\n"

static FILE *HttpdCmdPipePtr = NULL;
static FILE *LogFilePipePtr  = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Event handler reference.
 */
//--------------------------------------------------------------------------------------------------
static le_wifiAp_NewEventHandlerRef_t HdlrRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * The number of clients connected to the WiFi Access point (based on Connect/Disconnect events)
 */
//--------------------------------------------------------------------------------------------------
static uint32_t NumberClients = 0;

//--------------------------------------------------------------------------------------------------
/**
 * Log WiFi events in the given file
 */
//--------------------------------------------------------------------------------------------------
static le_result_t WifiEventLog
(
    const char *data,
    FILE *file
)
{
    size_t length;

    if (file == NULL)
    {
        LE_ERROR("Invalid file handler");
        return LE_BAD_PARAMETER;
    }
    if (data == NULL)
    {
        LE_ERROR("Invalid data parameter");
        return LE_BAD_PARAMETER;
    }
    if (*data == '\0')
    {
        LE_INFO("Nothing to log");
        return LE_OK;
    }
    length = strlen(data);
    if (length != fwrite(data, 1, length, file))
    {
        LE_ERROR("Write operation failed.");
        return LE_FAULT;
    }

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Handler for WiFi events
 */
//--------------------------------------------------------------------------------------------------
static void WifiEventHandler
(
    le_wifiAp_Event_t event,
        ///< [IN]
        ///< WiFi event to process
    void *contextPtr
        ///< [IN]
        ///< Associated WiFi event context
)
{
    char   str[BUF_SIZE];
    char   buffer[BUF_SIZE];
    time_t timestamp        = time(NULL);
    struct tm tmp = {0};

    LE_INFO("WiFi Ap event received");

    LogFilePipePtr = fopen(LOGFILE, "a");

    if (LogFilePipePtr == NULL)
    {
        LE_ERROR("fopen failed for " LOGFILE ":  errno:%d %s", errno, strerror(errno));
        return;
    }

    if (localtime_r(&timestamp, &tmp) == NULL)
    {
        LE_ERROR("Cannot convert Absolute time into local time.");
    }

    strftime(buffer, sizeof(buffer), "%H:%M:%S", &tmp);

    switch (event)
    {
        case LE_WIFIAP_EVENT_CLIENT_CONNECTED:
        {
            NumberClients++;
            if (LogFilePipePtr != NULL)
            {
                ///< A client connect to AP
                snprintf(str, BUF_SIZE, HTTP_CONNECTION_REPORT, buffer, NumberClients);
                LE_INFO("%s", str);
                WifiEventLog(str, LogFilePipePtr);
            }
        }
        break;

        case LE_WIFIAP_EVENT_CLIENT_DISCONNECTED:
        {
            ///< A client disconnect from AP
            // Check the number of clients because sometimes
            // a spurious disconnection event may occur.
            if (NumberClients > 0)
            {
                NumberClients--;
            }
            if (LogFilePipePtr != NULL)
            {
                snprintf(str, BUF_SIZE, HTTP_CONNECTION_REPORT, buffer, NumberClients);
                LE_INFO("%s", str);
                WifiEventLog(str, LogFilePipePtr);
            }
        }
        break;

        default:
            LE_ERROR("ERROR Unknown event %d", event);
        break;
    }

    if (LogFilePipePtr != NULL)
    {
        fclose(LogFilePipePtr);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Starts the HTTP server.
 * All AP configuration is done in the webscript.
 *
 */
//--------------------------------------------------------------------------------------------------
static void SubscribeApEvents
(
    void
)
{
    LE_INFO("SubscribeApEvents");
    // todo: Clear log file at startup?

    // Add an handler function to handle message reception
    HdlrRef = le_wifiAp_AddNewEventHandler(WifiEventHandler, NULL);
    LE_ASSERT(HdlrRef != NULL);
}

//--------------------------------------------------------------------------------------------------
/**
 * Starts the HTTP server.
 * All AP configuration is done in the webscript.
 *
 */
//--------------------------------------------------------------------------------------------------
static void StartWebServer
(
    void
)
{
    char tmpString[]   = HTTP_SYS_CMD;
    char str[STR_SIZE];
    struct tm tmp = {0};

    LE_INFO("popen : " HTTP_SYS_CMD);
    HttpdCmdPipePtr = popen(tmpString, "r");
    LE_INFO("AFTER (command did not hang) %p", HttpdCmdPipePtr);
    if (NULL == HttpdCmdPipePtr)
    {
        LE_ERROR("Failed to run command:\"%s\" errno:%d %s",
            (tmpString),
            errno,
            strerror(errno));
        LE_ERROR("errno:%d %s", errno, strerror(errno));

        return;
    }

    // Read the output a line at a time - output it.
    while (NULL != fgets(str, sizeof(str) - 1, HttpdCmdPipePtr))
    {
        LE_INFO("PARSING:%s: len:%d", str, (int)strnlen(str, sizeof(str) - 1));
    }

    if (NULL != HttpdCmdPipePtr)
    {
        pclose(HttpdCmdPipePtr);
    }

    // This clears and starts the log file.
    LogFilePipePtr = fopen(LOGFILE, "w");

    if (LogFilePipePtr != NULL)
    {
        char   buffer[BUF_SIZE];
        time_t timestamp        = time(NULL);

        LE_INFO("STARTING WIFI HTTP INTERFACE");

        if (localtime_r(&timestamp, &tmp) == NULL)
        {
            LE_ERROR("Cannot convert Absolute time into local time.");
        }

        strftime(buffer, sizeof(buffer), "%H:%M:%S", &tmp);
        snprintf(str,
            BUF_SIZE,
            "<font color=\"black\" >%s:</font> Starting WiFi HTTP interface...</br>\r\n",
            buffer);
        WifiEventLog(str, LogFilePipePtr);
        fclose(LogFilePipePtr);
    }
    else
    {
        LE_ERROR("ERROR: fopen failed for " LOGFILE ":  errno:%d %s", errno, strerror(errno));
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * runs the command
 */
//--------------------------------------------------------------------------------------------------
static void RunSystemCommand
(
    char *commandStringPtr
)
{
    int systemResult;

    if (NULL == commandStringPtr)
    {
        LE_ERROR("ERROR Parameter is NULL");
        return;
    }

    systemResult = system(commandStringPtr);
    // Return value of -1 means that the fork() has failed (see man system).
    if (0 == WEXITSTATUS(systemResult))
    {
        LE_INFO("Success: %s", commandStringPtr);
    }
    else
    {
        LE_ERROR("Error %s Failed: (%d)", commandStringPtr, systemResult);
    }

}

//--------------------------------------------------------------------------------------------------
/**
 * Stops the HTTP server.
 *
 */
//--------------------------------------------------------------------------------------------------
static void StopWebServer
(
    int signalId
)
{
    LE_INFO("StopWebServer : Received signal %d", signalId);
    LE_INFO("StopWebServer : Killing of instance of httpd server");
    RunSystemCommand("killall httpd");

    // Stop the AP
    le_wifiAp_Stop();

    // Turn off IP forwarding
    LE_INFO("Disabling IP forwarding");
    RunSystemCommand("echo 0 > /proc/sys/net/ipv4/ip_forward");
    // Removing masquerade modules
    LE_INFO("Removing the masquerading module...");
    RunSystemCommand("modprobe ipt_MASQUERADE");

    // Turn off the iptables
    RunSystemCommand("iptables -t nat -f");
    RunSystemCommand("iptables -t mangle -F");
    RunSystemCommand("iptables -F");
    RunSystemCommand("iptables -X");
}

//--------------------------------------------------------------------------------------------------
/**
 * App init.
 *
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    int rc;

    LE_INFO("======== WiFi Web Ap ======== on port " HTTP_PORT_NUMBER);

    // Set the environment
    putenv("PATH=/legato/systems/current/bin:"
        "/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin");

    tzset();

    // subscribes to access points events and logs them.
    SubscribeApEvents();

    signal(SIGINT, StopWebServer);
    signal(SIGTERM, StopWebServer);

    rc = system("/usr/sbin/iptables -A INPUT -m udp -p udp --dport 8080:8080 -j ACCEPT");
    if (WEXITSTATUS(rc))
    {
        LE_ERROR("iptables fails: %x", rc);
    }
    rc = system("/usr/sbin/iptables -A INPUT -p tcp --sport 1024:65535"
                " --dport 8080:8080 -j ACCEPT");
    if (WEXITSTATUS(rc))
    {
        LE_ERROR("iptables fails: %x", rc);
    }
    rc = system("/usr/sbin/iptables -A OUTPUT -p tcp --sport 8080:8080"
                " --dport 1024:65535 -j ACCEPT");
    if (WEXITSTATUS(rc))
    {
        LE_ERROR("iptables fails: %x", rc);
    }
    rc = system("/usr/sbin/iptables -A INPUT -m udp -p udp --sport 67:68 --dport 67:68 -j ACCEPT");
    if (WEXITSTATUS(rc))
    {
        LE_ERROR("iptables fails: %x", rc);
    }

    // Config interface is handled in CGI webscript.
    StartWebServer();
}
