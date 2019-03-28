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
//-------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "wifi_internal.h"



//--------------------------------------------------------------------------------------------------
/**
 * Process commands for WiFi client service.
 */
//--------------------------------------------------------------------------------------------------
void PrintApHelp(void)
{
    printf("WiFi command line access point usage\n"
        "==========\n\n"
        "To start the WiFi access point:\n"
        "\twifi ap start\n"

        "To stop the WiFi access point:\n"
        "\twifi ap stop\n"

        "To set the SSID of the WiFi access point:\n"
        "\twifi ap setssid [\"SSID\"]\n"

        "To set the IEEE stdmask of the WiFi access point:\n"
        "\twifi ap setstdmask [IEEEStdMask]\n"
        "Bit mask for IEEEStdMask;\n"
        "\t0x1:   IEEE 802.11a\n"
        "\t0x2:   IEEE 802.11b\n"
        "\t0x4:   IEEE 802.11g\n"
        "\t0x8:   IEEE 802.11ad\n"
        "\t0x10:  IEEE 802.11d\n"
        "\t0x20:  IEEE 802.11h\n"
        "\t0x40:  IEEE 802.11n\n"
        "\t0x80:  IEEE 802.11ac\n"
        "\t0x100: IEEE 802.11ax\n"
        "\t0x200: IEEE 802.11w\n"

        "To get the IEEE stdmask of the WiFi access point:\n"
        "\twifi ap getstdmask\n"

        "To set the channel of the WiFi access point:\n"
        "\twifi ap setchannel [ChannelNo]\n"
        "Values for ChannelNo;\n"
        "\tbetween 1 and 14  for IEEE 802.11b/g\n"
        "\tbetween 7 and 196 for IEEE 802.11a\n"
        "\tbetween 1 and 6   for IEEE 802.11ad\n"
        "\tSome legal restrictions might apply for your region\n"

        "To set the countrycode of the WiFi access point:\n"
        "\twifi ap setcountrycode [CountryCode]\n"

        "To set the security protocol used :\n"
        "\twifi ap setsecurityproto [SecuProto]\n"
        "Values for SecuProto;\n"
        "\t0: No security\n"
        "\t1: Using WPA2\n"

        "To set WPA2 passphrase that generates PSK :\n"
        "\twifi ap setpassphrase [passphrase]\n"

        "To set WPA2 PSK directly:\n"
        "\twifi ap setpsk [PSK]\n"

        "To set the discoverablity for the WiFi access point:\n"
        "\twifi ap setdiscoverable\n"

        "To maximum nbr of clients for the WiFi access point:\n"
        "\twifi ap setmaxclients [MAXNBR]\n"

        "To define the address of the AP and the IP addresses range as well:\n"
        "WARNING: Only IPv4 addresses are supported.\n"
        "\twifi ap setiprange [IP AP] [IP START] [IP STOP]\n"

        "\n");
}


//--------------------------------------------------------------------------------------------------
/**
 * Process commands for Wifi client service.
 */
