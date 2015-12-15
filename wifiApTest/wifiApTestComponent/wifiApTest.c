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

#define TEST_SSID     ((const uint8_t *) "GustavNet")
#define TEST_SSID_NBR_BYTES     (sizeof(TEST_SSID)-1)
#define TEST_PASSWORD "sten123456789"
#define WIFIDEBUG     "WIFI TEST DEBUG:"




//--------------------------------------------------------------------------------------------------
/**
 * Event handler reference.
 */
//--------------------------------------------------------------------------------------------------
static le_wifiAp_NewEventHandlerRef_t HdlrRef = NULL;


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
    LE_INFO( WIFIDEBUG "Wifi Ap event received");
    switch( event )
    {
        case LE_WIFIAP_EVENT_CLIENT_CONNECTED:
        {
            ///< A client connect to AP
            LE_INFO( WIFIDEBUG "LE_WIFIAP_EVENT_CLIENT_CONNECTED");
        }
        break;

        case LE_WIFIAP_EVENT_CLIENT_DISCONNECTED:
        {
            ///< A client connect to AP
            LE_INFO( WIFIDEBUG "LE_WIFICLIENT_EVENT_DISCONNECTED");
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
void Testle_wifiAp
(
    void
)
{
    LE_INFO( WIFIDEBUG "Start Test Testle_wifiClient");

    // Add an handler function to handle message reception
    HdlrRef=le_wifiAp_AddNewEventHandler( myMsgHandler, NULL );

    LE_ASSERT(HdlrRef != NULL);

    LE_ASSERT(HdlrRef != NULL);

    LE_ASSERT(LE_OK == le_wifiAp_SetSsid( TEST_SSID, TEST_SSID_NBR_BYTES ));

    LE_ASSERT(LE_OK == le_wifiAp_SetSsid( TEST_SSID, TEST_SSID_NBR_BYTES ));

    if( LE_OK == le_wifiAp_Start() )
    {
        LE_INFO( WIFIDEBUG "le_wifiClient_Start OK");
    }
    else
    {
        LE_ERROR(WIFIDEBUG "le_wifiClient_Start ERROR");
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
    LE_INFO(WIFIDEBUG "======== Wifi Ap Test  ========");
    Testle_wifiAp();
}
