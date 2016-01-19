// -------------------------------------------------------------------------------------------------
/**
 *  Wifi Client Platform Adapter
 *
 *  Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 *
 */
// -------------------------------------------------------------------------------------------------
#ifndef PA_WIFI_H
#define PA_WIFI_H

#include "legato.h"
#include "interfaces.h"

//--------------------------------------------------------------------------------------------------
/**
 * AccessPoint structure.
 */
//--------------------------------------------------------------------------------------------------
typedef struct {
    int16_t signalStrength; ///< if 0xffff, then the value is was not found.
    uint8_t ssidBytes[ LE_WIFIDEFS_MAX_SSID_BYTES ]; ///< Contains ssidLength number of bytes
    uint8_t ssidLength; ///< The number of Bytes in the ssidBytes
}
pa_wifiClient_AccessPoint_t;

//--------------------------------------------------------------------------------------------------
/**
 * Event Handler for PA Wifi Client changes
 *
 * @param event
 *        Handles the PA  wifi events.
 *        Two events are directly generated from the PA
 *            LE_WIFICLIENT_EVENT_CONNECTED,
 *            LE_WIFICLIENT_EVENT_DISCONNECTED,
 * @param contextPtr
 */
//--------------------------------------------------------------------------------------------------
typedef void (*pa_wifiClient_NewEventHandlerFunc_t)
(
    le_wifiClient_Event_t event,
    void* contextPtr
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for PA EVENT 'le_wifiClient_Event_t'
 *
 * This event provide information on PA Wifi Client event changes.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t  pa_wifiClient_AddEventHandler
(
    pa_wifiClient_NewEventHandlerFunc_t handlerPtr,
        ///< [IN]

    void* contextPtr
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to initialize the PA WIFI Module.
 *
 * @return LE_OK     The function succeed.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_Init
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to release the PA WIFI Module.
 *
 * @return LE_OK     The function succeed.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_Release
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * This function will start a scan and returns when it is done.
 * It should NOT return until the scan is done and results can be read with GetScanResult
 * Results are read via pa_wifiClient_GetScanResult.
 * When the reading is done pa_wifiClient_ScanDone MUST be called.*
 *
 * @return LE_FAULT  The function failed.
 * @return LE_BUSY   The function is already ongoing.
 * @return LE_OK     The function succeed.*/
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_Scan
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * This function to find out if scan is currently running
 * @return TRUE  Scan is running.
 * @return FALSE Scan is not running
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED bool pa_wifiClient_IsScanRunning
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * This function can be called after pa_wifi_Scan.
 * When the reading is done, that it no longer returns LE_OK, pa_wifiClient_ScanDone MUST be called.
 * @return LE_NOT_FOUND  There is no more AP:s found.
 * @return LE_OK     The function succeed.
 * @return LE_FAULT  The function failed.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_GetScanResult
(
    pa_wifiClient_AccessPoint_t * accessPointPtr
    ///< [IN][OUT]
    ///< Structure provided by calling function.
    ///< Results filled out if resullt was LE_OK
);

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called after the pa_wifiClient_Scan() has been done.
 * It signals that the scan results are no longer needed and frees some internal resourses.
 *
 * @return LE_OK     The function succeed.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_ScanDone
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * This function connects a wifiClient.
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.*/
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_Connect
(
    uint8_t ssidBytes[LE_WIFIDEFS_MAX_SSID_BYTES], ///< Contains ssidLength number of bytes
    uint8_t ssidLength ///< The number of Bytes in the ssidBytes
);

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
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the username and password (WPA-Entreprise).
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.*/
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_SetUserCredentials
(
    const char* username,
    const char* password
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the PassPhrase used to create PSK (WPA-Personal).
 * @see  pa_wifiClient_SetPreSharedKey
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.*/
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_SetPassphrase
(
    const char* passphrase
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the PreSharedKey (WPA-Personal)
 * @see  pa_wifiClient_SetPassPhrase
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.*/
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_SetPreSharedKey
(
    const char* preSharedKey
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the WEP key (WEP)
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.*/
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_SetWepKey
(
    const char* wepKey
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the security protocol for connection
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.*/
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_SetSecurityProtocol
(
    const le_wifiClient_SecurityProtocol_t securityProtocol
);

//--------------------------------------------------------------------------------------------------
/**
 * Clears all username, password, PreShared Key, passphrase settings previously made by
 * @see  pa_wifiClient_SetPassPhrase
 * @see  pa_wifiClient_SetPreSharedKey
 * @see  pa_wifiClient_SetUserCredentials
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_ClearAllCredentials
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Start Wifi Client PA
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.*/
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_Start
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Stop Wifi Client PA
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeed.*/
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_Stop
(
    void
);
#endif // PA_WIFI_H
