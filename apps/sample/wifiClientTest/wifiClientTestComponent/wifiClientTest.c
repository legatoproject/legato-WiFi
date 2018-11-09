/**
 * This module implements a test for WiFi client.
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"
#include "interfaces.h"


//--------------------------------------------------------------------------------------------------
//                                       Test Function
//--------------------------------------------------------------------------------------------------
#define TEST_SSID           "ExampleSSID"
#define TEST_SSID_NBR_BYTES (sizeof(TEST_SSID)-1)
#define TEST_PASSPHRASE     "passphrase"

#define NBR_OF_SCAN_LOOPS   2
#define NBR_OF_PINGS        "5"

//--------------------------------------------------------------------------------------------------
/**
 * Event handler reference.
 */
//--------------------------------------------------------------------------------------------------
static le_wifiClient_NewEventHandlerRef_t HdlrRef = NULL;
//--------------------------------------------------------------------------------------------------
/**
 * Access point reference.
 */
//--------------------------------------------------------------------------------------------------
static le_wifiClient_AccessPointRef_t AccessPointRefToConnectTo = NULL;
//--------------------------------------------------------------------------------------------------
/**
 * WiFi scans counter
 */
//--------------------------------------------------------------------------------------------------
static uint32_t ScanDoneEventCounter = 0;


//--------------------------------------------------------------------------------------------------
/**
 * IP Handling must be done by the application once the WiFi link is established
 */
