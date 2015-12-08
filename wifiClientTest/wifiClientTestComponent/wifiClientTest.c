 /**
  * This module implements a test for GNSS device starting.
  *
  * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
  *
  */

#include "legato.h"
#include "interfaces.h"


//--------------------------------------------------------------------------------------------------
//                                       Test Function
//--------------------------------------------------------------------------------------------------

#define TEST_SSID     "GustavNet"
#define TEST_SSID_NBR_BYTES     (sizeof(TEST_SSID)-1)
#define TEST_PASSWORD "sten123456789"
#define WIFIDEBUG     "WIFI TEST DEBUG:"

#define NBR_OF_SCAN_LOOPS 1


//--------------------------------------------------------------------------------------------------
/**
 * Event handler reference.
 */
//--------------------------------------------------------------------------------------------------
static le_wifiClient_NewEventHandlerRef_t HdlrRef = NULL;
static le_wifiClient_AccessPointRef_t AccessPointRefToConnectTo = NULL;

static bool ScanDoneEventCounter = 0;

//--------------------------------------------------------------------------------------------------
/**
 * Reads the Scan output
 *
 * @param event
 *        Handles the wifi events
 * @param contextPtr
 */
//--------------------------------------------------------------------------------------------------
static void MyReadScanResults
(
    void
)
{
    le_wifiClient_AccessPointRef_t accessPointRef = 0;
    int countNbrFound = 0;

    ///< Wifi Scan result for available Access Points available
    LE_INFO( WIFIDEBUG "LE_WIFICLIENT_EVENT_SCAN_DONE");
    if( NULL != (accessPointRef = le_wifiClient_GetFirstAccessPoint()) )
    {
        do
        {
            LE_INFO( WIFIDEBUG "le_wifiClient_GetSignalStrength %d ",
            le_wifiClient_GetSignalStrength(accessPointRef));
            uint8_t ssidBytes[LE_WIFIDEFS_MAX_SSID_BYTES];
            ///< Contains ssidLength number of bytes
            size_t ssidNumElements = LE_WIFIDEFS_MAX_SSID_BYTES;
            ///< The number of Bytes in the ssidBytes
            uint8_t ssidBytesToLookFor[] = TEST_SSID;
            size_t   memCmpLenght  = 8;

            countNbrFound++;

            LE_INFO( WIFIDEBUG "le_wifiClient_GetFirstAccessPoint OK");

            if( LE_OK == le_wifiClient_GetSsid( accessPointRef,
                                                &ssidBytes[0],
                                                &ssidNumElements))
            {
                LE_INFO( WIFIDEBUG "le_wifiClient_GetSsid OK, ssidLength %d;"
                                    "SSID: \"%.*s\" ",
                                    ssidNumElements,
                                    ssidNumElements,
                                    (char*) &ssidBytes[0]);

            if( 0 == memcmp(ssidBytes, ssidBytesToLookFor, memCmpLenght) )
            {
                LE_INFO( WIFIDEBUG "wifi: Found SSID we are looking for");
                AccessPointRefToConnectTo = accessPointRef;
            }

        }
        else
        {
            LE_ERROR( WIFIDEBUG "le_wifiClient_GetSsid ERROR");
        }
        }while ( NULL != (accessPointRef = le_wifiClient_GetNextAccessPoint( )) );
        LE_INFO( WIFIDEBUG "wifi: TESTRESULT: Found %d of AccessPoints", countNbrFound);
    }
    else
    {
        LE_ERROR( WIFIDEBUG "le_wifiClient_GetFirstAccessPoint ERROR");
    }

/*
    if ( NULL != AccessPointRefToConnectTo)
    {
        LE_INFO( WIFIDEBUG "Found test AP");

        LE_ASSERT( LE_OK == le_wifiClient_SetPassphrase( AccessPointRefToConnectTo, TEST_PASSWORD));

        if( LE_OK == le_wifiClient_Connect( AccessPointRefToConnectTo ) )
        {
            LE_INFO( WIFIDEBUG "le_wifiClient_Connect OK");
        }
        else
        {
            LE_INFO( WIFIDEBUG "le_wifiClient_Connect ERROR");
        }

    }
*/
}
static void TestConnect(void)
{
        // the AccessPointRefToConnectTo should still be valid after a Scan (if this will fail)
    if ( NULL != AccessPointRefToConnectTo)
    {
        LE_INFO( WIFIDEBUG " test AP valid  ");

        LE_ASSERT( LE_OK == le_wifiClient_SetPassphrase( AccessPointRefToConnectTo, TEST_PASSWORD));

        if( LE_OK == le_wifiClient_Connect( AccessPointRefToConnectTo ) )
        {
            LE_INFO( WIFIDEBUG "le_wifiClient_Connect OK");
        }
        else
        {
            LE_ERROR( WIFIDEBUG "le_wifiClient_Connect ERROR");
        }

    }
    else
    {
        LE_ERROR( WIFIDEBUG "le_wifiClient_Create ERROR: returned NULL.");
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
static void myMsgHandler
(
    le_wifiClient_Event_t event,
    void* contextPtr
)
{
    LE_INFO( WIFIDEBUG "Wifi Client event received");
    switch( event )
    {
        case LE_WIFICLIENT_EVENT_CONNECTED:
        {
            ///< Wifi Client Connected
            LE_INFO( WIFIDEBUG "LE_WIFICLIENT_EVENT_CONNECTED");
        }
        break;

        case LE_WIFICLIENT_EVENT_DISCONNECTED:
        {
            ///< Wifi Client Disconnected
            LE_INFO( WIFIDEBUG "LE_WIFICLIENT_EVENT_DISCONNECTED");
        }
        break;

        case LE_WIFICLIENT_EVENT_SCAN_DONE:
        {
            ScanDoneEventCounter ++; 
            LE_INFO( WIFIDEBUG "LE_WIFICLIENT_EVENT_SCAN_DONE: Now read the results (ScanDoneEventCounter %d)", ScanDoneEventCounter);
            MyReadScanResults();
            TestConnect();
        }
        break;
        default:
            LE_ERROR( WIFIDEBUG "ERROR Unknown event %d", event);
        break;
    }
}



/*
 * le_sms_RxMessageHandlerRef_t HdlrRef;
 *
 * // Add an handler function to handle message reception
 * HdlrRef=le_sms_AddRxMessageHandler(myMsgHandler);
 *
 * [...]
 *
 * // Remove Handler entry
 * le_sms_RemoveRxMessageHandler(HdlrRef);
*/
//--------------------------------------------------------------------------------------------------
/**
 * Test: GNSS device.
 *
 */
//--------------------------------------------------------------------------------------------------
void Testle_wifiClient
(
    void
)
{
    LE_INFO( WIFIDEBUG "Start Test Testle_wifiClient");

    // Add an handler function to handle message reception
    HdlrRef=le_wifiClient_AddNewEventHandler(myMsgHandler, NULL);


    if( LE_OK == le_wifiClient_Start() )
    {
        LE_INFO( WIFIDEBUG "le_wifiClient_Start OK");
    }
    else
    {
        LE_ERROR(WIFIDEBUG "le_wifiClient_Start ERROR");
    }

    
    AccessPointRefToConnectTo = le_wifiClient_Create( (const uint8_t *) TEST_SSID, TEST_SSID_NBR_BYTES);
    LE_INFO(WIFIDEBUG "test called le_wifiClient_Create. returned %p", AccessPointRefToConnectTo);
    
    
    LE_INFO(WIFIDEBUG "test Scan started. Waiting X seconds for it to finish");

    if( LE_OK == le_wifiClient_Scan() )
    {
        LE_INFO( WIFIDEBUG "le_wifiClient_Scan OK");
    }
    else
    {
        LE_INFO( WIFIDEBUG "le_wifiClient_Scan ERROR");
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
    LE_INFO(WIFIDEBUG "======== Wifi Client Test  ========");
    Testle_wifiClient();
}
