//-------------------------------------------------------------------------------------------------
/**
 * @file wifi_ap.c
 *
 * Wifi Service Command line.
 *
 * Note that since the command is run in new context each time,
 * nothing can be saved between runs.
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */
//--------------------------------------------------------------------------------------------------

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

