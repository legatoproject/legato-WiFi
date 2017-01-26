// -------------------------------------------------------------------------------------------------
/**
 *  Handles the component init for both WiFi Client and Access Point.
 *
 *  Copyright (C) Sierra Wireless Inc.
 *
 */
// -------------------------------------------------------------------------------------------------
#include "legato.h"
#include "interfaces.h"
#include "wifiService.h"

//--------------------------------------------------------------------------------------------------
/**
 *  Server Init
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    LE_DEBUG("WiFi Service COMPONENT_INIT");
    le_wifiClient_Init();
    le_wifiAp_Init();
}


