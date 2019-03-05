/**
 * This module implements the unit tests for WiFi client API
 *
 * Copyright (C) Sierra Wireless Inc.
 *
 */

#include "legato.h"
#include "interfaces.h"
#include "wifiService.h"

//--------------------------------------------------------------------------------------------------
/**
 * Start and stop the WiFi device
 *
 * API tested:
 * - le_wifiClient_Start
 * - le_wifiClient_Stop
 */
//--------------------------------------------------------------------------------------------------
static void TestWifiClient_StartStop
(
    void
)
{
    int i=0;
    int nbRetries = 10;

    for (i=0; i<nbRetries; i++)
    {
        if (i == 0)
        {
            // First time, we expect an LE_OK
            LE_ASSERT(LE_OK == le_wifiClient_Start());
        }
        else
        {
            LE_ASSERT(LE_BUSY == le_wifiClient_Start());
        }
    }

    for (i=0; i<nbRetries; i++)
    {
        LE_ASSERT(LE_OK == le_wifiClient_Stop());
    }

    LE_ASSERT(LE_DUPLICATE == le_wifiClient_Stop());
}

//--------------------------------------------------------------------------------------------------
/**
 * Create and destroy a WiFi client reference
 *
 * API tested:
 * - le_wifiClient_Create
 * - le_wifiClient_Delete
 * - le_wifiClient_GetSsid
 */
//--------------------------------------------------------------------------------------------------
static void TestWifiClient_CreateDestroy
(
    void
)
{
    const uint8_t ssid[] = "Example_of_ssid";
    size_t ssidNumElements = sizeof(ssid);
    le_wifiClient_AccessPointRef_t ref;

    LE_ASSERT(NULL == le_wifiClient_Create(NULL, 0));
    LE_ASSERT(NULL == le_wifiClient_Create(NULL, ssidNumElements));

    ref = le_wifiClient_Create(ssid, ssidNumElements);
    LE_ASSERT(NULL != ref);

    // Check that if the SSID already exists, the same reference is returned
    LE_ASSERT(ref == le_wifiClient_Create(ssid, ssidNumElements));

    // Get the SSID given its reference
    uint8_t getSsid[LE_WIFIDEFS_MAX_SSID_BYTES];
    size_t  getSsidLen = 1;

    LE_ASSERT(LE_BAD_PARAMETER == le_wifiClient_GetSsid(NULL, getSsid, &getSsidLen));
    LE_ASSERT(LE_BAD_PARAMETER == le_wifiClient_GetSsid(ref, NULL, &getSsidLen));
    LE_ASSERT(LE_BAD_PARAMETER == le_wifiClient_GetSsid(ref, getSsid, NULL));
    LE_ASSERT(LE_OVERFLOW == le_wifiClient_GetSsid(ref, getSsid, &getSsidLen));

    getSsidLen = sizeof(getSsid);
    LE_ASSERT(LE_OK == le_wifiClient_GetSsid(ref, getSsid, &getSsidLen));
    LE_ASSERT(0 == memcmp(ssid, getSsid,  getSsidLen));
    LE_ASSERT(ssidNumElements == getSsidLen);

    // Delete the reference
    LE_ASSERT(LE_BAD_PARAMETER == le_wifiClient_Delete(NULL));
    LE_ASSERT(LE_OK == le_wifiClient_Delete(ref));
}

//--------------------------------------------------------------------------------------------------
/**
 * Connect and disconnect from a WiFi AP
 *
 * API tested:
 * - le_wifiClient_Create
 * - le_wifiClient_Connect
 * - le_wifiClient_Disconnect
 */
//--------------------------------------------------------------------------------------------------
static void TestWifiClient_ConnectDisconnect
(
    void
)
{
    const uint8_t ssid[] = "Example";
    size_t ssidNumElements = sizeof(ssid);
    le_wifiClient_AccessPointRef_t ref;

    ref = le_wifiClient_Create(ssid, ssidNumElements);
    LE_ASSERT(NULL != ref);

    LE_ASSERT(LE_BAD_PARAMETER == le_wifiClient_Connect(NULL));
    LE_ASSERT(LE_OK == le_wifiClient_Connect(ref));

    LE_ASSERT(LE_OK == le_wifiClient_Disconnect());
}


//--------------------------------------------------------------------------------------------------
/**
 * Configure a WIFI client reference
 *
 * API tested:
 * - le_wifiClient_Connect
 * - le_wifiClient_Disconnect
 */
