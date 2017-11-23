 /**
  * This module implements a test for WiFi access point
  *
  * Copyright (C) Sierra Wireless Inc.
  *
  */

#include <signal.h>
#include "legato.h"
#include "interfaces.h"

//--------------------------------------------------------------------------------------------------
//                                       Test Function Variable defines
//--------------------------------------------------------------------------------------------------
#define TEST_SSID_STR "wifiApSSID"

#define TEST_SECU_PROTO LE_WIFIAP_SECURITY_WPA2
#define TEST_PASSPHRASE "passphrase"


//--------------------------------------------------------------------------------------------------
// PREREQUISITE!!     IP HANDLING
// Please note that IP handling is not provided by the WiFi Service,
// but the following is provided as an example of how the user can setup IP.
// For the following to work the file :
//  /etc/dnsmasq.d/wifiAP.conf
// must contain the following:
// _________________
// dhcp-range=wlan0,192.168.10.10,192.168.10.199,24h
// EOF
// _________________
// Then reboot! Because this is read at startup.
//--------------------------------------------------------------------------------------------------

// IP & mask of subnet created on the wlan
#define SUBNET         "192.168.10.0/24"
#define HOST_IP        "192.168.10.1"
// IP range allotted to clients
#define IP_RANGE_START "192.168.10.10"
#define IP_RANGE_END   "192.168.10.20"


//--------------------------------------------------------------------------------------------------
//                                       Internal defines
//--------------------------------------------------------------------------------------------------

// interface of the access point (LAN - Local Area Network)
#define ITF_LAN "wlan0"
// bridge interface to access the Internet (WAN - Wide Area Network)
#define ITF_WAN "eth0"

// defines because the SSID is actually uint8 not char.
#define TEST_SSID           ((const uint8_t *)TEST_SSID_STR)
#define TEST_SSID_NBR_BYTES (sizeof(TEST_SSID_STR)-1)


//--------------------------------------------------------------------------------------------------
/**
 * Event handler reference.
 */
//--------------------------------------------------------------------------------------------------
static le_wifiAp_NewEventHandlerRef_t HdlrRef = NULL;


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
        LE_ERROR("ERROR Parameter is NULL.");
        return;
    }
    if ('\0' == *commandStringPtr)
    {
        LE_INFO("INFO Nothing to execute.");
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
 * Sets the credentials
 * and bridge the wlan0 to ethernet
 */
//--------------------------------------------------------------------------------------------------
static void Testle_startDhcpServerAndbridgeConnection
(
    void
)
{
    RunSystemCommand("ifconfig " ITF_LAN " " HOST_IP " up");

    // Turn on IP forwarding
    RunSystemCommand("echo 1 > /proc/sys/net/ipv4/ip_forward");
    // Load masquerade module
    RunSystemCommand("modprobe ipt_MASQUERADE");

    RunSystemCommand("iptables -I POSTROUTING -t nat -o " ITF_WAN " -j MASQUERADE");
    RunSystemCommand("iptables -I FORWARD --match state "
        "--state RELATED,ESTABLISHED --jump ACCEPT");
    RunSystemCommand("iptables -I FORWARD -i " ITF_LAN " --destination " SUBNET
        " --match state --state NEW --jump ACCEPT");
    RunSystemCommand("iptables -I INPUT -s " SUBNET " --jump ACCEPT");
    RunSystemCommand("iptables -I FORWARD -i " ITF_WAN " --jump ACCEPT");
    RunSystemCommand("iptables -I FORWARD -o " ITF_WAN " --jump ACCEPT");
}

//! [SetCred]
//--------------------------------------------------------------------------------------------------
/**
 * Sets the credentials
 */
//--------------------------------------------------------------------------------------------------
static void Testle_setCredentials
(
    void
)
{
    LE_ASSERT(LE_OK == le_wifiAp_SetPassPhrase(TEST_PASSPHRASE));

    LE_ASSERT(LE_OK == le_wifiAp_SetSecurityProtocol(TEST_SECU_PROTO));

    LE_ASSERT(LE_OK == le_wifiAp_SetDiscoverable(true));
}
//! [SetCred]

//! [Subscribe]
//--------------------------------------------------------------------------------------------------
/**
 * Handler for WiFi client changes
 *
 */
//--------------------------------------------------------------------------------------------------
static void myMsgHandler
(
    le_wifiAp_Event_t event,
        ///< [IN]
        ///< WiFi event to process
    void *contextPtr
        ///< [IN]
        ///< Associated event context
)
{
    LE_INFO("WiFi access point event received");
    switch(event)
    {
        case LE_WIFIAP_EVENT_CLIENT_CONNECTED:
        {
            // Client connected to AP
            LE_INFO("LE_WIFIAP_EVENT_CLIENT_CONNECTED");
        }
        break;

        case LE_WIFIAP_EVENT_CLIENT_DISCONNECTED:
        {
            // Client disconnected from AP
            LE_INFO("LE_WIFICLIENT_EVENT_DISCONNECTED");
        }
        break;


        default:
            LE_ERROR("ERROR Unknown event %d", event);
        break;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Tests the WiFi access point.
 *
 */
//--------------------------------------------------------------------------------------------------
void Testle_wifiApStart
(
    void
)
{
    LE_INFO("Start Test WiFi access point");

    // Add an handler function to handle message reception
    HdlrRef=le_wifiAp_AddNewEventHandler(myMsgHandler, NULL);

    LE_ASSERT(HdlrRef != NULL);

    LE_ASSERT(LE_OK == le_wifiAp_SetSsid(TEST_SSID, TEST_SSID_NBR_BYTES));

    Testle_setCredentials();

    if (LE_OK == le_wifiAp_Start())
    {
        LE_INFO("Start OK");

        Testle_startDhcpServerAndbridgeConnection();
    }
    else
    {
        LE_ERROR("Start ERROR");
    }

    LE_ASSERT(LE_OK == le_wifiAp_SetIpRange(HOST_IP, IP_RANGE_START, IP_RANGE_END));
}
//! [Subscribe]


//--------------------------------------------------------------------------------------------------
/**
 * Stop the WiFi AP
 */
//--------------------------------------------------------------------------------------------------
static void Testle_wifiApStop
(
    int signalId
)
{
    LE_INFO("WIFI AP STOP : Received signal %d", signalId);

    // Stop the AP
    le_wifiAp_Stop();

    // Turn off IP forwarding
    LE_INFO("WIFI AP STOP - Disabling IP forwarding");
    RunSystemCommand("echo 0 > /proc/sys/net/ipv4/ip_forward");
    // Removing masquerade modules
    LE_INFO("WIFI AP STOP - Removing the masquerading module...");
    RunSystemCommand("rmmod ipt_MASQUERADE");

    // Turn off the iptables
    RunSystemCommand("iptables -t nat -f");
    RunSystemCommand("iptables -t mangle -F");
    RunSystemCommand("iptables -F");
    RunSystemCommand("iptables -X");


    // Flush the IP address of the wlan0 interface
    RunSystemCommand("ip addr flush dev " ITF_LAN);
}

//--------------------------------------------------------------------------------------------------
/**
 * App init.
 *
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    // WiFi Init
    LE_INFO("======== WiFi AP Test  ========");

    putenv("PATH=/legato/systems/current/bin:/usr/local/bin:"
        "/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin");

    signal(SIGINT, Testle_wifiApStop);
    signal(SIGTERM, Testle_wifiApStop);

    Testle_wifiApStart();
}
