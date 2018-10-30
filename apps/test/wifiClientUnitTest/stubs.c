/**
 * @file stubs.c
 *
 * Stub functions required for WiFi client unit test
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#include "legato.h"
#include "interfaces.h"

//--------------------------------------------------------------------------------------------------
/**
 * Event handler for PA WiFi access point changes.
 *
 * Handles the PA WiFi events.
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
 * Struct to hold the AccessPoint from the Scan's data.
 *
 */
//-------------------------------------------------------------------------------------------------
typedef struct
{
    pa_wifiClient_AccessPoint_t accessPoint;
    bool                        foundInLatestScan;
}
FoundAccessPoint_t;

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to initialize the PA WiFi Module.
 *
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_Init
(
    void
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Start WiFi Client PA
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_Start
(
    void
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Stop WiFi Client PA
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_Stop
(
    void
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function connects a wifiClient.
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_Connect
(
    uint8_t ssidBytes[LE_WIFIDEFS_MAX_SSID_BYTES],
        ///< [IN]
        ///< Contains ssidLength number of bytes
    uint8_t ssidLength
        ///< [IN]
        ///< The number of Bytes in the ssidBytes
)
{
    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * This function disconnects a wifiClient.
 *
 * @return LE_FAULT  The function failed.
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_Disconnect
(
    void
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Clears all username, password, PreShared Key, passphrase settings previously made by
 *
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_ClearAllCredentials
(
    void
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the security protocol for communication.
 *
 * @return LE_BAD_PARAMETER  The function failed due to an invalid parameter.
 * @return LE_OK             The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_SetSecurityProtocol
(
    const le_wifiClient_SecurityProtocol_t securityProtocol
        ///< [IN]
        ///< Security protocol used for communication.
)
{
    le_result_t result = LE_BAD_PARAMETER;

    switch (securityProtocol)
    {
        case LE_WIFICLIENT_SECURITY_NONE:
        case LE_WIFICLIENT_SECURITY_WEP:
        case LE_WIFICLIENT_SECURITY_WPA_PSK_PERSONAL:
        case LE_WIFICLIENT_SECURITY_WPA2_PSK_PERSONAL:
        case LE_WIFICLIENT_SECURITY_WPA_EAP_PEAP0_ENTERPRISE:
        case LE_WIFICLIENT_SECURITY_WPA2_EAP_PEAP0_ENTERPRISE:
            result = LE_OK;
            break;

        default:
            result = LE_BAD_PARAMETER;
            break;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the PassPhrase used to create PSK (WPA-Personal).
 *
 * @see  pa_wifiClient_SetPreSharedKey
 *
 * @return LE_BAD_PARAMETER  The function failed due to an invalid parameter.
 * @return LE_OK             The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_SetPassphrase
(
    const char *passphrasePtr
        ///< [IN]
        ///< Passphrase used for authentication
)
{
    uint32_t length;
    // Store Passphrase to be used later during connection procedure
    le_result_t result = LE_BAD_PARAMETER;

    LE_INFO("Set passphrase");
    if (NULL != passphrasePtr)
    {
        length = strlen(passphrasePtr);

        LE_INFO("Set passphrase");
        if ((LE_WIFIDEFS_MIN_PASSPHRASE_LENGTH <= length) &&
            (length <= LE_WIFIDEFS_MAX_PASSPHRASE_LENGTH))
        {
           result = LE_OK;
        }
        else
        {
            LE_ERROR("Invalid passphrase length (%d) [%d..%d]",
                length,
                LE_WIFIDEFS_MIN_PASSPHRASE_LENGTH,
                LE_WIFIDEFS_MAX_PASSPHRASE_LENGTH);
        }
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the WiFi pre-shared key (WPA-Personal)
 *
 * @see  pa_wifiClient_SetPassPhrase
 *
 * @return LE_BAD_PARAMETER  The function failed due to an invalid parameter.
 * @return LE_OK             The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_SetPreSharedKey
(
    const char *preSharedKeyPtr
        ///< [IN]
        ///< Pre-shared key (PSK) used for authentication
)
{
    le_result_t result = LE_BAD_PARAMETER;

    LE_INFO("Set PSK");
    if (NULL != preSharedKeyPtr)
    {
       result = LE_OK;
    }

    return result;
}

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
void pa_wifiClient_SetHiddenNetworkAttribute
(
    bool hidden
        ///< [IN]
        ///< If TRUE, the WIFI client will be able to connect to a hidden access point.
)
{
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called after the pa_wifiClient_Scan() has been done.
 * It signals that the scan results are no longer needed and frees some internal resources.
 *
 * @return LE_OK     The function succeeded.
 * @return LE_FAULT  The scan failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_ScanDone
(
    void
)
{
    return LE_OK;
}

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
le_result_t pa_wifiClient_Scan
(
    void
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function can be called after pa_wifi_Scan.
 * When the reading is done, it no longer returns LE_OK,
 * pa_wifiClient_ScanDone MUST be called.
 *
 * @return LE_NOT_FOUND  There is no more AP:s found.
 * @return LE_OK     The function succeeded.
 * @return LE_FAULT  The function failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_GetScanResult
(
    pa_wifiClient_AccessPoint_t *accessPointPtr
    ///< [IN][OUT]
    ///< Structure provided by calling function.
    ///< Results filled out if result was LE_OK.
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the WEP key (Wired Equivalent Privacy)
 *
 * @note WEP is unsecure and has been deprecated by the WiFi alliance. Still, it can be used in
 * user mode.
 *
 * @return LE_BAD_PARAMETER  The function failed due to an invalid parameter.
 * @return LE_OK             The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_SetWepKey
(
    const char *wepKeyPtr
        ///< [IN]
        ///< Wired Equivalent Privacy (WEP) key used for authentication
)
{
    le_result_t result = LE_BAD_PARAMETER;

    if (NULL != wepKeyPtr)
    {
       result = LE_OK;
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the username and password (WPA-Entreprise).
 *
 * @return LE_BAD_PARAMETER  The function failed due to an invalid parameter.
 * @return LE_OK             The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_SetUserCredentials
(
    const char *usernamePtr,
        ///< [IN]
        ///< Username used for authentication
    const char *passwordPtr
        ///< [IN]
        ///< Password used for authentication
)
{
    // Store User Credentials to be used later during connection procedure
    le_result_t result = LE_BAD_PARAMETER;

    LE_INFO("Set user credentials");
    if (NULL != usernamePtr)
    {
       result = LE_OK;
    }
    else
    {
        return LE_BAD_PARAMETER;
    }

    if (NULL != passwordPtr)
    {
       result = LE_OK;
    }
    else
    {
        return LE_BAD_PARAMETER;
    }
    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for PA EVENT 'le_wifiClient_Event_t'
 *
 * This event provides information on PA WiFi Client event changes.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiClient_AddEventHandler
(
    pa_wifiClient_NewEventHandlerFunc_t handlerPtr,
        ///< [IN]
        ///< Event handler function pointer.
    void *contextPtr
        ///< [IN]
        ///< Associated event context.
)
{
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the server service reference
 */
//--------------------------------------------------------------------------------------------------
le_msg_ServiceRef_t le_wifiClient_GetServiceRef
(
    void
)
{
    return NULL;
};

//--------------------------------------------------------------------------------------------------
/**
 * Get the client session reference for the current message
 */
//--------------------------------------------------------------------------------------------------
le_msg_SessionRef_t le_wifiClient_GetClientSessionRef
(
    void
)
{
    return (le_msg_SessionRef_t)0x1001;
}

//--------------------------------------------------------------------------------------------------
/**
 * Registers a function to be called whenever one of this service's sessions is closed by
 * the client.  (STUBBED FUNCTION)

 */
//--------------------------------------------------------------------------------------------------
le_msg_SessionEventHandlerRef_t MyAddServiceCloseHandler
(
    le_msg_ServiceRef_t             serviceRef, ///< [IN] Reference to the service.
    le_msg_SessionEventHandler_t    handlerFunc,///< [IN] Handler function.
    void*                           contextPtr  ///< [IN] Opaque pointer value to pass to handler.
)
{
    return NULL;
}