//--------------------------------------------------------------------------------------------------
static void TestWifiClient_Configure
(
    void
)
{
    const uint8_t ssid[] = "Example";
    size_t ssidNumElements = sizeof(ssid);
    le_wifiClient_AccessPointRef_t ref;

    ref = le_wifiClient_Create(ssid, ssidNumElements);
    LE_ASSERT(NULL != ref);

    // Setting a security protocol
    LE_ASSERT(LE_BAD_PARAMETER == le_wifiClient_SetSecurityProtocol(NULL, 0));
    LE_ASSERT(LE_OK == le_wifiClient_SetSecurityProtocol(ref, LE_WIFICLIENT_SECURITY_NONE));
    LE_ASSERT(LE_OK == le_wifiClient_SetSecurityProtocol(ref, LE_WIFICLIENT_SECURITY_WEP));
    LE_ASSERT(LE_OK ==
        le_wifiClient_SetSecurityProtocol(ref, LE_WIFICLIENT_SECURITY_WPA_PSK_PERSONAL));
    LE_ASSERT(LE_OK ==
        le_wifiClient_SetSecurityProtocol(ref, LE_WIFICLIENT_SECURITY_WPA2_PSK_PERSONAL));
    LE_ASSERT(LE_OK ==
        le_wifiClient_SetSecurityProtocol(ref, LE_WIFICLIENT_SECURITY_WPA_EAP_PEAP0_ENTERPRISE));
    LE_ASSERT(LE_OK ==
     le_wifiClient_SetSecurityProtocol(ref, LE_WIFICLIENT_SECURITY_WPA2_EAP_PEAP0_ENTERPRISE));

    // Setting a Passphrase
    char passBuffer[LE_WIFIDEFS_MAX_PASSPHRASE_LENGTH+1] = {0};

    LE_ASSERT(LE_BAD_PARAMETER == le_wifiClient_SetPassphrase(NULL, passBuffer));

    memset(passBuffer, 0, sizeof(passBuffer));
    memset(passBuffer, 'A', LE_WIFIDEFS_MIN_PASSPHRASE_LENGTH);
    LE_ASSERT(LE_OK == le_wifiClient_SetPassphrase(ref, passBuffer));

    memset(passBuffer, 0, sizeof(passBuffer));
    memset(passBuffer, 'B', LE_WIFIDEFS_MIN_PASSPHRASE_LENGTH-1);
    LE_ASSERT(LE_BAD_PARAMETER == le_wifiClient_SetPassphrase(ref, passBuffer));

    memset(passBuffer, 0, sizeof(passBuffer));
    memset(passBuffer, 'C', LE_WIFIDEFS_MAX_PASSPHRASE_LENGTH);
    LE_ASSERT(LE_OK == le_wifiClient_SetPassphrase(ref, passBuffer));

    memset(passBuffer, 0, sizeof(passBuffer));
    memset(passBuffer, 'D', LE_WIFIDEFS_MAX_PASSPHRASE_LENGTH+1);
    LE_ASSERT(LE_BAD_PARAMETER == le_wifiClient_SetPassphrase(ref, passBuffer));

    // Setting PSK key
    char psk[LE_WIFIDEFS_MAX_PSK_LENGTH] = {0};

    LE_ASSERT(LE_BAD_PARAMETER == le_wifiClient_SetPreSharedKey(NULL, psk));
    LE_ASSERT(LE_OK == le_wifiClient_SetPreSharedKey(ref, psk));

    // Setting a WEP key
    char wep[LE_WIFIDEFS_MAX_WEPKEY_BYTES] = {0};
    LE_ASSERT(LE_BAD_PARAMETER == le_wifiClient_SetWepKey(NULL, wep));
    LE_ASSERT(LE_OK == le_wifiClient_SetWepKey(ref, wep));


    // Setting the hidden attribute
    LE_ASSERT(LE_BAD_PARAMETER == le_wifiClient_SetHiddenNetworkAttribute(NULL, false));
    LE_ASSERT(LE_OK == le_wifiClient_SetHiddenNetworkAttribute(ref, false));
    LE_ASSERT(LE_OK == le_wifiClient_SetHiddenNetworkAttribute(ref, true));

}


//--------------------------------------------------------------------------------------------------
/**
 * Positive tests of Wifi security config setting & loading of a given SSID
 *
 * API tested:
 * - le_wifiClient_LoadSsid
 * - le_wifiClient_ConfigureWep
 * - le_wifiClient_ConfigurePsk
 * - le_wifiClient_ConfigureEap
 * - le_wifiClient_RemoveSsidSecurityConfigs
 */
