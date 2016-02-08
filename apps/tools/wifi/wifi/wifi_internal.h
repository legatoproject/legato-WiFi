//-------------------------------------------------------------------------------------------------
/**
 * @file wifi.h
 *
 * Wifi Service Command line internal headerfile
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */
//-------------------------------------------------------------------------------------------------

#ifndef WIFI_INTERNAL_H
#define WIFI_INTERNAL_H

#include "legato.h"
#include "interfaces.h"

//--------------------------------------------------------------------------------------------------
/**
 * Print help for Wifi Client
 */
//--------------------------------------------------------------------------------------------------
void PrintClientHelp( void );

//--------------------------------------------------------------------------------------------------
/**
 * Print help for Wifi Access Point
 */
//--------------------------------------------------------------------------------------------------
void PrintApHelp( void );

//--------------------------------------------------------------------------------------------------
/**
 * Process commands for Wifi Client service.
 */
//--------------------------------------------------------------------------------------------------
void ExecuteWifiClientCommand
(
    const char * commandPtr,       ///< [IN] Command to execute (NULL = run default command)
    size_t numArgs          ///< [IN] Number of arguments
);


//--------------------------------------------------------------------------------------------------
/**
 * Process commands for Wifi Client service.
 */
//--------------------------------------------------------------------------------------------------
void ExecuteWifiApCommand
(
    const char * commandPtr,       ///< [IN] Command to execute (NULL = run default command)
    size_t numArgs          ///< [IN] Number of arguments
);

#endif //WIFI_INTERNAL_H
