#ifndef PA_WIFI_AP_H
#define PA_WIFI_AP_H
// -------------------------------------------------------------------------------------------------
/**
 *
 *
 *  Copyright (C) Sierra Wireless Inc.
 *
 */
// -------------------------------------------------------------------------------------------------
#include "legato.h"
#include "interfaces.h"

//--------------------------------------------------------------------------------------------------
/**
 * Event handler for PA WiFi access point changes.
 *
 * Handles the PA WiFi events.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*pa_wifiAp_NewEventHandlerFunc_t)
(
    le_wifiAp_Event_t event,
        ///< [IN]
        ///< WiFi event to process
    void *contextPtr
        ///< [IN]
        ///< Associated WiFi event context
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for WiFi related events.
 *
 * These events provide information on WiFi access point status.
 *
 * @return LE_OK Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiAp_AddEventHandler
(
    pa_wifiAp_NewEventHandlerFunc_t handlerPtr,
        ///< [IN]
        ///< Event handler function pointer

    void *contextPtr
        ///< [IN]
        ///< Associated event context
);

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to initialize the PA WiFi Access Point.
 *
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiAp_Init
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to release the platform adaptor WiFi module.
 *
 * @return LE_OK     The function succeeded.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t pa_wifiAp_Release
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * This function starts the WiFi access point.
 * Note that all settings, if to be used, such as security, username, password must set prior to
 * starting the access point.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_OK            The function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_Start
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * This function stops the WiFi access point.
 *
 * @return LE_FAULT         The function failed.
 * @return LE_OK            The function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_Stop
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the security protocol to use.
 *
 * @note WEP is not supported as it is unsecure and has been deprecated in favor of WPA/WPA2.
 * This limitation applies to access point mode only.
 *
 * @return LE_BAD_PARAMETER Some parameter is invalid.
 * @return LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetSecurityProtocol
(
    le_wifiAp_SecurityProtocol_t securityProtocol
        ///< [IN]
        ///< The security protocol used to communicate with the access point.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the Service Set IDentification (SSID) of the access point.
 *
 * @note The SSID does not have to be human readable ASCII values, but often is.
 *
 * @return LE_BAD_PARAMETER Some parameter is invalid.
 * @return LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetSsid
(
    const uint8_t *ssidPtr,
        ///< [IN]
        ///< The SSID to set as an octet array.

    size_t ssidNumElements
        ///< [IN]
        ///< The length of the SSID in octets.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the passphrase used to generate the PSK.
 *
 * @note This is one way to authenticate against the access point. The other one is provided by the
 * le_wifiAp_SetPreSharedKey() function. Both ways are exclusive and are effective only when used
 * with WPA-personal authentication.
 *
 * @return LE_BAD_PARAMETER Parameter is invalid.
 * @return LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetPassPhrase
(
    const char *passphrasePtr
        ///< [IN]
        ///< Passphrase to authenticate against the access point.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the pre-shared key (PSK).
 *
 * @note This is one way to authenticate against the access point. The other one is provided by the
 * le_wifiAp_SetPassPhrase() function. Both ways are exclusive and are effective only when used
 * with WPA-personal authentication.
 *
 * @return LE_BAD_PARAMETER Parameter is invalid.
 * @return LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetPreSharedKey
(
    const char *preSharedKeyPtr
        ///< [IN]
        ///< Pre-shared key used to authenticate against the access point.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set if the Access Point should announce it's presence.
 * Default value is TRUE.
 * If the value is set to FALSE, the access point will be hidden.
 *
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetDiscoverable
(
    bool isDiscoverable
        ///< [IN]
        ///< If TRUE, the access point SSID is visible by the clients otherwise it is hidden.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the WiFi channel to use.
 * Default value is 1.
 * Some legal restrictions for values 12 - 14 might apply for your region.
 *
 * @return LE_OUT_OF_RANGE Requested channel number is out of range.
 * @return LE_OK           Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetChannel
(
    int8_t channelNumber
        ///< [IN]
        ///< the channel number must be between 1 and 14.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the maximum number of clients connected to WiFi access point at the same time.
 * Default value is 10.
 *
 * @note This value depends on the hardware/software capabilities of the WiFi module used.
 *
 * @return LE_OUT_OF_RANGE  Requested number of users exceeds the capabilities of the Access Point.
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetMaxNumberClients
(
    int maxNumberClients
        ///< [IN]
        ///< the maximum number of clients regarding the WiFi driver and hardware capabilities.
);

//--------------------------------------------------------------------------------------------------
/**
 * Define the access point IP address and the client IP addresses range.
 *
 * @note The access point IP address must be defined outside the client IP addresses range.
 *
 * @return LE_BAD_PARAMETER At least, one of the given IP addresses is invalid.
 * @return LE_FAULT         A system call has failed.
 * @return LE_OK            Function succeeded.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t pa_wifiAp_SetIpRange
(
    const char *ipApPtr,
        ///< [IN]
        ///< the IP address of the Access Point.
    const char *ipStartPtr,
        ///< [IN]
        ///< the start IP address of the Access Point.
    const char *ipStopPtr
        ///< [IN]
        ///< the stop IP address of the Access Point.
);
#endif // PA_WIFI_AP_H