//--------------------------------------------------------------------------------------------------
static void AskForIpAddress
(
    void
)
{
    int  systemResult;
    char tmpString[512];

    // DHCP client
    snprintf(tmpString,
        sizeof(tmpString),
        "PATH=/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin;"
        "/sbin/udhcpc -R -b -i wlan0"
   );

    systemResult = system(tmpString);
    // Return value of -1 means that the fork() has failed (see man system).
    if (0 == WEXITSTATUS(systemResult))
    {
        LE_INFO("DHCP client: %s", tmpString);
    }
    else
    {
        LE_ERROR("DHCP client %s Failed: (%d)", tmpString, systemResult);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * To test the established IP link we will try to pings Googles DNS server 8.8.8.8
 */
//--------------------------------------------------------------------------------------------------
static void TestToPingGooglesDNS
(
    void
)
{
    int  systemResult;
    char tmpString[512];

    // PING
    snprintf(tmpString, sizeof(tmpString), "ping -c " NBR_OF_PINGS " 8.8.8.8");

    LE_INFO("pinging 8.8.8.8 5x times: %s", tmpString);
    systemResult = system(tmpString);
    // Return value of -1 means that the fork() has failed (see man system).
    if (0 == WEXITSTATUS(systemResult))
    {
        LE_INFO("Ping: %s", tmpString);
    }
    else
    {
        LE_ERROR("Ping %s Failed: (%d)", tmpString, systemResult);
    }
}
//--------------------------------------------------------------------------------------------------
/**
 * Reads the Scan output
 */
//--------------------------------------------------------------------------------------------------
static void TestReadScanResults
(
    void
)
{
    le_wifiClient_AccessPointRef_t accessPointRef = 0;
    uint32_t                       countNbrFound  = 0;

    //< WiFi Scan result for available access points available
    LE_DEBUG("Scan results");
    if (NULL != (accessPointRef = le_wifiClient_GetFirstAccessPoint()))
    {
        do
        {
            // SSID container
            uint8_t ssidBytes[LE_WIFIDEFS_MAX_SSID_BYTES];
            // SSID length in bytes
            size_t ssidNumElements = LE_WIFIDEFS_MAX_SSID_BYTES;

            LE_DEBUG("le_wifiClient_GetSignalStrength %d ",
                le_wifiClient_GetSignalStrength(accessPointRef));

            countNbrFound++;

            LE_DEBUG("le_wifiClient_GetFirstAccessPoint OK");

            if (LE_OK == le_wifiClient_GetSsid(accessPointRef, &ssidBytes[0], &ssidNumElements))
            {
                LE_DEBUG("le_wifiClient_GetSsid OK, ssidLength %d;"
                    "SSID: \"%.*s\" ",
                    (int)ssidNumElements,
                    (int)ssidNumElements,
                    (char *)&ssidBytes[0]);
            }
            else
            {
                LE_ERROR("le_wifiClient_GetSsid ERROR");
            }
        } while (NULL != (accessPointRef = le_wifiClient_GetNextAccessPoint()));
        LE_DEBUG("WiFi: TESTRESULT: Found %d of AccessPoints", countNbrFound);
    }
    else
    {
        LE_ERROR("le_wifiClient_GetFirstAccessPoint ERROR");
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Starts a connection to the AccessPointRefToConnectTo previously found in
 * TestReadScanResults()
 */
//--------------------------------------------------------------------------------------------------
static void TestConnect
(
    void
)
{
    // the AccessPointRefToConnectTo should still be valid after a Scan (if this will fail)
    if (NULL != AccessPointRefToConnectTo)
    {
        LE_DEBUG(" test AP valid  ");


        LE_ASSERT(LE_OK == le_wifiClient_SetSecurityProtocol(AccessPointRefToConnectTo,
            LE_WIFICLIENT_SECURITY_WPA2_PSK_PERSONAL));

        LE_ASSERT(LE_OK == le_wifiClient_SetPassphrase(AccessPointRefToConnectTo,
            TEST_PASSPHRASE));

        if (LE_OK == le_wifiClient_Connect(AccessPointRefToConnectTo))
        {
            LE_DEBUG("Connect OK");
        }
        else
        {
            LE_ERROR("Connect ERROR");
        }

    }
    else
    {
        LE_ERROR("AccessPointRefToConnectTo  ERROR: AccessPointRefToConnectTo not found.");
    }
}
//--------------------------------------------------------------------------------------------------
/**
 * Handler for WiFi client changes
 */
//--------------------------------------------------------------------------------------------------
static void WifiClientEventHandler
(

    le_wifiClient_Event_t event,
        ///< [IN]
        ///< WiFi event to process
    void *contextPtr
        ///< [IN]
        ///< Associated WiFi event context
)
{
    LE_DEBUG("WiFi client event received");
    switch(event)
    {
        case LE_WIFICLIENT_EVENT_CONNECTED:
        {
            ///< WiFi Client Connected
            LE_DEBUG("LE_WIFICLIENT_EVENT_CONNECTED");

            AskForIpAddress();

            TestToPingGooglesDNS();

        }
        break;

        case LE_WIFICLIENT_EVENT_DISCONNECTED:
        {
            ///< WiFi client Disconnected
            LE_DEBUG("LE_WIFICLIENT_EVENT_DISCONNECTED");
        }
        break;

        case LE_WIFICLIENT_EVENT_SCAN_DONE:
        {
            ScanDoneEventCounter++;
            LE_DEBUG("LE_WIFICLIENT_EVENT_SCAN_DONE: "
                "Now read the results (ScanDoneEventCounter %d)",
                ScanDoneEventCounter);
            TestReadScanResults();
            if (ScanDoneEventCounter < NBR_OF_SCAN_LOOPS)
            {
                sleep(2);
                LE_DEBUG("LE_WIFICLIENT_EVENT_SCAN_DONE: Start New Scan %d)", ScanDoneEventCounter);
                le_wifiClient_Scan();
            }
            else
            {
                LE_DEBUG("LE_WIFICLIENT_EVENT_SCAN_DONE: try connect. %d)", ScanDoneEventCounter);
                TestConnect();
            }
        }
        break;
        default:
            LE_ERROR("ERROR Unknown event %d", event);
        break;
    }
}



//--------------------------------------------------------------------------------------------------
/**
 * Test: WiFi client
 */
//--------------------------------------------------------------------------------------------------
void Testle_wifiClient
(
    void
)
{
    LE_DEBUG("Start Test");

    // Add an handler function to handle message reception
    HdlrRef = le_wifiClient_AddNewEventHandler(WifiClientEventHandler, NULL);

    if (LE_OK == le_wifiClient_Start())
    {
        LE_DEBUG("Start OK");
    }
    else
    {
        LE_ERROR("Start ERROR");
    }

    AccessPointRefToConnectTo = le_wifiClient_Create((const uint8_t *)TEST_SSID,
        TEST_SSID_NBR_BYTES);
    LE_DEBUG("test called le_wifiClient_Create. returned %p", AccessPointRefToConnectTo);

    LE_DEBUG("test Scan started. Waiting X seconds for it to finish");

    if (LE_OK == le_wifiClient_Scan())
    {
        LE_DEBUG("Scan OK");
    }
    else
    {
        LE_ERROR("Scan ERROR");
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * App init.
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    // WiFi Init
    LE_DEBUG("======== WiFi Client Test  ========");
    Testle_wifiClient();
}
