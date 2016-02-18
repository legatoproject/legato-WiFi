 /**
  * This module implements a test for Wifi Access Point
  *
  * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
  *
  */

#include "legato.h"
#include "interfaces.h"


//--------------------------------------------------------------------------------------------------
//                                       Test Function
//--------------------------------------------------------------------------------------------------

#define TEST_SSID_STR "ExampleSSID"
#define TEST_PASSWORD "pass phrase"

#define TEST_SSID     ((const uint8_t *) TEST_SSID_STR)
#define TEST_SSID_NBR_BYTES     (sizeof(TEST_SSID_STR)-1)





//--------------------------------------------------------------------------------------------------
/**
 * Event handler reference.
 */
//--------------------------------------------------------------------------------------------------
static le_wifiAp_NewEventHandlerRef_t HdlrRef = NULL;


//! [SetCred]
static void Testle_setCredentials
(
    void
)
{
    LE_ASSERT(LE_OK == le_wifiAp_SetPassPhrase ( "Secret1" ));

    LE_ASSERT(LE_OK == le_wifiAp_SetSecurityProtocol ( LE_WIFIAP_SECURITY_WPA2 ));

    LE_ASSERT(LE_OK == le_wifiAp_SetDiscoverable ( true ));
}
//! [SetCred]

//! [Subscribe]
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
    LE_INFO( "Wifi Ap event received");
    switch( event )
    {
        case LE_WIFIAP_EVENT_CLIENT_CONNECTED:
        {
            ///< A client connect to AP
            LE_INFO( "LE_WIFIAP_EVENT_CLIENT_CONNECTED");
        }
        break;

        case LE_WIFIAP_EVENT_CLIENT_DISCONNECTED:
        {
            ///< A client connect to AP
            LE_INFO( "LE_WIFICLIENT_EVENT_DISCONNECTED");
        }
        break;


        default:
            LE_ERROR( "ERROR Unknown event %d", event);
        break;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Tests the Wifi Access Point.
 *
 */
//--------------------------------------------------------------------------------------------------
void Testle_wifiAp
(
    void
)
{
    LE_INFO( "Start Test Wifi Access Point");

    // Add an handler function to handle message reception
    HdlrRef=le_wifiAp_AddNewEventHandler( myMsgHandler, NULL );

    LE_ASSERT(HdlrRef != NULL);

    LE_ASSERT(LE_OK == le_wifiAp_SetSsid( TEST_SSID, TEST_SSID_NBR_BYTES ));

    Testle_setCredentials();

    if( LE_OK == le_wifiAp_Start() )
    {
        LE_INFO( "le_wifiAp_Start OK");
    }
    else
    {
        LE_ERROR( "le_wifiAp_Start ERROR");
    }

}
//! [Subscribe]


//--------------------------------------------------------------------------------------------------
/**
 * App init.
 *
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    // Wifi Init
    LE_INFO( "======== Wifi Ap Test  ========");
    Testle_wifiAp();
}