//--------------------------------------------------------------------------------------------------
void ExecuteWifiApCommand
(
    const char *commandPtr, ///< [IN] Command to execute (NULL = run default command)
    size_t numArgs          ///< [IN] Number of arguments
)
{
    le_result_t result = LE_OK;
    uint32_t    length;

    if (strcmp(commandPtr, "help") == 0)
    {
        PrintApHelp();
        exit(EXIT_SUCCESS);
    }
    else if (strcmp(commandPtr, "start") == 0)
    {
        if (LE_OK == (result = le_wifiAp_Start()))
        {
            printf("starting...\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR:le_wifiAp_Start returns %d.\n", result);
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
            printf("ERROR:le_wifiAp_Stop returns %d.\n", result);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setssid") == 0)
    {
        // wifi ap setssid "SSID"
        const char *ssidPtr = le_arg_GetArg(2);

        if (NULL == ssidPtr)
        {
            printf("wifi ap setssid. Missing or bad argument.\n");
            exit(EXIT_FAILURE);
        }
        length = strlen(ssidPtr);
        if ((length > LE_WIFIDEFS_MAX_SSID_LENGTH) || (length < 1))
        {
            // Notify on the console the user about the issue as this one can occur in background.
            printf("ERROR: SSID length (%d) must be between %d and %d bytes long.\n",
                length,
                LE_WIFIDEFS_MIN_SSID_LENGTH,
                LE_WIFIDEFS_MAX_SSID_LENGTH);
            LE_ERROR("ERROR: SSID length (%d) must be between %d and %d bytes long.\n",
                length,
                LE_WIFIDEFS_MIN_SSID_LENGTH,
                LE_WIFIDEFS_MAX_SSID_LENGTH);
            exit(EXIT_FAILURE);
        }

        if (LE_OK == le_wifiAp_SetSsid((const uint8_t *)ssidPtr, length))
        {
            printf("SSID set successfully.\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("Invalid SSID.\n");
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setsecurityproto") == 0)
    {
        // wifi ap setsecurityproto [SecuProto]
        const char *secuProtoStr     = le_arg_GetArg(2);
        int         securityProtocol = 0;

        if (NULL == secuProtoStr)
        {
            printf("ERROR: Missing or bad argument.\n");
            exit(EXIT_FAILURE);
        }
        securityProtocol = strtol(secuProtoStr, NULL, 10);
        if (errno != 0)
        {
            printf("ERROR: Bad argument value. Valid value is 0 or 1.\n");
            exit(EXIT_FAILURE);
        }
        if ((securityProtocol < 0) || (securityProtocol > 1))
        {
            printf("ERROR: Bad argument value. Valid value is 0 or 1.\n");
            exit(EXIT_FAILURE);
        }

        if (LE_OK == (result = le_wifiAp_SetSecurityProtocol(securityProtocol)))
        {
            printf("Security protocol set to %d.\n", securityProtocol);
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiAp_SetSecurityProtocol returns %d.\n", result);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setstdmask") == 0)
    {
        // wifi ap setbitmask [IEEEStdMask]
        const char *stdMaskPtr = le_arg_GetArg(2);
        le_wifiAp_IeeeStdBitMask_t stdMask;
        int8_t      hwMode = 0;
        int8_t      numCheck = 0;

        if (NULL == stdMaskPtr)
        {
            printf("ERROR: Missing or bad argument.\n");
            exit(EXIT_FAILURE);
        }

        stdMask = strtol(stdMaskPtr, NULL, 0);
        if (errno != 0)
        {
            printf("ERROR: Invalid argument.\n");
            exit(EXIT_FAILURE);
        }

        hwMode = stdMask & 0x0F;
        numCheck = (hwMode & 0x1) + ((hwMode >> 1) & 0x1) +
                       ((hwMode >> 2) & 0x1) + ((hwMode >> 3) & 0x1);

        // CAUTION: Hardware mode is exclusive. At least one mode should be set.
        if ( 0 == numCheck )
        {
            printf("ERROR: No hardware mode is set.\n");
            exit(EXIT_FAILURE);
        }
        if ( numCheck > 1 )
        {
            printf("ERROR: Only one hardware mode can be set.\n");
            exit(EXIT_FAILURE);
        }

        // CAUTION: ieee80211ac=1 only works with hw_mode=a
        if ((stdMask & LE_WIFIAP_BITMASK_IEEE_STD_AC) &&
            (0 == (stdMask & LE_WIFIAP_BITMASK_IEEE_STD_A)))
        {
            printf("ERROR: ieee80211ac=1 only works with hw_mode=a.\n");
            exit(EXIT_FAILURE);
        }

        if (LE_OK == (result = le_wifiAp_SetIeeeStandard(stdMask)))
        {
            printf("IEEEStdMask set to 0x%X.\n", stdMask);
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiAp_SetIeeeStandard returns %d.\n", result);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "getstdmask") == 0)
    {
        // wifi ap getbitmask [IEEEStdMask]
        le_wifiAp_IeeeStdBitMask_t stdmask;

        if (LE_OK == (result = le_wifiAp_GetIeeeStandard(&stdmask)))
        {
            printf("IEEEStdMask is 0x%X.\n", stdmask);
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiAp_GetIeeeStandard returns %d.\n", result);
            exit(EXIT_FAILURE);
        }
    }

    else if (strcmp(commandPtr, "setchannel") == 0)
    {
        // wifi ap setchannel [ChannelNo]
        const char *channelPtr = le_arg_GetArg(2);
        int         channelNo  = 0;

        if (NULL == channelPtr)
        {
            printf("ERROR: Missing or bad argument.\n");
            exit(EXIT_FAILURE);
        }

        channelNo = strtol(channelPtr, NULL, 10);
        if (errno != 0)
        {
            printf("ERROR: Invalid argument.\n");
            exit(EXIT_FAILURE);
        }
        // CAUTION: Range of channels value is different with different hardware mode.
        if (LE_OK == (result = le_wifiAp_SetChannel(channelNo)))
        {
            printf("Channel set to %d.\n", channelNo);
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("Please check hardware mode and channel range.\n");
            printf("ERROR: le_wifiAp_SetChannel returns %d.\n", result);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setcountrycode") == 0)
    {
        // wifi ap setcountrycode [CountryCode]
        const char *countryCodePtr = le_arg_GetArg(2);

        if (NULL == countryCodePtr)
        {
            printf("ERROR: Missing or bad argument.\n");
            exit(EXIT_FAILURE);
        }
        length = strlen(countryCodePtr);
        if (length != LE_WIFIDEFS_ISO_COUNTRYCODE_LENGTH)
        {
            printf("ERROR: Countrycode length must be %d.\n",
                   LE_WIFIDEFS_ISO_COUNTRYCODE_LENGTH);
            exit(EXIT_FAILURE);
        }

        if (LE_OK == (result = le_wifiAp_SetCountryCode(countryCodePtr)))
        {
            printf("Countrycode set to %s.\n", countryCodePtr);
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiAp_SetCountryCode returns %d.\n", result);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setpassphrase") == 0)
    {
        // wifi ap setpassphrase  [passphrase]
        const char *passPhrasePtr = le_arg_GetArg(2);

        if (NULL == passPhrasePtr)
        {
            printf("ERROR: Missing or bad argument.\n");
            exit(EXIT_FAILURE);
        }
        length = strlen(passPhrasePtr);
        if ((length < LE_WIFIDEFS_MIN_PASSPHRASE_LENGTH) ||
            (length > LE_WIFIDEFS_MAX_PASSPHRASE_LENGTH))
        {
            printf("ERROR: Passphrase length must be between %d and %d.\n",
                LE_WIFIDEFS_MIN_PASSPHRASE_LENGTH,
                LE_WIFIDEFS_MAX_PASSPHRASE_LENGTH);
            exit(EXIT_FAILURE);
        }

        if (LE_OK == (result = le_wifiAp_SetPassPhrase(passPhrasePtr)))
        {
            printf("Passphrase set.\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiAp_SetPassPhrase returns %d.\n", result);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setpsk") == 0)
    {
        // wifi ap setpsk [PSK]
        const char *pskPtr = le_arg_GetArg(2);

        if (NULL == pskPtr)
        {
            printf("ERROR: Missing or bad argument.\n");
            exit(EXIT_FAILURE);
        }
        length = strlen(pskPtr);
        if (length > LE_WIFIDEFS_MAX_PSK_LENGTH)
        {
            printf("PSK length must not exceed %d bytes.\n", LE_WIFIDEFS_MAX_PSK_LENGTH);
        }

        if (LE_OK == (result = le_wifiAp_SetPreSharedKey(pskPtr)))
        {
            printf("PSK set.\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiAp_SetPreSharedKey returns %d.\n", result);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setdiscoverable") == 0)
    {
        // wifi ap setdiscoverable [0/1]
        const char *discoverablePtr = le_arg_GetArg(2);
        int         discoverable       = 0;

        if (NULL == discoverablePtr)
        {
            printf("ERROR: Missing or bad argument.\n");
            exit(EXIT_FAILURE);
        }
        discoverable = strtol(discoverablePtr, NULL, 2);
        if (errno != 0)
        {
            printf("ERROR: Invalid argument.\n");
            exit(EXIT_FAILURE);
        }

        if (LE_OK == (result = le_wifiAp_SetDiscoverable((discoverable ? true : false))))
        {
            printf("Discoverable set to %d.\n", discoverable);
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiAp_SetDiscoverable returns %d.\n", result);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setmaxclients") == 0)
    {
        // wifi ap setmaxclients [MAXNBR]
        const char *maxClientsPtr = le_arg_GetArg(2);
        int         maxClients    = 0;

        if (NULL == maxClientsPtr)
        {
            printf("ERROR: Missing or bad argument.\n");
            exit(EXIT_FAILURE);
        }
        maxClients = strtol(maxClientsPtr, NULL, 10);
        if ((errno != 0) || (maxClients < 0))
        {
            printf("ERROR: Bad argument value. Should be a positive decimal value.\n");
            exit(EXIT_FAILURE);
        }

        if (LE_OK == (result = le_wifiAp_SetMaxNumberOfClients(maxClients)))
        {
            printf("Max number of clients set to %d.\n", maxClients);
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiAp_SetMaxNumberOfClients returns %d.\n", result);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setiprange") == 0)
    {
        // Only IPv4 addresses are supported.
        // wifi ap setiprange [IP AP] [IP START] [IP STOP]
        const char *ipAp    = le_arg_GetArg(2);
        const char *ipStart = le_arg_GetArg(3);
        const char *ipStop  = le_arg_GetArg(4);

        if ((NULL == ipAp) || (NULL == ipStart) || (NULL == ipStop) ||
            ('\0' == ipAp[0]) || ('\0' == ipStart[0]) || ('\0' == ipStop[0]))
        {
            printf("ERROR: Missing or bad argument(s).\n");
            exit(EXIT_FAILURE);
        }

        if (LE_OK == (result = le_wifiAp_SetIpRange(ipAp, ipStart, ipStop)))
        {
            printf("IP AP@=%s, Start@=%s, Stop@=%s\n", ipAp, ipStart, ipStop);
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiAp_SetIpRange returns %d.\n", result);
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        printf("Invalid command for WiFi service.\n");
        exit(EXIT_FAILURE);
    }
}

