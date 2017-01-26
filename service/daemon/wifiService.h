// -------------------------------------------------------------------------------------------------
/**
 *
 *  Handles the component init for both WiFi Client and Access Point.
 *
 *  Copyright (C) Sierra Wireless Inc.
 *
 */
// -------------------------------------------------------------------------------------------------
#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H

#include "legato.h"

#include "interfaces.h"

void le_wifiClient_Init(void);

void le_wifiAp_Init(void);

#endif //WIFI_SERVICE
