 /**
  * This module implements a test for Wifi Client.
  *
  * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
  *
  */

#include "legato.h"
#include "interfaces.h"


//--------------------------------------------------------------------------------------------------
//                                       Test Function
//--------------------------------------------------------------------------------------------------

#define TEST_SSID     "TestSSID"
#define TEST_SSID_NBR_BYTES     (sizeof(TEST_SSID)-1)
#define TEST_PASSPHRASE "passphrase"

#define NBR_OF_SCAN_LOOPS 1


//--------------------------------------------------------------------------------------------------
/**
 * Event handler reference.
 */
//--------------------------------------------------------------------------------------------------
static le_wifiClient_NewEventHandlerRef_t HdlrRef = NULL;
static le_wifiClient_AccessPointRef_t AccessPointRefToConnectTo = NULL;

static int ScanDoneEventCounter = 0;

//--------------------------------------------------------------------------------------------------
/**
 * Reads the Scan output
 *
 * @param event
 *        Handles the wifi events
 * @param contextPtr
 */
//--------------------------------------------------------------------------------------------------
static void TestReadScanResults
(
    void
)
{
    le_wifiClient_AccessPointRef_t accessPointRef = 0;
    int countNbrFound = 0;

    //< Wifi Scan result for available Access Points available
    LE_DEBUG( "TestReadScanResults" );
    if( NULL != (accessPointRef = le_wifiClient_GetFirstAccessPoint()) )
    {
        do
        {
            LE_DEBUG( "TestReadScanResults:le_wifiClient_GetSignalStrength %d ",
            le_wifiClient_GetSignalStrength(accessPointRef));
            uint8_t ssidBytes[LE_WIFIDEFS_MAX_SSID_BYTES];
            //< Contains ssidLength number of bytes
            size_t ssidNumElements = LE_WIFIDEFS_MAX_SSID_BYTES;


            countNbrFound++;

            LE_DEBUG( "TestReadScanResults:le_wifiClient_GetFirstAccessPoint OK");

            if( LE_OK == le_wifiClient_GetSsid( accessPointRef,
                                                &ssidBytes[0],
                                                &ssidNumElements) )
            {
                LE_DEBUG( "TestReadScanResults:le_wifiClient_GetSsid OK, ssidLength %d;"
                                    "SSID: \"%.*s\" ",
                                    ssidNumElements,
                                    ssidNumElements,
                                    (char*) &ssidBytes[0]);
        }
        else
        {
            LE_ERROR( "TestReadScanResults:le_wifiClient_GetSsid ERROR");
        }
        }while ( NULL != (accessPointRef = le_wifiClient_GetNextAccessPoint( )) );
        LE_DEBUG( "TestReadScanResults::wifi: TESTRESULT: Found %d of AccessPoints", countNbrFound);
    }
    else
    {
        LE_ERROR( "le_wifiClient_GetFirstAccessPoint ERROR");
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
    if ( NULL != AccessPointRefToConnectTo )
    {
        LE_DEBUG( " test AP valid  ");


        LE_ASSERT( LE_OK == le_wifiClient_SetSecurityProtocol( AccessPointRefToConnectTo,
                                                    LE_WIFICLIENT_SECURITY_WPA2_PSK_PERSONAL ));

        LE_ASSERT( LE_OK == le_wifiClient_SetPassphrase( AccessPointRefToConnectTo,
                                                         TEST_PASSPHRASE ));

        if( LE_OK == le_wifiClient_Connect( AccessPointRefToConnectTo ) )
        {
            LE_DEBUG( "le_wifiClient_Connect OK");
        }
        else
        {
            LE_ERROR( "le_wifiClient_Connect ERROR");
        }

    }
    else
    {
        LE_ERROR( "AccessPointRefToConnectTo  ERROR: AccessPointRefToConnectTo not found.");
    }
}
//--------------------------------------------------------------------------------------------------
/**
 * Handler for Wifi Client changes
 *
 * @param event
 *        Handles the wifi events
 * @param contextPtr
 */
//--------------------------------------------------------------------------------------------------
static void WifiClientEventHandler
(
    le_wifiClient_Event_t event,
    void* contextPtr
)
{
    LE_DEBUG( "Wifi Client event received");
    switch( event )
    {
        case LE_WIFICLIENT_EVENT_CONNECTED:
        {
            ///< Wifi Client Connected
            LE_DEBUG( "LE_WIFICLIENT_EVENT_CONNECTED");
        }
        break;

        case LE_WIFICLIENT_EVENT_DISCONNECTED:
        {
            ///< Wifi Client Disconnected
            LE_DEBUG( "LE_WIFICLIENT_EVENT_DISCONNECTED");
        }
        break;

        case LE_WIFICLIENT_EVENT_SCAN_DONE:
        {
            ScanDoneEventCounter++;
            LE_DEBUG( "LE_WIFICLIENT_EVENT_SCAN_DONE: Now read the results (ScanDoneEventCounter %d)"
            , ScanDoneEventCounter);
            TestReadScanResults();
            if( ScanDoneEventCounter < 5 )
            {
                sleep(2);
                LE_DEBUG( "LE_WIFICLIENT_EVENT_SCAN_DONE: Start New Scan %d)", ScanDoneEventCounter);
                le_wifiClient_Scan();
            }
            else
            {
                LE_DEBUG( "LE_WIFICLIENT_EVENT_SCAN_DONE: try connect. %d)", ScanDoneEventCounter);
                TestConnect();
            }
        }
        break;
        default:
            LE_ERROR( "ERROR Unknown event %d", event);
        break;
    }
}



//--------------------------------------------------------------------------------------------------
/**
 * Test: Wifi Client
 *
 */
//--------------------------------------------------------------------------------------------------
void Testle_wifiClient
(
    void
)
{
    LE_DEBUG( "Start Test Testle_wifiClient");

    // Add an handler function to handle message reception
    HdlrRef=le_wifiClient_AddNewEventHandler(WifiClientEventHandler, NULL);


    if( LE_OK == le_wifiClient_Start() )
    {
        LE_DEBUG( "le_wifiClient_Start OK");
    }
    else
    {
        LE_ERROR( "le_wifiClient_Start ERROR");
    }


    AccessPointRefToConnectTo =
        le_wifiClient_Create( (const uint8_t *) TEST_SSID, TEST_SSID_NBR_BYTES);
    LE_DEBUG( "test called le_wifiClient_Create. returned %p", AccessPointRefToConnectTo);


    LE_DEBUG( "test Scan started. Waiting X seconds for it to finish");

    if( LE_OK == le_wifiClient_Scan() )
    {
        LE_DEBUG( "le_wifiClient_Scan OK");
    }
    else
    {
        LE_DEBUG( "le_wifiClient_Scan ERROR");
    }
}



//--------------------------------------------------------------------------------------------------
/**
 * App init.
 *
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    // Wifi Init
    LE_DEBUG( "======== Wifi Client Test  ========");
    Testle_wifiClient();
}
