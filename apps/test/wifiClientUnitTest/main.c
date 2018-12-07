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
// Config tree parameters used in testing le_wifiClient_LoadSsid()
//--------------------------------------------------------------------------------------------------
#define CFG_TREE_ROOT_DIR           "wifiService:"
#define CFG_PATH_WIFI               "wifi/channel"
#define CFG_NODE_SECPROTOCOL        "secProtocol"
#define CFG_NODE_PASSPHRASE         "passphrase"
#define TEST_LOAD_SSID_SECPROTOCOL  3
#define TEST_LOAD_SSID_PASSPHRASE   "A1B2C3D4E5"

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
 * Load WIfi configs of a given SSID
 *
 * API tested:
 * - le_wifiClient_LoadSsid
 */
//--------------------------------------------------------------------------------------------------
static void TestWifiClient_LoadSsid
(
    void
)
{
    char configPath[LE_CFG_STR_LEN_BYTES], ssid[] = "Example";
    le_result_t ret;
    le_wifiClient_AccessPointRef_t ref;
    le_cfg_IteratorRef_t cfg;

    snprintf(configPath, sizeof(configPath), "%s/%s", CFG_TREE_ROOT_DIR, CFG_PATH_WIFI);
    cfg = le_cfg_CreateWriteTxn(configPath);
    le_cfg_SetString(cfg, "", ssid);
    le_cfg_CommitTxn(cfg);

    snprintf(configPath, sizeof(configPath), "%s/%s/%s", CFG_TREE_ROOT_DIR, CFG_PATH_WIFI, ssid);
    cfg = le_cfg_CreateWriteTxn(configPath);
    le_cfg_SetInt(cfg, CFG_NODE_SECPROTOCOL, TEST_LOAD_SSID_SECPROTOCOL);
    le_cfg_SetString(cfg, CFG_NODE_PASSPHRASE, TEST_LOAD_SSID_PASSPHRASE);
    le_cfg_CommitTxn(cfg);

    ret = le_wifiClient_LoadSsid(ssid, &ref);
    LE_INFO("LoadSsid returns %d", ret);
    LE_ASSERT(ret == LE_OK);
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

    LE_INFO ("======== UnitTest of WiFi client SUCCESS ========");

    exit(EXIT_SUCCESS);
}