//--------------------------------------------------------------------------------------------------
static void TestWifiClient_LoadSsid
(
    void
)
{
    const uint8_t ssid[] = "Example";
    const uint8_t username[] = "myName";
    const uint8_t secret[] = "mySecret";
    le_result_t ret;
    le_wifiClient_AccessPointRef_t ref;

    // Test le_wifiClient_LoadSsid() with WEP
    LE_INFO("Test Wifi's config setting & loading of WEP");
    ret = le_wifiClient_ConfigureWep(ssid, sizeof(ssid), secret, sizeof(secret));
    if (ret != LE_OK)
    {
        LE_ERROR("Failed to configure WEP into secStore; retcode %d", ret);
    }
    else
    {
        ret = le_wifiClient_LoadSsid(ssid, sizeof(ssid), &ref);
        if (ret != LE_OK)
        {
            LE_ERROR("LoadSsid failed over WEP; retcode %d", ret);
        }
        else
        {
            le_wifiClient_Delete(ref);
            ref = NULL;
        }
    }
    LE_ASSERT(ret == LE_OK);

    // Test le_wifiClient_LoadSsid() with WPA passphrase
    LE_INFO("Test Wifi's config setting & loading of WPA passphrase");
    ret = le_wifiClient_ConfigurePsk(ssid, sizeof(ssid), LE_WIFICLIENT_SECURITY_WPA_PSK_PERSONAL,
                                     secret, sizeof(secret), NULL, 0);
    if (ret != LE_OK)
    {
        LE_ERROR("Failed to configure WPA passphrase into secStore; retcode %d", ret);
    }
    else
    {
        ret = le_wifiClient_LoadSsid(ssid, sizeof(ssid), &ref);
        if (ret != LE_OK)
        {
            LE_ERROR("LoadSsid failed over WPA passphrase; retcode %d", ret);
        }
        else
        {
            le_wifiClient_Delete(ref);
            ref = NULL;
        }
    }
    LE_ASSERT(ret == LE_OK);

    // Test le_wifiClient_LoadSsid() with WPA2 PSK
    LE_INFO("Test Wifi's config setting & loading of WPA2 PSK");
    ret = le_wifiClient_ConfigurePsk(ssid, sizeof(ssid), LE_WIFICLIENT_SECURITY_WPA2_PSK_PERSONAL,
                                     NULL, 0, secret, sizeof(secret));
    if (ret != LE_OK)
    {
        LE_ERROR("Failed to configure WPA2 PSK into secStore; retcode %d", ret);
    }
    else
    {
        ret = le_wifiClient_LoadSsid(ssid, sizeof(ssid), &ref);
        if (ret != LE_OK)
        {
            LE_ERROR("LoadSsid failed over WPA2 PSK; retcode %d", ret);
        }
        else
        {
            le_wifiClient_Delete(ref);
            ref = NULL;
        }
    }
    LE_ASSERT(ret == LE_OK);

    // Test le_wifiClient_LoadSsid() with WPA EAP
    LE_INFO("Test Wifi's config setting & loading of WPA EAP");
    ret = le_wifiClient_ConfigureEap(ssid, sizeof(ssid),
                                     LE_WIFICLIENT_SECURITY_WPA_EAP_PEAP0_ENTERPRISE,
                                     username, sizeof(username), secret, sizeof(secret));
    if (ret != LE_OK)
    {
        LE_ERROR("Failed to configure WPA EAP into secStore; retcode %d", ret);
    }
    else
    {
        ret = le_wifiClient_LoadSsid(ssid, sizeof(ssid), &ref);
        if (ret != LE_OK)
        {
            LE_ERROR("LoadSsid failed over WPA EAP; retcode %d", ret);
        }
        else
        {
            le_wifiClient_Delete(ref);
            ref = NULL;
        }
    }
    LE_ASSERT(ret == LE_OK);

    // Test le_wifiClient_LoadSsid() with WPA2 EAP
    LE_INFO("Test Wifi's config setting & loading of WPA2 EAP");
    ret = le_wifiClient_ConfigureEap(ssid, sizeof(ssid),
                                     LE_WIFICLIENT_SECURITY_WPA2_EAP_PEAP0_ENTERPRISE,
                                     username, sizeof(username), secret, sizeof(secret));
    if (ret != LE_OK)
    {
        LE_ERROR("Failed to configure WPA2 EAP into secStore; retcode %d", ret);
    }
    else
    {
        ret = le_wifiClient_LoadSsid(ssid, sizeof(ssid), &ref);
        if (ret != LE_OK)
        {
            LE_ERROR("LoadSsid failed over WPA2 EAP; retcode %d", ret);
        }
        else
        {
            le_wifiClient_Delete(ref);
            ref = NULL;
        }
    }
    LE_ASSERT(ret == LE_OK);

    // Test le_wifiClient_LoadSsid() with no security
    LE_INFO("Test Wifi's config setting & loading of no security");
    ret = le_wifiClient_RemoveSsidSecurityConfigs(ssid, sizeof(ssid));
    if (ret != LE_OK)
    {
        LE_ERROR("Failed to configure no security; retcode %d", ret);
    }
    else
    {
        ret = le_wifiClient_LoadSsid(ssid, sizeof(ssid), &ref);
        if (ret != LE_OK)
        {
            LE_ERROR("LoadSsid failed over no security; retcode %d", ret);
        }
        else
        {
            le_wifiClient_Delete(ref);
            ref = NULL;
        }
    }
    LE_ASSERT(ret == LE_OK);
}


