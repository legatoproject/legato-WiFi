//-------------------------------------------------------------------------------------------------
/**
 * @file wifi_ap.c
 *
 * WiFi Service Command line.
 *
 * Note that since the command is run in new context each time,
 * nothing can be saved between runs.
 *
 * Copyright (C) Sierra Wireless Inc.
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
static void PrintHelp(void)
{
    printf("WiFi command line usage\n"
        "==========\n\n"
        "To run WiFi client:\n"
        "\twifi client help\n"
        "To run WiFi access point:\n"
        "\twifi ap help\n"
        "\n");
}


//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    // calling just "WiFi client/ap" without arguments will give helpmenu
    if (le_arg_NumArgs() <= 1)
    {
        PrintHelp();
        exit(EXIT_SUCCESS);
    }
    else
    {
        const char *servicePtr = le_arg_GetArg(0);
        const char *commandPtr = le_arg_GetArg(1);

        if ((0 == strcmp(servicePtr, "help")) ||
            (0 == strcmp(servicePtr, "--help")) ||
            (0 == strcmp(servicePtr, "-h")))
        {
            PrintHelp();
            exit(EXIT_SUCCESS);
        }
        else
        {
            if (strcmp(servicePtr, "client") == 0)
            {
                ExecuteWifiClientCommand(commandPtr, le_arg_NumArgs());
            }
            else if (strcmp(servicePtr, "ap") == 0)
            {
                ExecuteWifiApCommand(commandPtr, le_arg_NumArgs());;
            }
            else
            {
                PrintHelp();
                exit(EXIT_SUCCESS);
            }
        }
    }
}

