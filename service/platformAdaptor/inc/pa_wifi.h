// -------------------------------------------------------------------------------------------------
/**
 *  WiFi Client Platform Adapter
 *
 *  Copyright (C) Sierra Wireless Inc.
 *
 */
// -------------------------------------------------------------------------------------------------
#ifndef PA_WIFI_H
#define PA_WIFI_H

#include "legato.h"
#include "interfaces.h"

//--------------------------------------------------------------------------------------------------
/**
 * Return value from WiFi platform adapter code.
 * 8 is returned if WiFi connection can not be established within a given time
 * 14 is returned if wpa_supplicant is running
 * 50 is returned if WiFi hardware is absent.
 * 100 is returned if WiFi driver can not be installed.
 */
//--------------------------------------------------------------------------------------------------
#define PA_TIMEOUT          8
#define PA_DUPLICATE        14
#define PA_NOT_FOUND        50
#define PA_NOT_POSSIBLE     100
//--------------------------------------------------------------------------------------------------
/**
 * AccessPoint structure.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    int16_t  signalStrength;                        ///< LE_WIFICLIENT_NO_SIGNAL_STRENGTH means
                                                    ///< value was not found.
    uint8_t  ssidLength;                            ///< The number of bytes in the ssidBytes.
    uint8_t  ssidBytes[LE_WIFIDEFS_MAX_SSID_BYTES]; ///< Contains ssidLength number of bytes.
    char     bssid[LE_WIFIDEFS_MAX_BSSID_BYTES];    ///< Contains the bssid.
} pa_wifiClient_AccessPoint_t;

//--------------------------------------------------------------------------------------------------
/**
 * Event handler for PA WiFi access point changes.
 *
 * Handles the PA WiFi events.
 *
 * @deprecated pa_wifiClient_AddEventHandler() should not be used anymore.
 * It has been replaced by pa_wifiClient_AddEventIndHandler().
 */
