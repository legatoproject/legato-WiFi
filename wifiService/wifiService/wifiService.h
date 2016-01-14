#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H
// -------------------------------------------------------------------------------------------------
/**
 *
 *  Handles the component init for both Wifi Client and Access Point.
 *
 *  Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 *
 */
// -------------------------------------------------------------------------------------------------
#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H

#include "legato.h"

#include "interfaces.h"

void le_wifiClient_Init( void );

void le_wifiAp_Init( void );

#endif //WIFI_SERVICE
