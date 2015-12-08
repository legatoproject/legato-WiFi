//TODO: place this file in a good subdirectory.
// -------------------------------------------------------------------------------------------------
/**
 *  stub
 *
 *  Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 *
 */
// -------------------------------------------------------------------------------------------------
#include "legato.h"

#include "interfaces.h"

#include "pa_wifi.h"
#include "stdio.h"
// string to find trace in logtrace
#define PA_WIFIDEBUG "PA WIFI DEBUG:"


// Command to init the hardware
#define COMMAND_WIFI_HW_START "/etc/init.d/tiwifi start"
#define COMMAND_WIFI_HW_STOP  "/etc/init.d/tiwifi stop" // not sure that this works.
#define COMMAND_WIFI_WLAN_UP  "/sbin/ifconfig wlan0 up"
#define COMMAND_WIFI_START_SCAN  "/usr/sbin/iw dev wlan0 scan |grep 'SSID\\|signal'"

// WEP
static char SavedWepKey[LE_WIFIDEFS_MAX_WEPKEY_BYTES];
// WPA-Personal
static char SavedPassphrase[LE_WIFIDEFS_MAX_PASSPHRASE_BYTES];
static char SavedPreSharedKey[LE_WIFIDEFS_MAX_PSK_BYTES];
// WPA-Entreprise
static char SavedUsername[LE_WIFIDEFS_MAX_USERNAME_BYTES];
static char SavedPassword[LE_WIFIDEFS_MAX_PASSWORD_BYTES];


static FILE *IwScanFp = NULL;
static FILE *IwConnectFp = NULL;
static bool IsScanRunning = false;

