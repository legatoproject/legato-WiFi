// -------------------------------------------------------------------------------------------------
/**
 *  Handles the component init for both Wifi Client and Access Point.
 *
 *  Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 *
 */
// -------------------------------------------------------------------------------------------------
#include "legato.h"

#include "interfaces.h"

#include "pa_wifi.h"

#include "wifiService.h"

//--------------------------------------------------------------------------------------------------
/**
 *  Server Init
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    LE_DEBUG( "Wifi Service COMPONENT_INIT");
    le_wifiClient_Init( );
    WifiApComponentInit( );
}


