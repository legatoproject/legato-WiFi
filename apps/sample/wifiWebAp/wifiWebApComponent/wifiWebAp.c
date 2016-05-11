 /**
  * This module implements a Web interface for setting up the Wifi Access Point
  *
  * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
  *
  * It starts the webserver and
  * it subscribes to the Wifi Access Point events and logs them to a logfile.
  * The page index.html provides an interface page with two parts.
  *   The first part is an interface to setup the Wifi Access Point.
  *   The second part is the content of the logfile provided by this app.
  */

#include <signal.h>
#include "legato.h"
#include "interfaces.h"

#define   HTTP_PORT_NUMBER  "8080"
#define   HTTP_SYS_CMD  "/usr/sbin/httpd -v -p " HTTP_PORT_NUMBER " -u root -h /legato/systems/current/apps/wifiWebAp/read-only/var/www/ 2>&1"

#define LOGFILE "/tmp/wifi_http.log"
//#define LOGFILE "/legato/systems/current/apps/wifiWebAp/read-only/tmp/wifi_http.log"


#define TMP_STR_SIZE 256


static FILE *HttpdCmdFp = NULL;
static FILE *LogFileFp = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Event handler reference.
 */
//--------------------------------------------------------------------------------------------------
static le_wifiAp_NewEventHandlerRef_t HdlrRef = NULL;


//--------------------------------------------------------------------------------------------------
/**
 * The number of clients connected to the Wifi Access point (based on Connect/Disconnect events)
 */
//--------------------------------------------------------------------------------------------------
static uint16_t NumberClients = 0;

//--------------------------------------------------------------------------------------------------
/**
 * Handler for Wifi Client changes
 *
 * @param event
 *        Handles the wifi events
 * @param contextPtr
 */
//--------------------------------------------------------------------------------------------------
static void myMsgHandler
(
    le_wifiAp_Event_t event,
    void* contextPtr
)
{
    char str[TMP_STR_SIZE];
    LE_INFO( "Wifi Ap event received");

    LogFileFp = fopen( LOGFILE, "a");

    if( LogFileFp == NULL )
    {
        LE_ERROR( "fopen failed for " LOGFILE ":  errno:%d %s",
                        errno,
                        strerror( errno ));
    }

    switch( event )
    {
        case LE_WIFIAP_EVENT_CLIENT_CONNECTED:
        {
            NumberClients++;
            if( LogFileFp != NULL )
            {
                ///< A client connect to AP
                snprintf(str, TMP_STR_SIZE, "LE_WIFIAP_EVENT_CLIENT_CONNECTED: Total Clients Connected: %d</br>\r\n", NumberClients);
                LE_INFO( "%s", str );
                fwrite(str , 1 , strlen(str) , LogFileFp );
            }
        }
        break;

        case LE_WIFIAP_EVENT_CLIENT_DISCONNECTED:
        {
            ///< A client disconnect from AP
            // Check the number of clients because sometimes a spurious disconnection event may occur.
            if (NumberClients > 0)
            {
                NumberClients--;
            }
            if( LogFileFp != NULL )
            {
                snprintf(str, TMP_STR_SIZE, "LE_WIFICLIENT_EVENT_DISCONNECTED: Total Clients Connected: %d</br>\r\n", NumberClients);
                LE_INFO(  "%s", str );
                fwrite(str , 1 , strlen(str) , LogFileFp );
            }
        }
        break;


        default:
            LE_ERROR( "ERROR Unknown event %d", event);
        break;
    }

    if( LogFileFp != NULL )
    {
        fclose(LogFileFp);
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
    LE_INFO( "SubscribeApEvents");
    // todo: Clear log file at startup?

    // Add an handler function to handle message reception
    HdlrRef=le_wifiAp_AddNewEventHandler( myMsgHandler, NULL );
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
    char tmpString[] = HTTP_SYS_CMD;
    char str[1024];

    LE_INFO( "StartWebServer : popen : " HTTP_SYS_CMD );
    HttpdCmdFp = popen( tmpString, "r" );
    LE_INFO( "StartWebServer : AFTER (command did not hang) %p", HttpdCmdFp);
    if ( NULL == HttpdCmdFp )
    {
        LE_ERROR( "StartWebServer: Failed to run command:\"%s\" errno:%d %s",
                        (tmpString),
                        errno,
                        strerror( errno ));
        LE_ERROR( "StartWebServer:  errno:%d %s",
                        errno,
                        strerror( errno ));

        return;
    }

    // Read the output a line at a time - output it.
    while ( NULL != fgets( str, sizeof(str)-1, HttpdCmdFp ) )
    {
        LE_INFO( "PARSING:%s: len:%d", str, (int) strnlen( str,sizeof(str)-1 ) );
    }


    if ( NULL != HttpdCmdFp )
    {
        pclose( HttpdCmdFp );
    }

    // This clears and starts the log file.
    LogFileFp = fopen( LOGFILE, "w");

    if( LogFileFp != NULL )
    {
        snprintf(str, TMP_STR_SIZE, "STARTING WIFI HTTP INTERFACE\n");
        LE_INFO(  "%s", str );
        fwrite(str , 1 , strlen(str) , LogFileFp );
        fclose(LogFileFp);
    }
    else
    {
        LE_ERROR( "ERROR: fopen failed for " LOGFILE ":  errno:%d %s",
                        errno,
                        strerror( errno ));
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * runs the command
 */
//--------------------------------------------------------------------------------------------------
static void RunSystemCommand
(
    char * commandString
)
{
    int16_t systemResult;

    if( NULL == commandString )
    {
        LE_ERROR( "RunSystemCommand ERROR Parameter is NULL" );
        return;
    }

    systemResult = system( commandString );
    // Return value of -1 means that the fork() has failed (see man system).
    if ( 0 == WEXITSTATUS( systemResult ) )
    {
        LE_INFO("RunSystemCommand Success: %s", commandString);
    }
    else
    {
        LE_ERROR( "RunSystemCommand Error %s Failed: (%d)", commandString, systemResult );
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
    LE_INFO( "StopWebServer : Received signal %d", signalId );
    LE_INFO( "StopWebServer : Killing of instance of httpd server" );
    RunSystemCommand( "killall httpd" );

    // Stop the AP
    le_wifiAp_Stop();

    // Turn off IP forwarding
    LE_INFO( "StopWebServer - Disabling IP forwarding" );
    RunSystemCommand("echo 0 > /proc/sys/net/ipv4/ip_forward");
    // Removing masquerade modules
    LE_INFO( "StopWebServer - Removing the masquerading module..." );
    RunSystemCommand("modprobe ipt_MASQUERADE");

    // Turn off the iptables
    RunSystemCommand( "iptables -t nat -f" );
    RunSystemCommand( "iptables -t mangle -F" );
    RunSystemCommand( "iptables -F" );
    RunSystemCommand( "iptables -X" );
}

//--------------------------------------------------------------------------------------------------
/**
 * App init.
 *
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    LE_INFO( "======== Wifi Web Ap ======== on port " HTTP_PORT_NUMBER);

    // Set the environment
    putenv("PATH=/legato/systems/current/bin:/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin");

    // subscribes to Access Points events and logs them.
    SubscribeApEvents();

    signal(SIGINT, StopWebServer);
    signal(SIGTERM, StopWebServer);

    // Config interface is handled in CGI webscript.
    StartWebServer();
}
