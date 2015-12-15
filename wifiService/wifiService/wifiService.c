// -------------------------------------------------------------------------------------------------
/**
 *  Legato Wifi Client
 *
 *  Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 *
 */
// -------------------------------------------------------------------------------------------------
#include "legato.h"

#include "interfaces.h"

#include "pa_wifi.h"

#include "wifiService.h"

#define WIFIDEBUG "WIFI DEBUG:"

//--------------------------------------------------------------------------------------------------
/**
 *  Server Init
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    LE_INFO( WIFIDEBUG "Wifi Service COMPONENT_INIT");
    wifiClientComponentInit( );
    wifiApComponentInit( );
}


