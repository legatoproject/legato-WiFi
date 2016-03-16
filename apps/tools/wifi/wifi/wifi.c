//--------------------------------------------------------------------------------------------------
/**
 * @page c_le_wifiAp Wifi Access Point Service
 *
 * @ref le_wifiAp_interface.h "API Reference"
 *
 * <HR>
 *
 * This Wifi Service API provides Wifi Access Point setup.
 * Please note that if there is only one wifi hardware the Wifi Access Point
 * cannot be used at the same time as the Wifi Client service.
 *
 * @section le_wifi_binding IPC interfaces binding
 *
 *
 * Here's a code sample binding to Wifi service:
 * @verbatim
   bindings:
   {
      clientExe.clientComponent.le_wifiAp -> wifiService.le_wifiAp
   }
   @endverbatim
 *
 *
 */
//--------------------------------------------------------------------------------------------------



//-------------------------------------------------------------------------------------------------
/**
 * @file wifi.c
 *
 * Wifi Service Command line.
 *
 * Note that since the command is run in new context each time,
 * nothing can be saved between runs.
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */
//-------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "wifi_internal.h"



//--------------------------------------------------------------------------------------------------
/**
 * General help message for both client & ap.
 */
//--------------------------------------------------------------------------------------------------
static void PrintHelp( void )
{
        printf("wifi command line usage\n"
           "==========\n\n"
           "To run Wifi Client:\n"
           "\twifi client help\n"
           "To run Wifi Access Point:\n"
           "\twifi ap help\n"
           "\n");
}


//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    // calling just "wifi client/ap" without arguments will give helpmenu
    if (le_arg_NumArgs() <= 1)
    {
        PrintHelp();
        exit(EXIT_SUCCESS);
    }
    else
    {
        const char* service = le_arg_GetArg(0);
        const char* command = le_arg_GetArg(1);

        if ( (0 == strcmp(service, "help")) ||
             (0 == strcmp(service, "--help")) ||
             (0 == strcmp(service, "-h")) )
        {
            PrintHelp();
            exit(EXIT_SUCCESS);
        }
        else
        {
            if (strcmp(service, "client" ) == 0)
            {
                ExecuteWifiClientCommand(command, le_arg_NumArgs());
            }
            else if (strcmp(service, "ap" ) == 0)
            {
                ExecuteWifiApCommand(command, le_arg_NumArgs());;
            }
            else
            {
                PrintHelp();
                exit(EXIT_SUCCESS);
            }
        }
    }
}

