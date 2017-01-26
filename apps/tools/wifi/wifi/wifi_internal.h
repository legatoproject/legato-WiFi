//-------------------------------------------------------------------------------------------------
/**
 * @file wifi_internal.h
 *
 * WiFi Service Command line internal headerfile
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//-------------------------------------------------------------------------------------------------

#ifndef WIFI_INTERNAL_H
#define WIFI_INTERNAL_H

#include "legato.h"
#include "interfaces.h"

//--------------------------------------------------------------------------------------------------
/**
 * Print help for WiFi client
 */
//--------------------------------------------------------------------------------------------------
void PrintClientHelp(void);

//--------------------------------------------------------------------------------------------------
/**
 * Print help for WiFi access point
 */
//--------------------------------------------------------------------------------------------------
void PrintApHelp(void);

//--------------------------------------------------------------------------------------------------
/**
 * Process commands for WiFi client service.
 */
//--------------------------------------------------------------------------------------------------
void ExecuteWifiClientCommand
(
    const char *commandPtr, ///< [IN] Command to execute (NULL = run default command)
    size_t numArgs          ///< [IN] Number of arguments
);


//--------------------------------------------------------------------------------------------------
/**
 * Process commands for WiFi client service.
 */
//--------------------------------------------------------------------------------------------------
void ExecuteWifiApCommand
(
    const char *commandPtr, ///< [IN] Command to execute (NULL = run default command)
    size_t numArgs          ///< [IN] Number of arguments
);

#endif //WIFI_INTERNAL_H