static pa_wifiClient_NewEventHandlerFunc_t CallBackHandlerFuncPtr = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to initialize the PA WIFI Module.
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_Init
(
    void
)
{
    le_result_t result = LE_FAULT;
    int16_t systemResult =  system( COMMAND_WIFI_HW_START);
    /**
     * Return value of -1 means that the fork() has failed (see man system).
     * The script /etc/init.d/tiwifi returns 0 if the kernel modules are loaded correctly
     * and the wlan0 interface is seen,
     * 127 if modules not loaded or interface not seen,
     * 1 if the option passed if unknown (start stop and restart).
     */
    if ( systemResult == 0)
    {
        LE_INFO( PA_WIFIDEBUG "Wifi Client Command OK:" COMMAND_WIFI_HW_START);
        result = LE_OK;
    }
    else
    {
        LE_WARN( PA_WIFIDEBUG "Wifi Client Command Failed: (%d)" COMMAND_WIFI_HW_START, systemResult);
        result = LE_FAULT;
    }

    if ( LE_OK == result)
    {
        systemResult = system(COMMAND_WIFI_WLAN_UP);
        if ( 0 != systemResult )
        {
            LE_WARN( PA_WIFIDEBUG "/sbin/ifconfig wlan0 up  FAILED %d", systemResult);
            result = LE_FAULT;
        }
        else
        {
            LE_INFO( PA_WIFIDEBUG "/sbin/ifconfig wlan0 up  OK!");
            result = LE_OK;
        }
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to release the PA WIFI Module.
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_Release
(
    void
)
{
    //TODO: Quite sure it does not work to release the driver script.
//    LE_DEBUG("Execute '/etc/init.d/start_tiwifi.sh stop'");
//    if ( system("/etc/init.d/start_tiwifi.sh stop") == -1 )
//    {
//        LE_WARN("system '%s' failed", systemCmd);
//        return LE_FAULT;
//    }

    return LE_OK;
}


LE_SHARED le_result_t pa_wifiClient_Scan
(
    void
)
{
    le_result_t result = LE_OK;
    LE_INFO( PA_WIFIDEBUG "pa_wifiClient_Scan");
    if( IsScanRunning )
    {
        LE_ERROR( PA_WIFIDEBUG "pa_wifi_Scan: Scan is already running");
        return LE_BUSY;
    }

    if( NULL != IwScanFp )
        return LE_BUSY;

    IsScanRunning = true;
    /* Open the command for reading. */
    IwScanFp = popen( COMMAND_WIFI_START_SCAN, "r");

    if (IwScanFp == NULL)
    {
        LE_ERROR( PA_WIFIDEBUG "pa_wifi_Scan: Failed to run command: errno:%d: \"%s\" Cmd:"
                                COMMAND_WIFI_START_SCAN,
                                errno,
                                strerror(errno)
                                );
        result = LE_FAULT;
    }

    IsScanRunning = false;
    return result;
}

bool pa_wifiClient_IsScanRunning
(
)
{
    return IsScanRunning;
}

LE_SHARED le_result_t pa_wifiClient_GetScanResult
(
    pa_wifiClient_AccessPoint_t * accessPointPtr
    ///< [IN][OUT]
    ///< Structure provided by calling function.
    ///< Results filled out if resullt was LE_OK
)
{
    char path[1035];


    LE_INFO( PA_WIFIDEBUG "pa_wifi_GetScanResult");

    if (IwScanFp == NULL)
    {
       LE_ERROR( PA_WIFIDEBUG "le_wifiClient_Scan: ERROR must call pa_wifi_Scan first");
       return LE_FAULT;
    }
    if (accessPointPtr == NULL)
    {
       LE_ERROR( PA_WIFIDEBUG "le_wifiClient_Scan: ERROR : accessPoint == NULL");
       return LE_BAD_PARAMETER ;
    }

    /*default values*/
    accessPointPtr->signalStrength = 0xffff;
    accessPointPtr->ssidLength = 0;

    /* Read the output a line at a time - output it. */
    while( fgets(path, sizeof(path)-1, IwScanFp) != NULL )
    {

        LE_INFO( PA_WIFIDEBUG "PARSING:%s: len:%d", path, strnlen(path,sizeof(path)-1) );

        if( 0 == strncmp("\tSSID: ", path, 7)){
            accessPointPtr->ssidLength = strnlen(path, LE_WIFIDEFS_MAX_SSID_BYTES) - 7 -1;
            LE_INFO( PA_WIFIDEBUG "FOUND SSID:%s  %c%c.. ", path, path[7], path[8]  );
            memset( &accessPointPtr->ssidBytes, 0, LE_WIFIDEFS_MAX_SSID_BYTES );
            memcpy ( &accessPointPtr->ssidBytes, &path[7], accessPointPtr->ssidLength );
            LE_INFO( PA_WIFIDEBUG "FOUND SSID: Parsed:\"%s\"", &accessPointPtr->ssidBytes[0] );
            return LE_OK;
        }
        else if( 0 == strncmp("\tsignal: ", path, 9)){
            LE_INFO( PA_WIFIDEBUG "FOUND SIGNAL STRENGTH:%s  %c %c ", path, path[10], path[11]  );
            accessPointPtr->signalStrength = atoi( &path[9] );
            LE_INFO( PA_WIFIDEBUG "FOUND SIGNAL STRENGTH: signalStrength:%d ",
            accessPointPtr->signalStrength );
        }
    }

    return LE_NOT_FOUND;

}

LE_SHARED le_result_t pa_wifiClient_ScanDone
(
    void
)
{
    if( NULL != IwScanFp )
    {
        pclose(IwScanFp);
        IwScanFp = NULL;
        IsScanRunning = false;
    }
    return LE_OK;
}





LE_SHARED le_result_t pa_wifiClient_Connect
(
    uint8_t ssidBytes[LE_WIFIDEFS_MAX_SSID_BYTES], ///< Contains ssidLength number of bytes
    uint8_t ssidLength ///< The number of Bytes in the ssidBytes
)
{
    int16_t systemResult;
    char tmpstring[255];
    char path[1035];
    le_result_t result = LE_OK;

    LE_INFO( PA_WIFIDEBUG "pa_wifiClient_Connect SSID length %d SSID: \"%.*s\"",
        ssidLength,
        ssidLength,
        (char*) ssidBytes);

    // Which type of Wifi client connection is requested?
    if ( (0 == strlen(SavedWepKey)) &&
         (0 == strlen(SavedPassphrase)) &&
         (0 == strlen(SavedPreSharedKey)) &&
         (0 == strlen(SavedUsername)) &&
         (0 == strlen(SavedPassword)) )
    {
        // Opened Wifi hotspot (SSID with no password)
        sprintf(tmpstring, "/usr/sbin/iw dev wlan0 connect \"%.*s\"",
            ssidLength,
            (char*) ssidBytes);

        systemResult = system(tmpstring);
        if ( 0 != systemResult )
        {
            LE_WARN( PA_WIFIDEBUG "CMD: %s  FAILED %d", tmpstring, systemResult);
            result = LE_FAULT;
        }
        else
        {
            LE_INFO( PA_WIFIDEBUG "CMD: %s  OK!", tmpstring);
            result = LE_OK;
        }
    } else if (0 != strlen(SavedPassword))
    {
        // WEP connection (SSID with WEP key)
        sprintf(tmpstring, "/usr/sbin/iw dev wlan0 connect \"%.*s\" keys 0:\"%.*s\"",
            ssidLength,
            (char*) ssidBytes,
            strlen(SavedWepKey),
            (char*) SavedWepKey);

        systemResult = system(tmpstring);
        if ( 0 != systemResult )
        {
            LE_WARN( PA_WIFIDEBUG "CMD: %s  FAILED %d", tmpstring, systemResult);
            result = LE_FAULT;
        }
        else
        {
            LE_INFO( PA_WIFIDEBUG "CMD: %s  OK!", tmpstring);
            result = LE_OK;
        }
    } else if (0 != strlen(SavedPassphrase) || (0 != strlen(SavedPreSharedKey)))
    {
        // WPA/WPA2 connection (SSID with passphrase)
        // 1. Create the config file /tmp/wpa_supplicant.conf
        // and set option to be able to connect to hidden AP
        // Add the following lines to /tmp/wpa_supplicant.conf
        // update_config=1
        // ap_scan=2
        // ctrl_interface=/sbin/wpa_supplicant

        FILE * configFilePtr = fopen("/tmp/wpa_supplicant.conf", "w");
        fwrite("ap_scan=2\n" ,1, strlen("ap_scan=2\n") , configFilePtr);
        fclose(configFilePtr);

        /*IwConnectFp = popen(tmpstring, "w");

        if (IwConnectFp == NULL)
        {
            LE_ERROR( PA_WIFIDEBUG "pa_wifi_Connect: Failed to run command:\"%s\" errno:%d %s",
                                (tmpstring),
                                errno,
                                strerror(errno));
            result = LE_FAULT;
        }*/

        // 2. Passphrase/PSK.
        sprintf(tmpstring,"/sbin/wpa_passphrase \"%.*s\" %s >> /tmp/wpa_supplicant.conf",
            ssidLength,
            (char*) ssidBytes,
            SavedPassphrase);

        systemResult = system(tmpstring);
        if ( 0 != systemResult )
        {
            LE_WARN( PA_WIFIDEBUG "CMD: %s  FAILED %d", tmpstring, systemResult);
            result = LE_FAULT;
        }
        else
        {
            LE_INFO( PA_WIFIDEBUG "CMD: %s  OK!", tmpstring);
            result = LE_OK;
        }
        // 3. Connection: wpa_supplicant -B -i wlan0 -c /tmp/wpa_supplicant.conf
        if ( LE_OK == result)
        {
            systemResult = system("/sbin/wpa_supplicant -B -D nl80211 -i wlan0 -c /tmp/wpa_supplicant.conf");
            if ( 0 != systemResult )
            {
                LE_WARN( PA_WIFIDEBUG "CMD: /sbin/wpa_supplicant -B ...  FAILED %d", systemResult);
                result = LE_FAULT;
            }
            else
            {
                LE_INFO( PA_WIFIDEBUG "CMD: /sbin/wpa_supplicant -B ...  OK!");
                result = LE_OK;
            }
        }
    }
    // Test Connection
    if (LE_OK == result)
    {
        sprintf(tmpstring, "/usr/sbin/iw wlan0 link");
        IwConnectFp = popen(tmpstring, "r");
        if (IwConnectFp == NULL)
        {
            LE_ERROR( PA_WIFIDEBUG "pa_wifi_Connect: Failed to run command:\"%s\" errno:%d %s",
                                (tmpstring),
                                errno,
                                strerror(errno));
            result = LE_FAULT;
        } else {
            /* Read the output a line at a time - output it. */
            while( fgets(path, sizeof(path)-1, IwConnectFp) != NULL )
            {
                LE_INFO( PA_WIFIDEBUG "PARSING:%s: len:%d", path, strnlen(path,sizeof(path)-1) );
                if ( 0 == strncmp("Not connected.", path, strlen("Not connected.")))
                {
                    LE_INFO( PA_WIFIDEBUG "FOUND : Not connected.");
                    return LE_FAULT;
                } else {
                    LE_INFO( PA_WIFIDEBUG "Wifi connected!");
                    return LE_OK;
                }
            }
            pclose(IwConnectFp);
        }
    }
    // If no error on Wifi Client Connection command then obtain IP address and test if connection
    // is well established
    // Obtain IP address by DHCP
    if ( LE_OK == result)
    {
        systemResult = system("/sbin/udhcpc wlan0");
        if ( 0 != systemResult )
        {
            LE_WARN( PA_WIFIDEBUG "CMD: udhcpc wlan0  FAILED %d", systemResult);
            result = LE_FAULT;
        }
        else
        {
            LE_INFO( PA_WIFIDEBUG "CMD: udhcpc wlan0  OK!");
            result = LE_OK;
        }
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function disconnects a wifiClient.
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.*/
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_Disconnect
(
    void
)
{
    //TODO:
    return LE_OK;
}

LE_SHARED le_result_t pa_wifiClient_ClearAllCredentials
(
    void
)
{
    //TODO:
    return LE_OK;
}

LE_SHARED le_result_t pa_wifiClient_SetWepKey
(
    const char* wepKey
)
{
    le_result_t result = LE_BAD_PARAMETER;
    LE_INFO( PA_WIFIDEBUG "pa_wifiClient_SetWepKey");
    if( NULL != wepKey)
    {
       strncpy( &SavedWepKey[0], &wepKey[0], LE_WIFIDEFS_MAX_WEPKEY_LENGTH);
       // Make sure there is a null termination
       SavedWepKey[LE_WIFIDEFS_MAX_WEPKEY_LENGTH] = '\0';
       result = LE_OK;
    }
    return result;
}

LE_SHARED le_result_t pa_wifiClient_SetPreSharedKey
(
    const char* preSharedKey
)
{
    le_result_t result = LE_BAD_PARAMETER;
    LE_INFO( PA_WIFIDEBUG "pa_wifiClient_SetPreSharedKey");
    if( NULL != preSharedKey)
    {
       strncpy( &SavedPreSharedKey[0], &preSharedKey[0], LE_WIFIDEFS_MAX_PSK_LENGTH);
       // Make sure there is a null termination
       SavedPreSharedKey[LE_WIFIDEFS_MAX_PSK_LENGTH] = '\0';
       result = LE_OK;
    }
    return result;
}

LE_SHARED le_result_t pa_wifiClient_SetPassphrase
(
    const char* passphrase
)
{
    // Store Passphrase to be used later during connection procedure
    le_result_t result = LE_BAD_PARAMETER;
    LE_INFO( PA_WIFIDEBUG "pa_wifiClient_SetPreSharedKey");
    if( NULL != passphrase)
    {
       strncpy( &SavedPassphrase[0], &passphrase[0], LE_WIFIDEFS_MAX_PASSPHRASE_LENGTH);
       // Make sure there is a null termination
       SavedPassphrase[LE_WIFIDEFS_MAX_PASSPHRASE_LENGTH] = '\0';
       result = LE_OK;
    }
    return result;
}

// WPA-Entreprise
LE_SHARED le_result_t pa_wifiClient_SetUserCredentials
(
    const char* username,
    const char* password
)
{
    // Store User Credentials to be used later during connection procedure
    le_result_t result = LE_BAD_PARAMETER;
    LE_INFO( PA_WIFIDEBUG "pa_wifiClient_SetUserCredentials");
    if( NULL != username)
    {
       strncpy( &SavedUsername[0], &username[0], LE_WIFIDEFS_MAX_USERNAME_LENGTH);
       // Make sure there is a null termination
       SavedUsername[LE_WIFIDEFS_MAX_USERNAME_LENGTH] = '\0';
       result = LE_OK;
    }
    if( NULL != password)
    {
       strncpy( &SavedPassword[0], &password[0], LE_WIFIDEFS_MAX_PASSWORD_LENGTH);
       // Make sure there is a null termination
       SavedPassword[LE_WIFIDEFS_MAX_PASSWORD_LENGTH] = '\0';
       result = LE_OK;
    }
    return result;
}

LE_SHARED le_result_t  pa_wifiClient_AddEventHandler
(
    pa_wifiClient_NewEventHandlerFunc_t handlerPtr,
        ///< [IN]

    void* contextPtr
        ///< [IN]
)
{
    CallBackHandlerFuncPtr = handlerPtr;
    return LE_OK;
}