//--------------------------------------------------------------------------------------------------
typedef void (*pa_wifiClient_NewEventHandlerFunc_t)
(
    le_wifiClient_Event_t event,
        ///< [IN]
        ///< WiFi event to process
    void *contextPtr
        ///< [IN]
        ///< Associated WiFi event context
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for PA EVENT 'le_wifiClient_Event_t'
 *
 * This event provides information on PA WiFi Client event changes.
 *
 * @deprecated pa_wifiClient_AddEventHandler() should not be used anymore.
 * It has been replaced by pa_wifiClient_AddEventIndHandler().
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_AddEventHandler
(
    pa_wifiClient_NewEventHandlerFunc_t handlerPtr,
        ///< [IN]
        ///< Event handler function pointer.
    void *contextPtr
        ///< [IN]
        ///< Associated event context.
);

//--------------------------------------------------------------------------------------------------
/**
 * Event handler for PA WiFi connection changes.
 *
 * Handles the PA WiFi events.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*pa_wifiClient_EventIndHandlerFunc_t)
(
    le_wifiClient_EventInd_t* wifiEventIndPtr,
        ///< [IN]
        ///< WiFi event pointer to process
    void *contextPtr
        ///< [IN]
        ///< Associated WiFi event context
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for PA EVENT 'le_wifiClient_EventInd_t'
 *
 * This event provides information on PA WiFi Client event changes.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_AddEventIndHandler
(
    pa_wifiClient_EventIndHandlerFunc_t handlerPtr,
        ///< [IN]
        ///< Event handler function pointer.
    void *contextPtr
        ///< [IN]
        ///< Associated event context.
);

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to initialize the PA WiFi Module.
 *
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_Init
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to release the PA WiFi Module.
 *
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_Release
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * This function will start a scan and returns when it is done.
 * It should NOT return until the scan is done.
 * Results are read via pa_wifiClient_GetScanResult.
 * When the reading is done pa_wifiClient_ScanDone MUST be called.
 *
 * @return LE_FAULT  The function failed.
 * @return LE_BUSY   The function is already ongoing.
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_Scan
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * This function is used to find out if a scan is currently running.
 *
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
 * When the reading is done, it no longer returns LE_OK
 * pa_wifiClient_ScanDone MUST be called.
 *
 * @return LE_NOT_FOUND  There is no more AP found.
 * @return LE_OK         The function succeeded.
 * @return LE_FAULT      The function failed.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_GetScanResult
(
    pa_wifiClient_AccessPoint_t *accessPointPtr,
    ///< [IN][OUT]
    ///< Structure provided by calling function.
    ///< Results filled out if result was LE_OK.
    char scanIfName[]
    ///< [IN][OUT]
    ///< Array provided by calling function.
    ///< Store WLAN interface used for scan.
);

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called after the pa_wifiClient_Scan() has been done.
 * It signals that the scan results are no longer needed and frees some internal resourses.
 *
 * @return LE_OK     The function succeeded.
 * @return LE_FAULT  The scan failed
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
 * @return LE_FAULT             The function failed.
 * @return LE_BAD_PARAMETER     Invalid parameter.
 * @return LE_DUPLICATE         Duplicated request.
 * @return LE_TIMEOUT           Connection request time out.
 * @return LE_OK                The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_Connect
(
    uint8_t ssidBytes[LE_WIFIDEFS_MAX_SSID_BYTES],
        /// [IN]
        ///< Contains ssidLength number of bytes
    uint8_t ssidLength
        /// [IN]
        ///< The number of bytes in the ssidBytes
);

//--------------------------------------------------------------------------------------------------
/**
 * This function disconnects a wifiClient.
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeeded.
 */
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
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_SetUserCredentials
(
    const char *usernamePtr,
        ///< [IN]
        ///< Username used for authentication
    const char *passwordPtr
        ///< [IN]
        ///< Password used for authentication
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the PassPhrase used to create PSK (WPA-Personal).
 * @see  pa_wifiClient_SetPreSharedKey
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_SetPassphrase
(
    const char *passphrasePtr
        ///< [IN]
        ///< Passphrase used for authentication
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the PreSharedKey (WPA-Personal)
 * @see  pa_wifiClient_SetPassPhrase
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_SetPreSharedKey
(
    const char *preSharedKeyPtr
        ///< [IN]
        ///< Pre-shared key (PSK) used for authentication
);

//--------------------------------------------------------------------------------------------------
/**
 * This function specifies whether the target Access Point is hiding its presence from clients or
 * not. When an Access Point is hidden, it cannot be discovered by a scan process.
 *
 * @note By default, this attribute is not set which means that the client is unable to connect to
 * a hidden access point. When enabled, the client will be able to connect to the access point
 * whether it is hidden or not.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void pa_wifiClient_SetHiddenNetworkAttribute
(
    bool hidden
        ///< [IN]
        ///< If TRUE, the WIFI client will be able to connect to a hidden access point.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the WEP key (WEP)
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_SetWepKey
(
    const char *wepKeyPtr
        ///< [IN]
        ///< Wired Equivalent Privacy (WEP) key used for authentication
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the security protocol for connection
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_SetSecurityProtocol
(
    const le_wifiClient_SecurityProtocol_t securityProtocol
        ///< [IN]
        ///< Security protocol used for communication.
);

//--------------------------------------------------------------------------------------------------
/**
 * Clears all username, password, pre-shared key, passphrase settings previously made by
 * @see  pa_wifiClient_SetPassPhrase
 * @see  pa_wifiClient_SetPreSharedKey
 * @see  pa_wifiClient_SetUserCredentials
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_ClearAllCredentials
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Start WiFi Client PA
 *
 * @return LE_FAULT         The function failed.
 * @return LE_OK            The function succeeded.
 * @return LE_NOT_FOUND     The WiFi card is absent.
 * @return LE_UNAVAILABLE   The WiFi card may not work.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_Start
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Stop WiFi Client PA
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiClient_Stop
(
    void
);
#endif // PA_WIFI_H
