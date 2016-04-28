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
//-------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "wifi_internal.h"



//--------------------------------------------------------------------------------------------------
/**
 * Process commands for Wifi Client service.
 */
//--------------------------------------------------------------------------------------------------
void PrintApHelp( void )
{
    printf("wifi command line Access Point usage\n"
       "==========\n\n"
       "To start the  Wifi Access Point:\n"
       "\twifi ap start\n"

       "To stop the  Wifi Access Point:\n"
       "\twifi ap stop\n"


       "To set the SSID of the Wifi Access Point:\n"
       "\twifi ap setssid [SSID]\n"

       "To set the channel of the Wifi Access Point:\n"
       "\twifi ap setchannel [ChannelNo]\n"

       "To set the security protocol used :\n"
       "\twifi ap setsecurityproto [SecuProto]\n"
       "Values for SecuProto;\n"
       "\t0: No security\n"
       "\t1: Using WPA2\n"

       "To set WPA2 passphrase that generates PSK :\n"
       "\twifi ap setpassphrase  [passphrase]\n"

       "To set WPA2 PSK directly:\n"
        "\twifi ap setpsk [PSK]\n"


       "To setdiscoverablity for the Wifi Access Point:\n"
       "\twifi ap setdiscoverable\n"

       "To maximum nbr of clients for the Wifi Access Point:\n"
       "\twifi ap setmaxclients [MAXNBR]\n"

       "To define the address of the AP and the IP addresses range as well:\n"
       "WARNING: Only IPv4 addresses are supported.\n"
       "\twifi ap setiprange [IP AP] [IP START] [IP STOP]\n"

       "\n");
}


//--------------------------------------------------------------------------------------------------
/**
 * Process commands for Wifi Client service.
 */
//--------------------------------------------------------------------------------------------------
void ExecuteWifiApCommand
(
    const char * commandPtr,       ///< [IN] Command to execute (NULL = run default command)
    size_t numArgs          ///< [IN] Number of arguments
)
{
    le_result_t result = LE_OK;

    if (strcmp(commandPtr, "help") == 0)
    {
        PrintApHelp();
        exit(EXIT_SUCCESS);
    }
    else if (strcmp(commandPtr, "start") == 0)
    {
        if (LE_OK == (result=le_wifiAp_Start()))
        {
            printf("starting\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR:le_wifiAp_Start returns %d \n", result);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "stop") == 0)
    {
        if (LE_OK == le_wifiAp_Stop())
        {
            printf("Stopped.\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR:le_wifiAp_Stop returns %d \n", result);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setssid") == 0)
    { // wifi ap setssid "SSID"
        const char * ssid_string = le_arg_GetArg(2);
        if ( NULL == ssid_string )
        {
            printf("wifi ap setssid . Missing or bad argument\n");
            exit(EXIT_FAILURE);
        }
        if ( LE_OK == le_wifiAp_SetSsid ( (const uint8_t *) ssid_string, strlen(ssid_string) ))
        {
            printf("le_wifiAp_SetSsid successful \n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("le_wifiAp_SetSsid returns NULL\n");
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setsecurityproto") == 0)
    {
        //wifi ap setsecurityproto [SecuProto]
        const char * secuProtoStr = le_arg_GetArg(2);
        int securityProtocol = 0;
        if ( NULL == secuProtoStr )
        {
            printf("ERROR: Missing or bad argument\n");
            exit(EXIT_FAILURE);
        }
        securityProtocol = strtol(secuProtoStr, NULL, 10);

        if ( LE_OK == ( result=le_wifiAp_SetSecurityProtocol ( securityProtocol )))
        {
            printf("Security Protocl set to %d\n", securityProtocol);
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiAp_SetSecurityProtocol returns %d\n",result);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setchannel") == 0)
    {
        //wifi ap setchannel [ChannelNo]
        const char * arg2 = le_arg_GetArg(2);
        int channelNo = 0;
        if ( NULL == arg2 )
        {
            printf("ERROR: Missing or bad argument\n");
            exit(EXIT_FAILURE);
        }
        channelNo = strtol(arg2, NULL, 10);

        if ( LE_OK == ( result=le_wifiAp_SetChannel( channelNo )))
        {
            printf("Channel set to %d\n", channelNo);
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiAp_SetChannel returns %d\n",result);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setpassphrase") == 0)
    {
        //le_wifiAp_SetPassPhrase
        //wifi ap setpassphrase  [passphrase]
        const char * arg2 = le_arg_GetArg(2);
        if ( NULL == arg2 )
        {
            printf("ERROR: Missing or bad argument\n");
            exit(EXIT_FAILURE);
        }

        if ( LE_OK == ( result=le_wifiAp_SetPassPhrase( arg2 )))
        {
            printf("Passphrase set.\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiAp_SetPassPhrase returns %d\n",result);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setpsk") == 0)
    {
        //le_wifiAp_SetPreSharedKey
        //wifi ap setpsk  [PSK]
        const char * arg2 = le_arg_GetArg(2);
        if ( NULL == arg2 )
        {
            printf("ERROR: Missing or bad argument\n");
            exit(EXIT_FAILURE);
        }

        if ( LE_OK == ( result=le_wifiAp_SetPreSharedKey( arg2 )))
        {
            printf("PSK set.\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiAp_SetPreSharedKey returns %d\n",result);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setdiscoverable") == 0)
    {
        //wifi ap setdiscoverable [0/1]
        const char * arg2 = le_arg_GetArg(2);
        int discoverable = 0;
        if ( NULL == arg2 )
        {
            printf("ERROR: Missing or bad argument\n");
            exit(EXIT_FAILURE);
        }
        discoverable = strtol(arg2, NULL, 10);

        if ( LE_OK == ( result=le_wifiAp_SetDiscoverable( discoverable )))
        {
            printf("Discoverable set to %d\n", discoverable);
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiAp_SetDiscoverable returns %d\n",result);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setmaxclients") == 0)
    {
        //le_wifiAp_SetMaxNumberOfClients()
        //wifi ap setmaxclients [MAXNBR]
        const char * arg2 = le_arg_GetArg(2);
        int maxClients = 0;
        if ( NULL == arg2 )
        {
            printf("ERROR: Missing or bad argument\n");
            exit(EXIT_FAILURE);
        }
        maxClients = strtol(arg2, NULL, 10);

        if ( LE_OK == ( result=le_wifiAp_SetMaxNumberOfClients( maxClients)))
        {
            printf("Max number of clients set to %d\n", maxClients);
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiAp_SetMaxNumberOfClients returns %d\n",result);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setiprange") == 0)
    {
        // Only IPv4 addresses are supported.
        // wifi ap setiprange [IP AP] [IP START] [IP STOP]
        const char * ipAp = le_arg_GetArg(2);
        const char * ipStart = le_arg_GetArg(3);
        const char * ipStop = le_arg_GetArg(4);
        if ( ( NULL == ipAp ) || ( NULL == ipStart ) || ( NULL == ipStop ) ||
             ( '\0' == ipAp ) || ( '\0' == ipStart ) || ( '\0' == ipStop ) )
        {
            printf("ERROR: Missing or bad argument(s)\n");
            exit(EXIT_FAILURE);
        }

        if ( LE_OK == ( result=le_wifiAp_SetIpRange( ipAp, ipStart, ipStop )))
        {
            printf("IP AP@=%s, Start@=%s, Stop@=%s\n", ipAp, ipStart, ipStop);
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiAp_SetIpRange returns %d\n",result);
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        printf("Invalid command for wifi service.\n");
        exit(EXIT_FAILURE);
    }
}

