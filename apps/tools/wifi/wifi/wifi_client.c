//-------------------------------------------------------------------------------------------------
/**
 * @file wifi_client.c
 *
 * WiFi client command line.
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
 * Event handler reference.
 */
//--------------------------------------------------------------------------------------------------
static le_wifiClient_NewEventHandlerRef_t ScanHdlrRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Event handler reference.
 */
//--------------------------------------------------------------------------------------------------
static le_wifiClient_NewEventHandlerRef_t ConnectHdlrRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Handler for WiFi client Connect event(s)
 *
 */
//--------------------------------------------------------------------------------------------------
static void WifiClientConnectEventHandler
(
    le_wifiClient_Event_t event,
        ///< [IN]
        ///< WiFi event to process
    void *contextPtr
        ///< [IN]
        ///< Associated event context
)
{
    LE_DEBUG("WiFi client event received");
    switch(event)
    {
        case LE_WIFICLIENT_EVENT_CONNECTED:
        {
            printf("CONNECTED.\n");
            le_wifiClient_RemoveNewEventHandler(ConnectHdlrRef);
            exit(EXIT_SUCCESS);
        }
        break;

        case LE_WIFICLIENT_EVENT_DISCONNECTED:
        {
            printf("DISCONNECTED.\n");
            le_wifiClient_RemoveNewEventHandler(ConnectHdlrRef);
            exit(EXIT_SUCCESS);
        }
        break;

        case LE_WIFICLIENT_EVENT_SCAN_DONE:
        {
            // This could happen. Not an error. Do nothing.
            LE_DEBUG("FYI: Got EVENT SCAN, while waiting for CONNECT.");
        }
        break;
        default:
            LE_ERROR("ERROR Unknown event %d", event);
        break;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Read Scan results and print them on the command line
 */
//--------------------------------------------------------------------------------------------------
static void WifiReadScanResults
(
    void
)
{
    le_wifiClient_AccessPointRef_t apRef = 0;
    le_result_t result = LE_OK;


    if (NULL != (apRef = le_wifiClient_GetFirstAccessPoint()))
    {
        do
        {
            uint8_t ssidBytes[LE_WIFIDEFS_MAX_SSID_BYTES];
            // Contains ssidNumElements number of bytes
            size_t ssidNumElements = LE_WIFIDEFS_MAX_SSID_LENGTH;

            if (LE_OK == (result = le_wifiClient_GetSsid(apRef,
                &ssidBytes[0],
                &ssidNumElements)))
            {
                printf("Found:\tSSID:\t\"%.*s\"\tStrength:%d\tRef:%p\n",
                    (int)ssidNumElements,
                    (char *)&ssidBytes[0],
                    le_wifiClient_GetSignalStrength(apRef),
                    apRef);
            }
            else
            {
                printf("ERROR::le_wifiClient_GetSsid failed: %d", result);
                exit(EXIT_FAILURE);
            }
        }
        while (NULL != (apRef = le_wifiClient_GetNextAccessPoint()));
    }
    else
    {
        LE_ERROR("le_wifiClient_GetFirstAccessPoint ERROR");
        printf("DEBUG: le_wifiClient_GetFirstAccessPoint ERROR");
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Handler for WiFi client Scan event
 *
 */
//--------------------------------------------------------------------------------------------------
static void WifiClientScanEventHandler
(
    le_wifiClient_Event_t event,
        ///< [IN]
        ///< WiFi event to process
    void *contextPtr
        ///< [IN]
        ///< Associated event context
)
{
    LE_DEBUG("WiFi Client event received");
    switch(event)
    {
        case LE_WIFICLIENT_EVENT_CONNECTED:
        {
            LE_DEBUG("FYI: Got EVENT CONNECTED, was while waiting for SCAN.");
        }
        break;

        case LE_WIFICLIENT_EVENT_DISCONNECTED:
        {
            LE_DEBUG("FYI: Got EVENT DISCONNECTED, was while waiting for SCAN.");
        }
        break;

        case LE_WIFICLIENT_EVENT_SCAN_DONE:
        {
            LE_DEBUG("LE_WIFICLIENT_EVENT_SCAN_DONE: Now read the results ");
            WifiReadScanResults();
            le_wifiClient_RemoveNewEventHandler(ScanHdlrRef);
            exit(EXIT_SUCCESS);
        }
        break;
        default:
            LE_ERROR("ERROR Unknown event %d", event);
        break;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Process commands for WiFi client service.
 */
//--------------------------------------------------------------------------------------------------
void PrintClientHelp(void)
{
       printf("wifi command line usage.\n"
           "==========\n\n"
           "To start using the WiFi client:\n"
           "\twifi client start\n"

           "To stop using the WiFi client:\n"
           "\twifi client stop\n"

           "To start a scan:\n"
           "\twifi client scan\n"

           "To create to an access point and get [REF]:\n"
           "\twifi client create [SSID]\n"

           "To connect to an access point set in 'create':\n"
           "\twifi client connect [REF]\n"

           "To set security protocol\n"
           "\twifi client setsecurityproto [REF] [SecuProto]\n"

           "Values for SecuProto;\n"
           "\t0: No security\n"
           "\t1: Using WEP\n"
           "\t2: Using WPA\n"
           "\t3: Using WPA2\n"
           "\t4: Using WPA Enterprise\n"
           "\t5 :Using WPA2 Enterprise\n"

           "To disconnect from an access point:\n"
           "\twifi client disconnect\n"

           "To delete the access point set in 'create':\n"
           "\twifi client delete [REF]\n"

           "To set WPA/WPA2 passphrase that generates PSK :\n"
           "\twifi client setpassphrase [REF] [passphrase]\n"

           "To set WPA/WPA2 PSK directly:\n"
            "\twifi client setpsk [REF] [PSK]\n"

           "To set WPA/WPA2 Enterprise user credentials:\n"
           "\twifi client setusercred [REF] [username] [password]\n"

            "To set WEP Key:\n"
           "\twifi client setwepkey [REF] [WEPKEY]\n"

           "\n");
}


//--------------------------------------------------------------------------------------------------
/**
 * Process commands for WiFi client service.
 */
//--------------------------------------------------------------------------------------------------
void ExecuteWifiClientCommand
(
    const char *commandPtr, ///< [IN] Command to execute (NULL = run default command)
    size_t numArgs          ///< [IN] Number of arguments
)
{
    int         rc1;            // sscanf() results
    int         rc2;            // sscanf() results
    uint32_t    length;
    le_result_t result = LE_OK;

    if (strcmp(commandPtr, "help") == 0)
    {
        PrintClientHelp();
        exit(EXIT_SUCCESS);
    }
    else if (strcmp(commandPtr, "start") == 0)
    {

        if (LE_OK == le_wifiClient_Start())
        {
            printf("successfully called start.\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiClient_Start returns ERROR.\n");
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "stop") == 0)
    {
        if (LE_OK == le_wifiClient_Stop())
        {
            printf("le_wifiClient_Stop returns OK.\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("le_wifiClient_Stop returns ERROR.\n");
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "scan") == 0)
    {
        // wifi client scan
        printf("starting scan.\n");

        // Add a handler function to handle message reception
        ScanHdlrRef=le_wifiClient_AddNewEventHandler(WifiClientScanEventHandler, NULL);


        if (LE_OK != (result= le_wifiClient_Scan()))
        {
            printf("ERROR: le_wifiClient_Scan returns %d.\n", result);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "create") == 0)
    {
        // wifi client create "SSID"
        const char                     *ssidPtr            = le_arg_GetArg(2);
        uint32_t                        length;
        le_wifiClient_AccessPointRef_t  createdAccessPoint;

        if (NULL == ssidPtr)
        {
            printf("ERROR: Missing argument.\n");
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

        createdAccessPoint = le_wifiClient_Create((const uint8_t *)ssidPtr, strlen(ssidPtr));

        if (NULL != createdAccessPoint)
        {
            printf("Created %s has reference %p.\n", ssidPtr, createdAccessPoint);
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("le_wifiClient_Create returns NULL.\n");
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "delete") == 0)
    {
        // wifi client delete [REF]
        const char                     *refPtr = le_arg_GetArg(2);
        le_wifiClient_AccessPointRef_t  apRef  = NULL;

        if (NULL == refPtr)
        {
            printf("ERROR. Missing argument.\n");
            exit(EXIT_FAILURE);
        }

        rc1 = sscanf(refPtr, "%x", (unsigned int *)&apRef);

        if ((1 == rc1) && (LE_OK == le_wifiClient_Delete(apRef)))
        {
            printf("Successful deletion.\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiClient_Delete returns ERROR.\n");
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "connect") == 0)
    {
        // wifi client connect [REF]
        const char                     *refPtr = le_arg_GetArg(2);
        le_wifiClient_AccessPointRef_t  apRef  = NULL;

        if (NULL == refPtr)
        {
            printf("ERROR: Missing argument.\n");
            exit(EXIT_FAILURE);
        }
        rc1 = sscanf(refPtr, "%x", (unsigned int *)&apRef);

        // Add a handler function to handle message reception
        ConnectHdlrRef = le_wifiClient_AddNewEventHandler(WifiClientConnectEventHandler, NULL);

        if ((1 == rc1) && (LE_OK == (result = le_wifiClient_Connect(apRef))))
        {
            printf("Connecting...\n");
        }
        else
        {
            printf("ERROR: le_wifiClient_Connect returns error code %d.\n", result);
            le_wifiClient_RemoveNewEventHandler(ConnectHdlrRef);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "disconnect") == 0)
    {
        // wifi client disconnect
        if (LE_OK == (result = le_wifiClient_Disconnect()))
        {
            printf("WiFi client disconnected.\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiClient_Disconnect returns error code %d.\n", result);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setsecurityproto") == 0)
    {
        // wifi client setsecurityproto [REF] [Security Protocol]
        const char                       *refPtr              = le_arg_GetArg(2);
        const char                       *securityProtocolPtr = le_arg_GetArg(3);
        le_wifiClient_AccessPointRef_t    apRef               = NULL;
        le_wifiClient_SecurityProtocol_t  securityProtocol    = 0;

        if ((NULL == refPtr) || (NULL == securityProtocolPtr))
        {
            printf("ERROR. Missing argument.\n");
            exit(EXIT_FAILURE);
        }

        rc1 = sscanf(refPtr, "%x", (unsigned int *)&apRef);
        rc2 = sscanf(securityProtocolPtr, "%u", (unsigned int *) &securityProtocol);
        result = le_wifiClient_SetSecurityProtocol(apRef, securityProtocol);

        if ((1 == rc1) && (1 == rc2) && (LE_OK == result))
        {
            printf("Successfully set security protocol.\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiClient_SetSecurityProtocol returns ERROR.\n");
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setpassphrase") == 0)
    {
        // wifi client setpassphrase [REF] [passPhrasePtr]
        const char                     *refPtr        = le_arg_GetArg(2);
        const char                     *passPhrasePtr = le_arg_GetArg(3);
        le_wifiClient_AccessPointRef_t  apRef         = NULL;

        if ((NULL == refPtr) || (NULL == passPhrasePtr))
        {
            printf("ERROR: Missing argument.\n");
            exit(EXIT_FAILURE);
        }

        rc1 = sscanf(refPtr, "%x", (unsigned int *)&apRef);

        length = strlen(passPhrasePtr);
        if ((length < LE_WIFIDEFS_MIN_PASSPHRASE_LENGTH) ||
            (length > LE_WIFIDEFS_MAX_PASSPHRASE_LENGTH))
        {
            printf("ERROR: Passphrase length must be between %d and %d.\n",
                LE_WIFIDEFS_MIN_PASSPHRASE_LENGTH,
                LE_WIFIDEFS_MAX_PASSPHRASE_LENGTH);
            exit(EXIT_FAILURE);
        }

        result = le_wifiClient_SetPassphrase(apRef, passPhrasePtr);
        if ((1 == rc1) && (LE_OK == result))
        {
            printf("Successfully set passphrase.\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiClient_SetPassphrase returns %d.\n", result);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setpsk") == 0)
    {
        // wifi client setpsk [REF] [PSK]
        const char                     *refPtr = le_arg_GetArg(2);
        const char                     *pskPtr = le_arg_GetArg(3);
        le_wifiClient_AccessPointRef_t  apRef  = NULL;

        if ((NULL == refPtr) || (NULL == pskPtr))
        {
            printf("ERROR: Missing argument.\n");
            exit(EXIT_FAILURE);
        }
        length = strlen(pskPtr);
        if (length > LE_WIFIDEFS_MAX_PSK_LENGTH)
        {
            printf("PSK length must not exceed %d bytes.\n", LE_WIFIDEFS_MAX_PSK_LENGTH);
        }

        rc1 = sscanf(refPtr, "%x", (unsigned int *)&apRef);

        if ((1 == rc1) && (LE_OK == le_wifiClient_SetPreSharedKey(apRef, pskPtr)))
        {
            printf("PSK set successfully.\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("le_wifiClient_SetPreSharedKey returns ERROR.\n");
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setusercred") == 0)
    {
        // wifi client setusercred [REF] [username] [password]
        const char                     *refPtr      = le_arg_GetArg(2);
        const char                     *usernamePtr = le_arg_GetArg(3);
        const char                     *passwordPtr = le_arg_GetArg(4);
        le_wifiClient_AccessPointRef_t  apRef       = NULL;

        if ((NULL == refPtr) || (NULL == usernamePtr) || (NULL == passwordPtr))
        {
            printf("ERROR: Missing argument.\n");
            exit(EXIT_FAILURE);
        }

        rc1 = sscanf(refPtr, "%x", (unsigned int *)&apRef);
        result = le_wifiClient_SetUserCredentials(apRef, usernamePtr, passwordPtr);

        if ((1 == rc1) && (LE_OK == result))
        {
            printf("Successfully set user credentials.\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiClient_SetUserCredentials returns %d.\n", result);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setwepkey") == 0)
    {
        // wifi client setwepKeyPtr [REF] [WEPKEY]
        const char                     *refPtr    = le_arg_GetArg(2);
        const char                     *wepKeyPtr = le_arg_GetArg(3);
        le_wifiClient_AccessPointRef_t  apRef     = NULL;

        if ((NULL == refPtr) || (NULL == wepKeyPtr))
        {
            printf("ERROR: Missing argument.\n");
            exit(EXIT_FAILURE);
        }

        rc1 = sscanf(refPtr, "%x", (unsigned int *)&apRef);
        result = le_wifiClient_SetPreSharedKey(apRef, wepKeyPtr);

        if ((1 == rc1) && (LE_OK == result))
        {
            printf("WEP key set sucessfully.\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiClient_SetPreSharedKey returns %d.\n", result);
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        printf("ERROR: Invalid command for WiFi service.\n");
        exit(EXIT_FAILURE);
    }
}


