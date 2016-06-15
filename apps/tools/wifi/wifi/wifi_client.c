//-------------------------------------------------------------------------------------------------
/**
 * @file wifi_client.c
 *
 * Wifi client command line.
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
 * Handler for Wifi Client Connect event(s)
 *
 * @param event
 *        Handles the wifi events
 * @param contextPtr
 */
//--------------------------------------------------------------------------------------------------
static void WifiClientConnectEventHandler
(
    le_wifiClient_Event_t event,
    void* contextPtr
)
{
    LE_DEBUG( "Wifi Client event received");
    switch( event )
    {
        case LE_WIFICLIENT_EVENT_CONNECTED:
        {
            printf("CONNECTED\n");
            le_wifiClient_RemoveNewEventHandler( ConnectHdlrRef );
            exit(EXIT_SUCCESS);
        }
        break;

        case LE_WIFICLIENT_EVENT_DISCONNECTED:
        {
            printf("DISCONNECTED\n");
            le_wifiClient_RemoveNewEventHandler( ConnectHdlrRef );
            exit(EXIT_SUCCESS);
        }
        break;

        case LE_WIFICLIENT_EVENT_SCAN_DONE:
        {
            // this could happen. Not an error. Do nothing.
            LE_DEBUG( "FYI: Got EVENT SCAN, was while waiting for CONNECT.");
        }
        break;
        default:
            LE_ERROR( "ERROR Unknown event %d", event);
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
    le_wifiClient_AccessPointRef_t accessPointRef = 0;
    le_result_t result = LE_OK;


    if( NULL != (accessPointRef = le_wifiClient_GetFirstAccessPoint()) )
    {
        do
        {
            uint8_t ssidBytes[LE_WIFIDEFS_MAX_SSID_BYTES];
            // Contains ssidNumElements number of bytes
            size_t ssidNumElements = LE_WIFIDEFS_MAX_SSID_BYTES;

            if( LE_OK == (result=le_wifiClient_GetSsid( accessPointRef,
                                                &ssidBytes[0],
                                                &ssidNumElements)) )
            {
                printf("Found:\tSSID:\t\"%.*s\"\tStrength:%d\tRef:%p\n",
                                    (int)ssidNumElements,
                                    (char*) &ssidBytes[0],
                                    le_wifiClient_GetSignalStrength(accessPointRef),
                                    accessPointRef);
            }
            else
            {
                printf( "ERROR::le_wifiClient_GetSsid failed: %d",result);
                exit(EXIT_FAILURE);
            }
        }
        while ( NULL != (accessPointRef = le_wifiClient_GetNextAccessPoint( )) );
    }
    else
    {
        LE_ERROR( "le_wifiClient_GetFirstAccessPoint ERROR");
        printf( "DEBUG: le_wifiClient_GetFirstAccessPoint ERROR" );
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Handler for Wifi Client Scan event
 *
 * @param event
 *        Handles the wifi events
 * @param contextPtr
 */
//--------------------------------------------------------------------------------------------------
static void WifiClientScanEventHandler
(
    le_wifiClient_Event_t event,
    void* contextPtr
)
{
    LE_DEBUG( "Wifi Client event received");
    switch( event )
    {
        case LE_WIFICLIENT_EVENT_CONNECTED:
        {
            LE_DEBUG( "FYI: Got EVENT CONNECTED, was while waiting for SCAN.");
        }
        break;

        case LE_WIFICLIENT_EVENT_DISCONNECTED:
        {
            LE_DEBUG( "FYI: Got EVENT DISCONNECTED, was while waiting for SCAN.");
        }
        break;

        case LE_WIFICLIENT_EVENT_SCAN_DONE:
        {
            LE_DEBUG( "LE_WIFICLIENT_EVENT_SCAN_DONE: Now read the results ");
            WifiReadScanResults();
            le_wifiClient_RemoveNewEventHandler( ScanHdlrRef );
            exit(EXIT_SUCCESS);
        }
        break;
        default:
            LE_ERROR( "ERROR Unknown event %d", event);
        break;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Process commands for Wifi Client service.
 */
//--------------------------------------------------------------------------------------------------
void PrintClientHelp( void )
{
       printf("wifi command line usage\n"
           "==========\n\n"
           "To start using the Wifi Client:\n"
           "\twifi client start\n"
           "To stop  using the Wifi Client:\n"
           "\twifi client stop\n"
           "To start a scan:\n"
           "\twifi client scan\n"
           "To create to a access point and get [REF]:\n"
           "\twifi client create [SSID]\n"
           "To connect to a access point set in 'create':\n"
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
            "\twifi client setpsk [REF] [psk]\n"

           "To set WPA/WPA2 Enterprise user credentials:\n"
           "\twifi client setusercred [REF] [userName] [password]\n"

            "To set WEP Key:\n"
           "\twifi client setwepkey [REF] [WEPKEY]\n"

           "\n");
}


//--------------------------------------------------------------------------------------------------
/**
 * Process commands for Wifi Client service.
 */
//--------------------------------------------------------------------------------------------------
void ExecuteWifiClientCommand
(
    const char * commandPtr,       ///< [IN] Command to execute (NULL = run default command)
    size_t numArgs          ///< [IN] Number of arguments
)
{
    int rc1, rc2; // sscanf() results
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
            printf("successfully called start\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiClient_Start returns ERROR\n");
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "stop") == 0)
    {
        if (LE_OK == le_wifiClient_Stop())
        {
            printf("le_wifiClient_Stop returns OK\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("le_wifiClient_Stop returns ERROR\n");
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "scan") == 0)
    {
        //"wifi client scan
        printf("starting scan\n");

        // Add an handler function to handle message reception
        ScanHdlrRef=le_wifiClient_AddNewEventHandler(WifiClientScanEventHandler, NULL);


        if (LE_OK != (result= le_wifiClient_Scan()))
        {
            printf("ERROR: le_wifiClient_Scan returns %d\n", result);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "create") == 0)
    { // >wifi client create "SSID"
        const char * ssid_string = le_arg_GetArg(2);

        if ( NULL == ssid_string )
        {
            printf("ERROR: Missing argument\n");
            exit(EXIT_FAILURE);
        }

        le_wifiClient_AccessPointRef_t createdAccessPoint =
            le_wifiClient_Create ( (const uint8_t *) ssid_string, strlen(ssid_string) );

        if (NULL != createdAccessPoint)
        {
            printf("Created %s has reference %p\n", ssid_string, createdAccessPoint);
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("le_wifiClient_Create returns NULL\n");
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "delete") == 0)
    {
        // wifi client delete [REF]
        const char * arg2 = le_arg_GetArg(2);
        le_wifiClient_AccessPointRef_t accessPointRef = NULL;

        if ( NULL == arg2 )
        {
            printf("ERROR. Missing argument\n");
            exit(EXIT_FAILURE);
        }

        rc1 = sscanf(arg2, "%x", (unsigned int *)&accessPointRef);

        if ((1 == rc1) && (LE_OK == le_wifiClient_Delete(accessPointRef)))
        {
            printf("Successful deletion\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiClient_Delete returns ERROR\n");
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "connect") == 0)
    { // wifi client connect [REF]
        const char * arg2 = le_arg_GetArg(2);
        le_wifiClient_AccessPointRef_t accessPointRef = NULL;


        if ( NULL == arg2 )
        {
            printf("ERROR: Missing argument\n");
            exit(EXIT_FAILURE);
        }
        rc1 = sscanf(arg2, "%x", (unsigned int *)&accessPointRef);

        // Add an handler function to handle message reception
        ConnectHdlrRef = le_wifiClient_AddNewEventHandler(WifiClientConnectEventHandler, NULL);

        if ((1 == rc1) && (LE_OK == (result= le_wifiClient_Connect(accessPointRef))))
        {
            printf("Connecting\n");
        }
        else
        {
            printf("ERROR: le_wifiClient_Connect returns error code %d\n", result);
            le_wifiClient_RemoveNewEventHandler( ConnectHdlrRef );
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setsecurityproto") == 0)
    {
        // wifi client setsecurityproto [REF] [SecuritProto]
        const char * arg2 = le_arg_GetArg(2);
        const char * arg3 = le_arg_GetArg(3);
        le_wifiClient_AccessPointRef_t accessPointRef = NULL;
        le_wifiClient_SecurityProtocol_t securityProtocol = 0;


        if ( ( NULL == arg2 ) || ( NULL == arg3 ) )
        {
            printf("ERROR. Missing argument\n");
            exit(EXIT_FAILURE);
        }

        rc1 = sscanf(arg2, "%x", (unsigned int *)&accessPointRef);
        rc2 = sscanf(arg3, "%u", &securityProtocol);
        printf("wifi client le_wifiClient_SetSecurityProtocol  %p \n", accessPointRef);


        if ((1 == rc1) && (1 == rc2) && (LE_OK == le_wifiClient_SetSecurityProtocol(accessPointRef, securityProtocol)))
        {
            printf("Successfully set security protocol\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiClient_SetSecurityProtocol returns ERROR\n");
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setpassphrase") == 0)
    { // wifi client setpassphrase [REF] [passPhrase]
        const char * arg2 = le_arg_GetArg(2);
        const char * passPhrase = le_arg_GetArg(3);
        le_wifiClient_AccessPointRef_t accessPointRef = NULL;

        if ( ( NULL == arg2 ) || ( NULL == passPhrase ) )
        {
            printf("ERROR: Missing argument\n");
            exit(EXIT_FAILURE);
        }

        rc1 = sscanf(arg2, "%x", (unsigned int *)&accessPointRef);

        if ((1 == rc1) && (LE_OK == (result=le_wifiClient_SetPassphrase(accessPointRef, passPhrase))))
        {
            printf("le_wifiClient_SetPassphrase returns OK\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiClient_SetPassphrase returns %d\n", result);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setpsk") == 0)
    { // wifi client setpsk [REF] [PSK]
        const char * arg2 = le_arg_GetArg(2);
        const char * psk = le_arg_GetArg(3);
        le_wifiClient_AccessPointRef_t accessPointRef = NULL;

        if ( ( NULL == arg2 ) || ( NULL == psk ) )
        {
            printf("ERROR: Missing argument\n");
            exit(EXIT_FAILURE);
        }

        rc1 = sscanf(arg2, "%x", (unsigned int *)&accessPointRef);

        if ((1 == rc1) && (LE_OK == le_wifiClient_SetPreSharedKey(accessPointRef, psk)))
        {
            printf("PSK set sucessfully\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("le_wifiClient_SetPreSharedKey returns ERROR\n");
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setusercred") == 0)
    { // wifi client setusercred [REF] [userName] [password]
        const char * arg2 = le_arg_GetArg(2);
        const char * username = le_arg_GetArg(3);
        const char * password = le_arg_GetArg(4);
        le_wifiClient_AccessPointRef_t accessPointRef = NULL;

        if ( ( NULL == arg2 ) || ( NULL == username ) )
        {
            printf("ERROR: Missing argument\n");
            exit(EXIT_FAILURE);
        }

        rc1 = sscanf(arg2, "%x", (unsigned int *)&accessPointRef);

        if ((1 == rc1) && (LE_OK == (result=le_wifiClient_SetUserCredentials(accessPointRef, username, password))))
        {
            printf("Successfully set user credentials\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiClient_SetUserCredentials returns %d\n", result);
            exit(EXIT_FAILURE);
        }
    }
    else if (strcmp(commandPtr, "setwepkey") == 0)
    { // wifi client setwepkey [REF] [WEPKEY]
        const char * arg2 = le_arg_GetArg(2);
        const char * wepkey = le_arg_GetArg(3);
        le_wifiClient_AccessPointRef_t accessPointRef = NULL;

        if ( ( NULL == arg2 ) || ( NULL == wepkey ) )
        {
            printf("ERROR: Missing argument\n");
            exit(EXIT_FAILURE);
        }

        rc1 = sscanf(arg2, "%x", (unsigned int *)&accessPointRef);

        if ((1 == rc1) && (LE_OK == (result=le_wifiClient_SetPreSharedKey(accessPointRef, wepkey))))
        {
            printf("wepkey set sucessfully\n");
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("ERROR: le_wifiClient_SetPreSharedKey returns %d\n", result);
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        printf("ERROR: Invalid command for wifi service.\n");
        exit(EXIT_FAILURE);
    }
}