//--------------------------------------------------------------------------------------------------
/**
 * Negative tests of config of Wifi security protocols
 *
 * APIs tested:
 * - le_wifiClient_ConfigureWep
 * - le_wifiClient_ConfigurePsk
 * - le_wifiClient_ConfigureEap
 * - le_wifiClient_RemoveSsidSecurityConfigs
 */
//--------------------------------------------------------------------------------------------------
static void TestWifiClient_ConfigureSecurity_NegTests
(
    void
)
{
    const uint8_t ssid[] = "Example";
    const uint8_t username[] = "myName";
    const uint8_t secret[] = "mySecret";
    le_result_t ret;

    // -ve test le_wifiClient_ConfigureWep() with WEP
    LE_INFO("Negative test Wifi's config setting of WEP");
    ret = le_wifiClient_ConfigureWep(ssid, sizeof(ssid), NULL, sizeof(secret));
    LE_ASSERT(ret == LE_BAD_PARAMETER);

    // -ve test le_wifiClient_ConfigurePsk() with WPA passphrase
    LE_INFO("Negative test Wifi's config setting of WPA passphrase");
    ret = le_wifiClient_ConfigurePsk(ssid, sizeof(ssid), LE_WIFICLIENT_SECURITY_WEP,
                                     secret, sizeof(secret), NULL, 0);
    LE_ASSERT(ret == LE_BAD_PARAMETER);
    ret = le_wifiClient_ConfigurePsk(ssid, sizeof(ssid), LE_WIFICLIENT_SECURITY_WPA_PSK_PERSONAL,
                                     NULL, 0, NULL, 0);
    LE_ASSERT(ret == LE_BAD_PARAMETER);
    ret = le_wifiClient_ConfigurePsk(ssid, sizeof(ssid), LE_WIFICLIENT_SECURITY_WPA_PSK_PERSONAL,
                                     secret, 1000, NULL, 0);
    LE_ASSERT(ret == LE_BAD_PARAMETER);
    ret = le_wifiClient_ConfigurePsk(ssid, sizeof(ssid), LE_WIFICLIENT_SECURITY_WPA_PSK_PERSONAL,
                                     NULL, 0, secret, 1000);
    LE_ASSERT(ret == LE_BAD_PARAMETER);

    // -ve test le_wifiClient_ConfigureEap() with EAP
    LE_INFO("Negative test Wifi's config setting of EAP");
    ret = le_wifiClient_ConfigureEap(ssid, sizeof(ssid), LE_WIFICLIENT_SECURITY_WEP,
                                     username, sizeof(username), secret, sizeof(secret));
    LE_ASSERT(ret == LE_BAD_PARAMETER);
    ret = le_wifiClient_ConfigureEap(ssid, sizeof(ssid),
                                     LE_WIFICLIENT_SECURITY_WPA_EAP_PEAP0_ENTERPRISE,
                                     NULL, 0, secret, sizeof(secret));
    LE_ASSERT(ret == LE_BAD_PARAMETER);
    ret = le_wifiClient_ConfigureEap(ssid, sizeof(ssid), LE_WIFICLIENT_SECURITY_WEP,
                                     username, sizeof(username), NULL, 0);
    LE_ASSERT(ret == LE_BAD_PARAMETER);

    // -ve test le_wifiClient_RemoveSsidSecurityConfigs() with no security
    LE_INFO("Negative test Wifi's config setting of no security");
    ret = le_wifiClient_RemoveSsidSecurityConfigs(ssid, 0);
    LE_ASSERT(ret == LE_BAD_PARAMETER);
}


//--------------------------------------------------------------------------------------------------
/**
 * main of the test
 *
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    le_wifiClient_Init();

    LE_INFO ("======== Start UnitTest of WiFi client ========");

    TestWifiClient_StartStop();

    TestWifiClient_CreateDestroy();

    TestWifiClient_ConnectDisconnect();

    TestWifiClient_Configure();

    TestWifiClient_LoadSsid();

    TestWifiClient_ConfigureSecurity_NegTests();

    LE_INFO ("======== UnitTest of WiFi client SUCCESS ========");

    exit(EXIT_SUCCESS);
}
