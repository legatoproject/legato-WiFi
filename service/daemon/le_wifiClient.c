// -------------------------------------------------------------------------------------------------
/**
 *  Legato WiFi Client
 *
 *  Copyright (C) Sierra Wireless Inc.
 *
 */
// -------------------------------------------------------------------------------------------------
#include "legato.h"

#include "interfaces.h"

#include "pa_wifi.h"


//--------------------------------------------------------------------------------------------------
/**
 * The following are Wifi client's config tree path and node definitions
 */
//-------------------------------------------------------------------------------------------------
#define CFG_TREE_ROOT_DIR           "wifiService:"
#define CFG_PATH_WIFI               "wifi/channel"
#define CFG_NODE_HIDDEN_SSID        "hidden"
#define CFG_NODE_SECPROTOCOL        "secProtocol"

//--------------------------------------------------------------------------------------------------
/**
 * The following are Wifi client's secured store's item root and node definitions
 */
//-------------------------------------------------------------------------------------------------
#define SECSTORE_WIFI_ITEM_ROOT     "wifiService/channel"
#define SECSTORE_NODE_PASSPHRASE    "passphrase"
#define SECSTORE_NODE_PSK           "preSharedKey"
#define SECSTORE_NODE_WEP_KEY       "wepKey"
#define SECSTORE_NODE_USERNAME      "userName"
#define SECSTORE_NODE_USERPWD       "userPassword"

//--------------------------------------------------------------------------------------------------
/**
 * The initial allocated number of APs at system start.
 * Note that the pool will grow automatically if it is needed.
 */
//-------------------------------------------------------------------------------------------------
#define INIT_AP_COUNT 32

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
 * Safe Reference Map for Access Points found during scan or le_wifiClient_Create()
 */
//--------------------------------------------------------------------------------------------------
static le_ref_MapRef_t ScanApRefMap;

//--------------------------------------------------------------------------------------------------
/**
 * Pool from which FoundAccessPoint_t objects are allocated.
 * After each Scan pool is first emptied then populated with the result.
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t AccessPointPool;

//--------------------------------------------------------------------------------------------------
/**
 * The number of found AP:s from the scan used for informative traces.
 *
 */
//--------------------------------------------------------------------------------------------------
static uint32_t FoundWifiApCount = 0;

//--------------------------------------------------------------------------------------------------
/**
 * iteration reference used by GetFirst & GetNext
 * @see variable GetFirstSessionRef
 */
//--------------------------------------------------------------------------------------------------
static le_ref_IterRef_t IterRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Saves reference at call of GetFirst to check that GetNext is being called by same caller.
 * This to protect varible IterRef.
 */
//--------------------------------------------------------------------------------------------------
static le_msg_SessionRef_t GetFirstSessionRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * If the Scan is running in a seperate thread, this variable will have a value != NULL.
 * The thread destructor will clear the
 */
//--------------------------------------------------------------------------------------------------
static le_thread_Ref_t ScanThreadRef = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * Result of completed scan
 */
//--------------------------------------------------------------------------------------------------
static le_result_t ScanResult = LE_OK;

//--------------------------------------------------------------------------------------------------
/**
 * Event ID for WiFi Event message notification.
 *
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t NewWifiEventId;

//--------------------------------------------------------------------------------------------------
/**
 * Event ID for WiFi Event indication notification.
 *
 */
//--------------------------------------------------------------------------------------------------
static le_event_Id_t WifiEventId;

//--------------------------------------------------------------------------------------------------
/**
 * Pool for WifiClient state events reporting.
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t WifiEventPool;

//--------------------------------------------------------------------------------------------------
/**
 * The number of calls to le_wifiClient_Start().
 * This is used to call stop the WiFi hardware, when the last client calls stop.
 */
//--------------------------------------------------------------------------------------------------
static uint32_t ClientStartCount = 0;

//--------------------------------------------------------------------------------------------------
/**
 * This CurrentConnection records the selected connection to get established via an API call to
 * le_wifiClient_Connect().  But note that it doesn't necessarily mean that this connection has
 * been successfully established, since the attempt to connect might fail, take time to finish, etc.
 * Its value will be reset after an API call to le_wifiClient_Disconnect() or le_wifiClient_Stop().
 * Thus, this CurrentConnection variable refers more to the selected SSID to get connected instead.
 */
//--------------------------------------------------------------------------------------------------
static le_wifiClient_AccessPointRef_t CurrentConnection = NULL;

//--------------------------------------------------------------------------------------------------
/**
 * WLAN interface being used to do WiFi scan.
 */
//--------------------------------------------------------------------------------------------------
static char scanIfName[LE_WIFIDEFS_MAX_IFNAME_BYTES] = {0};

//--------------------------------------------------------------------------------------------------
/**
 * CallBack for PA Events.
 */
//--------------------------------------------------------------------------------------------------
static void PaEventHandler
(
    le_wifiClient_EventInd_t* wifiEventPtr,
    void *contextPtr
)
{
    LE_DEBUG("WiFi event: %d, interface: %s, bssid: %s",
            wifiEventPtr->event,
            wifiEventPtr->ifName,
            wifiEventPtr->apBssid);

    if (LE_WIFICLIENT_EVENT_DISCONNECTED == wifiEventPtr->event)
    {
        LE_DEBUG("disconnectCause: %d", wifiEventPtr->disconnectionCause);
    }

    le_event_ReportWithRefCounting(WifiEventId, wifiEventPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * Local function to find an access point reference based on BSSID among the AP found in scan.
 * If not found will return NULL.
 */
//--------------------------------------------------------------------------------------------------
static le_wifiClient_AccessPointRef_t FindAccessPointRefFromBssid
(
    const char* bssidPtr
        ///< [OUT]
        ///< The BSSID returned as a byte array.
)
{
    le_wifiClient_AccessPointRef_t apRef = NULL;
    le_ref_IterRef_t               iter  = le_ref_GetIterator(ScanApRefMap);

    LE_DEBUG("Find AP from BSSID");

    while (le_ref_NextNode(iter) == LE_OK)
    {
        apRef = (le_wifiClient_AccessPointRef_t)le_ref_GetSafeRef(iter);
        if (NULL != apRef)
        {
            FoundAccessPoint_t *apPtr = (FoundAccessPoint_t *)le_ref_Lookup(ScanApRefMap, apRef);
            if (NULL != apPtr)
            {
                if (0 == strncmp(apPtr->accessPoint.bssid, bssidPtr, LE_WIFIDEFS_MAX_BSSID_BYTES))
                {
                    LE_DEBUG("Found apRef %p", apRef);
                    return apRef;
                }
            }
        }
        else
        {
            LE_ERROR("ERROR le_ref_GetSafeRef returned NULL iter:%p", iter);
        }
    }
    return NULL;
}

//--------------------------------------------------------------------------------------------------
/**
 * Local function to find an access point reference based on SSID among the AP found in scan.
 * If not found will return NULL.
 */
//--------------------------------------------------------------------------------------------------
static le_wifiClient_AccessPointRef_t FindAccessPointRefFromSsid
(
    const uint8_t* ssidPtr,
        ///< [OUT]
        ///< The SSID returned as a byte array.

    size_t ssidNumElements
        ///< [INOUT]
        ///< SSID length in bytes.
)
{
    le_wifiClient_AccessPointRef_t apRef = NULL;
    le_ref_IterRef_t               iter  = le_ref_GetIterator(ScanApRefMap);

    LE_DEBUG("Find AP from SSID");

    while (le_ref_NextNode(iter) == LE_OK)
    {
        apRef = (le_wifiClient_AccessPointRef_t)le_ref_GetSafeRef(iter);
        if (NULL != apRef)
        {
            FoundAccessPoint_t *apPtr = (FoundAccessPoint_t *)le_ref_Lookup(ScanApRefMap, apRef);
            if (NULL != apPtr)
            {
                if (apPtr->accessPoint.ssidLength == ssidNumElements)
                {
                    if (0 == memcmp(apPtr->accessPoint.ssidBytes, ssidPtr, ssidNumElements))
                    {
                        LE_DEBUG("Found apRef %p", apRef);
                        return apRef;
                    }
                }
            }
        }
        else
        {
            LE_ERROR("ERROR le_ref_GetSafeRef returned NULL iter:%p", iter);
        }
    }
    return NULL;
}


//--------------------------------------------------------------------------------------------------
/**
 * Local function to add AP:s found during scan to AddRef point interface
 */
//--------------------------------------------------------------------------------------------------

static le_wifiClient_AccessPointRef_t AddAccessPointToApRefMap
(
    pa_wifiClient_AccessPoint_t *apPtr
)
{
    // first see if it alreay exists in our list of reference.
    le_wifiClient_AccessPointRef_t returnedRef = FindAccessPointRefFromBssid(apPtr->bssid);

    if (NULL != returnedRef)
    {
        FoundAccessPoint_t *oldAccessPointPtr =
            (FoundAccessPoint_t *)le_ref_Lookup(ScanApRefMap, returnedRef);


        if (NULL != oldAccessPointPtr)
        {
            LE_DEBUG("Already exists %p. Update SignalStrength %d, SSID '%s'",
                     returnedRef, apPtr->signalStrength, &apPtr->ssidBytes[0]);

            oldAccessPointPtr->accessPoint.signalStrength = apPtr->signalStrength;
            oldAccessPointPtr->accessPoint.ssidLength = apPtr->ssidLength;
            memcpy(&oldAccessPointPtr->accessPoint.ssidBytes, &apPtr->ssidBytes,
                   apPtr->ssidLength);
            oldAccessPointPtr->foundInLatestScan = true;
        }

        return returnedRef;
    }
    else
    {
        FoundAccessPoint_t *foundAccessPointPtr = le_mem_ForceAlloc(AccessPointPool);

        if (foundAccessPointPtr)
        {
            FoundWifiApCount++;
            LE_DEBUG("New AP[%d] SignalStrength %d | SSID length %d | SSID: \"%.*s\"",
                FoundWifiApCount,
                apPtr->signalStrength,
                apPtr->ssidLength,
                apPtr->ssidLength,
                (char *)apPtr->ssidBytes
               );

            // struct member value copy
            foundAccessPointPtr->accessPoint = *apPtr;
            foundAccessPointPtr->foundInLatestScan = true;

            // Create a Safe Reference for this object.
            returnedRef = le_ref_CreateRef(ScanApRefMap, foundAccessPointPtr);

            LE_DEBUG("le_ref_CreateRef foundAccessPointPtr %p; Ref%p ",
                foundAccessPointPtr, returnedRef);
        }
        else
        {
            LE_ERROR("le_mem_ForceAlloc failed. Count %d", FoundWifiApCount);
        }
    }

    return returnedRef;
}


//--------------------------------------------------------------------------------------------------
/**
 *  Frees one members of the access point and the corresponding access points memory
 */
//--------------------------------------------------------------------------------------------------
static void RemoveAccessPoint
(
    le_wifiClient_AccessPointRef_t apRef
)
{
    FoundAccessPoint_t *apPtr = (FoundAccessPoint_t *)le_ref_Lookup(ScanApRefMap, apRef);

    if (apPtr == NULL)
    {
        LE_ERROR("Bad reference");
        return;
    }

    le_ref_DeleteRef(ScanApRefMap, apRef);
    le_mem_Release(apPtr);
}


//--------------------------------------------------------------------------------------------------
/**
 *  Frees all members of the AddRef and the corresponding AccessPoints added during Scan and
 *  le_wifiClient_Create()
 */
//--------------------------------------------------------------------------------------------------
static void ReleaseAllAccessPoints
(
    void
)
{
    le_wifiClient_AccessPointRef_t apRef = NULL;
    le_ref_IterRef_t               iter  = le_ref_GetIterator(ScanApRefMap);

    LE_DEBUG("Release all AP");

    while (le_ref_NextNode(iter) == LE_OK)
    {
        apRef = (le_wifiClient_AccessPointRef_t)le_ref_GetSafeRef(iter);
        if (NULL != apRef)
        {
            RemoveAccessPoint(apRef);
        }
        else
        {
            LE_ERROR("ERROR le_ref_GetSafeRef returned NULL iter:%p", iter);
            return;
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Marks the current access points as old by changing the values for foundInLatestScan
 * and signalStrength.
 * These values will be updated later, if the same AP is still found.
 * This way the new and old AccessPoints can be separated.
 *
 */
//--------------------------------------------------------------------------------------------------
static void MarkAllAccessPointsOld
(
    void
)
{
    le_wifiClient_AccessPointRef_t apRef   = NULL;
    le_ref_IterRef_t               iter    = le_ref_GetIterator(ScanApRefMap);
    uint32_t                       counter = 0;

    LE_DEBUG("Mark all AP as old");

    while (le_ref_NextNode(iter) == LE_OK)
    {
        apRef = (le_wifiClient_AccessPointRef_t)le_ref_GetSafeRef(iter);
        if (NULL != apRef)
        {
            FoundAccessPoint_t* apPtr = (FoundAccessPoint_t *)le_ref_Lookup(ScanApRefMap, apRef);

            if (apPtr != NULL)
            {
                apPtr->accessPoint.signalStrength = LE_WIFICLIENT_NO_SIGNAL_STRENGTH;
                apPtr->foundInLatestScan = false;
                LE_DEBUG("Marking %p as old", apRef);
                counter++;
            }
            else
            {
                LE_ERROR("Bad reference");
                return;
            }

        }
        else
        {
            LE_ERROR("ERROR le_ref_GetSafeRef returned NULL iter:%p", iter);
            break;
        }
    }
    LE_DEBUG("Marked: %d", counter);
}


//--------------------------------------------------------------------------------------------------
/**
 * Start Scanning for WiFi Access points
 * Will result in an event LE_WIFICLIENT_EVENT_SCAN_DONE when the scan results are available or
 * an event LE_WIFICLIENT_EVENT_SCAN_FAILED if there was an error while scanning.
 *
 * @return LE_FAULT         Function failed.
 * @return LE_OK            Function succeeded.
 */
//--------------------------------------------------------------------------------------------------
static void *ScanThread
(
    void *contextPtr
)
{
    pa_wifiClient_AccessPoint_t accessPoint;
    le_result_t                 *scanResultPtr = contextPtr;
    le_result_t                 paResult    = pa_wifiClient_Scan();

    if (LE_OK != paResult)
    {
        LE_ERROR("Scan failed (%d)", paResult);
        *scanResultPtr = LE_FAULT;
        return NULL;
    }

    FoundWifiApCount = 0;

    MarkAllAccessPointsOld();
    memset(scanIfName, 0, LE_WIFIDEFS_MAX_IFNAME_BYTES);
    while (LE_OK == (paResult = pa_wifiClient_GetScanResult(&accessPoint, scanIfName)))
    {
        if (AddAccessPointToApRefMap(&accessPoint) == NULL)
        {
            LE_ERROR("pa_wifiClient_ScanDone() failed");
            paResult = LE_FAULT;
            break;
        }
    }

    *scanResultPtr = ((paResult == LE_OK) || (paResult == LE_NOT_FOUND)) ? LE_OK : paResult;

    paResult = pa_wifiClient_ScanDone();
    if (LE_OK != paResult)
    {
        LE_ERROR("pa_wifiClient_ScanDone() failed (%d)", paResult);
        *scanResultPtr = paResult;
    }

    return NULL;
}


//--------------------------------------------------------------------------------------------------
/**
 * Thread Destructor for scan
 */
//--------------------------------------------------------------------------------------------------
static void ScanThreadDestructor
(
    void *context
)
{
    le_result_t scanResult = *((le_result_t*)context);

    LE_DEBUG("Destruct scan thread");
    ScanThreadRef = NULL;
    le_wifiClient_EventInd_t* wifiEventPtr = le_mem_ForceAlloc(WifiEventPool);

    if (scanResult == LE_OK)
    {
        wifiEventPtr->event = LE_WIFICLIENT_EVENT_SCAN_DONE;
    }
    else
    {
        LE_WARN("Scan failed");
        wifiEventPtr->event = LE_WIFICLIENT_EVENT_SCAN_FAILED;
    }

    wifiEventPtr->disconnectionCause = LE_WIFICLIENT_UNKNOWN_CAUSE;
    strncpy(wifiEventPtr->ifName, scanIfName, LE_WIFIDEFS_MAX_IFNAME_LENGTH);
    wifiEventPtr->ifName[LE_WIFIDEFS_MAX_IFNAME_LENGTH] = '\0';
    wifiEventPtr->apBssid[0] = '\0';
    PaEventHandler(wifiEventPtr, NULL);
}


//--------------------------------------------------------------------------------------------------
/**
 * Handler function to the close session service to detect if the application crashed.
 *
 */
//--------------------------------------------------------------------------------------------------
static void CloseSessionEventHandler
(
    le_msg_SessionRef_t  sessionRef,
    void                *contextPtr
)
{
    if (sessionRef == GetFirstSessionRef)
    {
        GetFirstSessionRef = NULL;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Is Scan running. Checks if the ScanThread is still running
 */
//--------------------------------------------------------------------------------------------------
static bool IsScanRunning(void)
{
    LE_DEBUG("IsScanRunning .%d", (NULL != ScanThreadRef));
    return (NULL != ScanThreadRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * The first-layer WiFi Client Event Handler.
 *
 * @deprecated le_wifiClient_AddNewEventHandler() should not be used anymore.
 *
 * It has been replaced by le_wifiClient_AddConnectionEventHandler().
 */
//--------------------------------------------------------------------------------------------------
static void FirstLayerWifiClientEventHandler
(
    void *reportPtr,
    void *secondLayerHandlerFunc
)
{
    le_wifiClient_NewEventHandlerFunc_t  clientHandlerFunc = secondLayerHandlerFunc;
    le_wifiClient_Event_t               *wifiEventPtr      = (le_wifiClient_Event_t *)reportPtr;

    if (NULL != wifiEventPtr)
    {
        LE_DEBUG("Event: %d", *wifiEventPtr);
        clientHandlerFunc(*wifiEventPtr, le_event_GetContextPtr());
    }
    else
    {
        LE_ERROR("Event is NULL");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * The first-layer WiFi Client Connection Event Handler.
 *
 */
//--------------------------------------------------------------------------------------------------
static void FirstLayerWifiClientConnectionEventHandler
(
    void *reportPtr,
    void *secondLayerHandlerFunc
)
{
    le_wifiClient_EventInd_t*  wifiEventPtr = reportPtr;
    le_wifiClient_ConnectionEventHandlerFunc_t  clientHandlerFunc = secondLayerHandlerFunc;


    if (NULL != wifiEventPtr)
    {
        clientHandlerFunc(wifiEventPtr, le_event_GetContextPtr());
    }
    else
    {
        LE_WARN("wifiEventPtr is NULL");
    }
    // The reportPtr is a reference counted object, so need to release it
    le_mem_Release(reportPtr);
}

//--------------------------------------------------------------------------------------------------
/**
 * returns the value of the field "foundInLatestScan"
 * This is used to determine if a Ref was found in the last scan or not.
 */
//--------------------------------------------------------------------------------------------------
static bool WasApFoundInLastScan
(
    le_wifiClient_AccessPointRef_t apRef
        ///< [IN]
        ///< WiFi Access Point reference.
)
{
    FoundAccessPoint_t *apPtr = (FoundAccessPoint_t *)le_ref_Lookup(ScanApRefMap, apRef);

    if (apPtr == NULL)
    {
        LE_ERROR("Bad reference");
        return false;
    }

    return apPtr->foundInLatestScan;
}


//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'le_wifiClient_NewEvent'
 *
 * This event provide information on Wifi Client event changes.
 *
 * @deprecated le_wifiClient_AddNewEventHandler() should not be used anymore.
 *
 * It has been replaced by le_wifiClient_AddConnectionEventHandler().
 */
//--------------------------------------------------------------------------------------------------
le_wifiClient_NewEventHandlerRef_t le_wifiClient_AddNewEventHandler
(
    le_wifiClient_NewEventHandlerFunc_t handlerFuncPtr,
        ///< [IN]
        ///< Event handling function

    void *contextPtr
        ///< [IN]
        ///< Associated event context
)
{
    le_event_HandlerRef_t handlerRef;

    // Note: THIS ONE REGISTERS THE CB function..
    LE_DEBUG("Add new event handler");

    if (handlerFuncPtr == NULL)
    {
        LE_KILL_CLIENT("handlerFuncPtr is NULL !");
        return NULL;
    }

    handlerRef = le_event_AddLayeredHandler("NewWiFiClientMsgHandler",
        NewWifiEventId,
        FirstLayerWifiClientEventHandler,
        (le_event_HandlerFunc_t)handlerFuncPtr);

    le_event_SetContextPtr(handlerRef, contextPtr);

    return (le_wifiClient_NewEventHandlerRef_t)(handlerRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * This function must be called to register an handler for WiFi connection state change.
 *
 * @return A handler reference, which is only needed for later removal of the handler.
 *
 * @note Doesn't return on failure, so there's no need to check the return value for errors.
 */
//--------------------------------------------------------------------------------------------------
le_wifiClient_ConnectionEventHandlerRef_t le_wifiClient_AddConnectionEventHandler
(
    le_wifiClient_ConnectionEventHandlerFunc_t handlerFuncPtr,
        ///< [IN]
        ///< Event handling function

    void *contextPtr
        ///< [IN]
        ///< Associated event context
)
{
    le_event_HandlerRef_t handlerRef;

    // Note: THIS ONE REGISTERS THE CB function..
    LE_DEBUG("Add wifi connection event handler");

    if (handlerFuncPtr == NULL)
    {
        LE_KILL_CLIENT("handlerFuncPtr is NULL !");
        return NULL;
    }

    handlerRef = le_event_AddLayeredHandler("WiFiClientMsgHandler",
                                            WifiEventId,
                                            FirstLayerWifiClientConnectionEventHandler,
                                            (le_event_HandlerFunc_t)handlerFuncPtr);

    le_event_SetContextPtr(handlerRef, contextPtr);

    return (le_wifiClient_ConnectionEventHandlerRef_t)(handlerRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'le_wifiClient_NewEvent'
 *
 * @deprecated le_wifiClient_AddNewEventHandler() should not be used anymore.
 *
 * It has been replaced by le_wifiClient_AddConnectionEventHandler().
 */
//--------------------------------------------------------------------------------------------------
void le_wifiClient_RemoveNewEventHandler
(
    le_wifiClient_NewEventHandlerRef_t handlerRef
        ///< [IN]
        ///< Reference of the event handler to remove
)
{
    LE_DEBUG("Remove event handler");
    le_event_RemoveHandler((le_event_HandlerRef_t)handlerRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'le_wifiClient_ConnectionEvent'
 */
//--------------------------------------------------------------------------------------------------
void le_wifiClient_RemoveConnectionEventHandler
(
    le_wifiClient_ConnectionEventHandlerRef_t handlerRef
        ///< [IN]
        ///< Reference of the event handler to remove
)
{
    LE_DEBUG("Remove event handler");
    le_event_RemoveHandler((le_event_HandlerRef_t)handlerRef);
}

//--------------------------------------------------------------------------------------------------
/**
 * Starts the WIFI device.
 *
 * @return
 *      - LE_OK     Function succeeded.
 *      - LE_FAULT  Function failed.
 *      - LE_BUSY   The WIFI device is already started.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_Start
(
    void
)
{
    le_result_t result;

    // Only the first client starts the WIFI module
    if (!ClientStartCount)
    {
        result = pa_wifiClient_Start();
        if (LE_OK == result)
        {
            LE_DEBUG("WIFI client started successfully");
            // Increment the number of clients calling this start function
            ClientStartCount++;
        }
        else
        {
            LE_WARN("Unable to start WIFI client; return code: %d", result);
        }
    }
    else
    {
        LE_WARN("WIFI client already started");
        // Increment the number of clients calling this start function
        ClientStartCount++;
        result = LE_BUSY;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Stops the WIFI device.
 *
 * @return
 *      - LE_OK        Function succeeded.
 *      - LE_FAULT     Function failed.
 *      - LE_DUPLICATE The WIFI device is already stopped.
 *
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_Stop
(
    void
)
{
    le_result_t result = LE_OK;

    if (0 == ClientStartCount)
    {
        LE_WARN("The WIFI device is already stopped");
        return LE_DUPLICATE;
    }

    // Only the last client closes the WIFI module
    if (1 == ClientStartCount)
    {
        pa_wifiClient_ClearAllCredentials();
        CurrentConnection = NULL;

        result = pa_wifiClient_Stop();
        if (LE_OK != result)
        {
            LE_ERROR("Unable to stop WIFI client. Err: %d", result);
        }

        ReleaseAllAccessPoints();
        LE_DEBUG("WIFI client stopped successfully");
    }

    // Decrement the number of clients calling this stop function
    if ((LE_OK == result) && (ClientStartCount))
    {
        ClientStartCount--;
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Start Scanning for WiFi Access points
 * Will result in event LE_WIFICLIENT_EVENT_SCAN_DONE when the scan results are available.
 *
 * @return
 *      - LE_OK     Function succeeded.
 *      - LE_FAULT  Function failed.
 *      - LE_BUSY   Scan already running.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_Scan
(
    void
)
{
    if (!IsScanRunning())
    {
        LE_DEBUG("Scan started");

        // Start the thread
        ScanResult = LE_OK;
        ScanThreadRef = le_thread_Create("WiFi Client Scan Thread", ScanThread, &ScanResult);
        le_thread_AddChildDestructor(ScanThreadRef, ScanThreadDestructor, &ScanResult);

        le_thread_Start(ScanThreadRef);
        return LE_OK;
    }
    else
    {
        LE_DEBUG("ERROR: Scan already running");
        return LE_BUSY;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the first WiFi Access Point found.
 *
 * @return
 *      - WiFi  Access Point reference if ok.
 *      - NULL  If no Access Point reference available.
 */
//--------------------------------------------------------------------------------------------------
le_wifiClient_AccessPointRef_t le_wifiClient_GetFirstAccessPoint
(
    void
)
{
    le_wifiClient_AccessPointRef_t apRef = NULL;
    bool                           found = false;

    if (IsScanRunning())
    {
        LE_ERROR("ERROR: Scan is running.");
        return NULL;
    }
    IterRef = le_ref_GetIterator(ScanApRefMap);
    GetFirstSessionRef = le_wifiClient_GetClientSessionRef();

    LE_DEBUG("Get first AP");

    while (le_ref_NextNode(IterRef) == LE_OK)
    {
        apRef = (le_wifiClient_AccessPointRef_t)le_ref_GetSafeRef(IterRef);
        if (WasApFoundInLastScan(apRef))
        {
            found = true;
            break;
        }
    }

    if (found)
    {
        LE_DEBUG("AP ref = %p", apRef);
        return apRef;
    }
    else
    {
        LE_DEBUG("AP not found");
        return NULL;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the next WiFi Access Point.
 * Will return the Access Points in the order of found.
 * This function must be called in the same context as the GetFirstAccessPoint
 *
 * @return
 *      - WiFi  Access Point reference if ok.
 *      - NULL  If no Access Point reference available.
*/
//--------------------------------------------------------------------------------------------------
le_wifiClient_AccessPointRef_t le_wifiClient_GetNextAccessPoint
(
    void
)
{
    le_wifiClient_AccessPointRef_t apRef = NULL;
    bool                           found = false;

    LE_DEBUG("Get next AP");
    if (IsScanRunning())
    {
        LE_ERROR("ERROR: Scan is running.");
        return NULL;
    }

    /* This check to protect the variable IterRef that shouldn't be called from different contexts*/
    if (le_wifiClient_GetClientSessionRef() != GetFirstSessionRef)
    {
        LE_ERROR("ERROR: Called from different context than GetFirstAccessPoint");
        return NULL;
    }

    while (le_ref_NextNode(IterRef) == LE_OK)
    {
        apRef = (le_wifiClient_AccessPointRef_t)le_ref_GetSafeRef(IterRef);
        LE_DEBUG("AP ref = %p", apRef);
        if (WasApFoundInLastScan(apRef))
        {
            found = true;
            break;
        }
    }

    if (found)
    {
        LE_DEBUG("AP ref = %p", apRef);
        return apRef;
    }
    else
    {
        LE_DEBUG("AP not found");
        GetFirstSessionRef = NULL;
        return NULL;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the signal strength of the AccessPoint
 *
 * @return
 *      - Signal strength in dBm. Example -30 = -30dBm
 *      - If no signal available it will return LE_WIFICLIENT_NO_SIGNAL_STRENGTH
 *
 * @note The function returns the signal strength as reported at the time of the scan.
 */
//--------------------------------------------------------------------------------------------------
int16_t le_wifiClient_GetSignalStrength
(
    le_wifiClient_AccessPointRef_t apRef
        ///< [IN]
        ///< WiFi Access Point reference.
)
{
    FoundAccessPoint_t *apPtr = le_ref_Lookup(ScanApRefMap, apRef);

    LE_DEBUG("Get signal strength");
    if (NULL == apPtr)
    {
        LE_ERROR("Invalid access point reference.");
        return LE_WIFICLIENT_NO_SIGNAL_STRENGTH;
    }

    return apPtr->accessPoint.signalStrength;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the Basic Service set identifier (BSSID) of the AccessPoint
 *
 * @return
 *         LE_OK            Function succeeded.
 *         LE_FAULT         Function failed.
 *         LE_BAD_PARAMETER Invalid parameter.
 *         LE_OVERFLOW      bssid buffer is too small to contain the BSSID.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_GetBssid
(
    le_wifiClient_AccessPointRef_t apRef,
        ///< [IN]
        ///< WiFi Access Point reference.

    char *bssidPtr,
        ///< [OUT]
        ///< The BSSID

    size_t bssidSize
        ///< [IN]
)
{
    FoundAccessPoint_t *apPtr = le_ref_Lookup(ScanApRefMap, apRef);

    LE_DEBUG("AP ref %p", apRef);
    if (NULL == apPtr)
    {
        LE_ERROR("Invalid access point reference.");
        return LE_BAD_PARAMETER;
    }

    if (NULL == bssidPtr)
    {
        LE_ERROR("Invalid parameter BSSID = %p", bssidPtr);
        return LE_BAD_PARAMETER;
    }

    if (strnlen(apPtr->accessPoint.bssid, LE_WIFIDEFS_MAX_BSSID_BYTES) > bssidSize)
    {
        return LE_OVERFLOW;
    }
    strncpy(bssidPtr, apPtr->accessPoint.bssid, LE_WIFIDEFS_MAX_BSSID_BYTES);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the Service set identification (SSID) of the AccessPoint
 *
 * @return
 *        LE_OK            Function succeeded.
 *        LE_FAULT         Function failed.
 *        LE_BAD_PARAMETER Invalid parameter.
 *        LE_OVERFLOW      ssid buffer is too small to contain the SSID.
 *
 * @note The SSID does not have to be human readable ASCII values, but often is.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_GetSsid
(
    le_wifiClient_AccessPointRef_t apRef,
        ///< [IN]
        ///< WiFi Access Point reference.

    uint8_t *ssidPtr,
        ///< [OUT]
        ///< The SSID returned as a octet array.

    size_t *ssidNumElementsPtr
        ///< [INOUT]
        ///< SSID length in octets.
)
{
    FoundAccessPoint_t *apPtr = le_ref_Lookup(ScanApRefMap, apRef);

    LE_DEBUG("AP ref %p", apRef);
    if (NULL == apPtr)
    {
        LE_ERROR("Invalid access point reference.");
        return LE_BAD_PARAMETER;
    }

    if ((NULL == ssidPtr) || (NULL == ssidNumElementsPtr))
    {
        LE_ERROR("Invalid parameter SSID = %p, SSID length = %p", ssidPtr, ssidNumElementsPtr);
        return LE_BAD_PARAMETER;
    }

    if (*ssidNumElementsPtr < apPtr->accessPoint.ssidLength) {
        LE_ERROR("SSID buffer length (%zu) is too small to contain SSID of length (%d)",
                 *ssidNumElementsPtr, apPtr->accessPoint.ssidLength);
        return LE_OVERFLOW;
    }

    *ssidNumElementsPtr = apPtr->accessPoint.ssidLength;
    LE_DEBUG("apPtr->AccessPoint.ssidLength %d", apPtr->accessPoint.ssidLength);

    memcpy(&ssidPtr[0], &apPtr->accessPoint.ssidBytes[0], apPtr->accessPoint.ssidLength);

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the currently selected connection to be established. The output will be Null if none is
 * selected
 *
 * @return
 *      - LE_OK            Function succeeded.
 *      - LE_FAULT         Function failed.
 */
//--------------------------------------------------------------------------------------------------
void le_wifiClient_GetCurrentConnection
(
    le_wifiClient_AccessPointRef_t *apRef  ///< [OUT] currently selected connection's AP reference
)
{
    LE_DEBUG("AP reference of currently selected connection: %p", CurrentConnection);
    if (!apRef)
    {
        return;
    }
    *apRef = CurrentConnection;
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the passphrase used to generate the PSK.
 *
 * @return
 *      - LE_OK            Function succeeded.
 *      - LE_FAULT         Function failed.
 *      - LE_BAD_PARAMETER Invalid parameter.
 *
 * @note The difference between le_wifiClient_SetPreSharedKey() and this function
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_SetPassphrase
(
    le_wifiClient_AccessPointRef_t apRef,
        ///< [IN]
        ///< WiFi Access Point reference.

    const char *passPhrasePtr
        ///< [IN]
        ///< pass-phrase for PSK
)
{
    le_result_t result = LE_BAD_PARAMETER;

    LE_DEBUG("Set passphrase");

    if (NULL == le_ref_Lookup(ScanApRefMap, apRef))
    {
        LE_ERROR("Invalid access point reference.");
        return LE_BAD_PARAMETER;
    }

    if (NULL != passPhrasePtr)
    {
        result = pa_wifiClient_SetPassphrase(passPhrasePtr);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the Pre Shared Key, PSK.
 *
 * @return
 *      - LE_OK             Function succeeded.
 *      - LE_FAULT          Function failed.
 *      - LE_BAD_PARAMETER  Invalid parameter.
 *
 * @note This is one way to authenticate against the access point. The other one is provided by the
 * le_wifiClient_SetPassPhrase() function. Both ways are exclusive and are effective only when used
 * with WPA-personal authentication.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_SetPreSharedKey
(
    le_wifiClient_AccessPointRef_t apRef,
        ///< [IN]
        ///< WiFi Access Point reference.

    const char *preSharedKeyPtr
        ///< [IN]
        ///< Pre-shared key used to authenticate against the access point.
)
{
    le_result_t result = LE_BAD_PARAMETER;

    LE_DEBUG("Set PSK");
    if (NULL == le_ref_Lookup(ScanApRefMap, apRef))
    {
        LE_ERROR("Invalid access point reference.");
        return LE_BAD_PARAMETER;
    }

    if (NULL != preSharedKeyPtr)
    {
        result = pa_wifiClient_SetPreSharedKey(preSharedKeyPtr);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * This function specifies whether the target Access Point is hiding its presence from clients or
 * not. When an Access Point is hidden, it cannot be discovered by a scan process.
 *
 * @return
 *      - LE_OK             Function succeeded.
 *      - LE_BAD_PARAMETER  Invalid parameter.
 *
 * @note By default, this attribute is not set which means that the client is unable to connect to
 * a hidden access point. When enabled, the client will be able to connect to the access point
 * whether it is hidden or not.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_SetHiddenNetworkAttribute
(
    le_wifiClient_AccessPointRef_t apRef,
        ///< [IN]
        ///< WiFi Access Point reference.

    bool hidden
        ///< [IN]
        ///< If TRUE, the WIFI client will be able to connect to a hidden access point.
)
{
    LE_DEBUG("Set whether access point is hidden or not: %d", hidden);
    if (NULL == le_ref_Lookup(ScanApRefMap, apRef))
    {
        LE_ERROR("Invalid access point reference.");
        return LE_BAD_PARAMETER;
    }

    pa_wifiClient_SetHiddenNetworkAttribute(hidden);
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * WPA-Enterprise requires a username and password to authenticate.
 * This function sets these parameters.
 *
 * @return
 *      - LE_OK             Function succeeded.
 *      - LE_FAULT          Function failed.
 *      - LE_BAD_PARAMETER  Invalid parameter.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_SetUserCredentials
(
    le_wifiClient_AccessPointRef_t apRef,
        ///< [IN]
        ///< WiFi Access Point reference.

    const char *userNamePtr,
        ///< [IN]
        ///< UserName used for WPA-Enterprise authentication protocol.

    const char *passwordPtr
        ///< [IN]
        ///< Password used for WPA-Enterprise authentication protocol.
)
{
    le_result_t result = LE_BAD_PARAMETER;

    LE_DEBUG("Set user credentials");
    if (NULL == le_ref_Lookup(ScanApRefMap, apRef))
    {
        LE_ERROR("Invalid access point reference.");
        return LE_BAD_PARAMETER;
    }

    if ((NULL != userNamePtr) && (NULL != passwordPtr))
    {
        result = pa_wifiClient_SetUserCredentials(userNamePtr, passwordPtr);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the WEP key (WEP)
 *
 * @return
 *      - LE_OK     Function succeeded.
 *      - LE_FAULT  Function failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_SetWepKey
(
    le_wifiClient_AccessPointRef_t apRef,
        ///< [IN]
        ///< WiFi Access Point reference.

    const char *wepKeyPtr
        ///< [IN]
        ///< Wired Equivalent Privacy (WEP) key used for authentication
)
{
    le_result_t result = LE_BAD_PARAMETER;

    LE_DEBUG("Set WEP key");
    if (NULL == le_ref_Lookup(ScanApRefMap, apRef))
    {
        LE_ERROR("Invalid access point reference.");
        return LE_BAD_PARAMETER;
    }

    if (NULL != wepKeyPtr)
    {
        result = pa_wifiClient_SetWepKey(wepKeyPtr);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
/**
 * Set the security protocol for connection
 *
 * @return
 *      - LE_OK             Function succeeded.
 *      - LE_FAULT          Function failed.
 *      - LE_BAD_PARAMETER  Invalid parameter.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_SetSecurityProtocol
(
    le_wifiClient_AccessPointRef_t apRef,
        ///< [IN]
        ///< WiFi Access Point reference.

    le_wifiClient_SecurityProtocol_t securityProtocol
        ///< [IN]
        ///< Security Mode
)
{
    LE_DEBUG("Set security protocol");
    if (NULL == le_ref_Lookup(ScanApRefMap, apRef))
    {
        LE_ERROR("Invalid access point reference.");
        return LE_BAD_PARAMETER;
    }

    return pa_wifiClient_SetSecurityProtocol(securityProtocol);
}

//--------------------------------------------------------------------------------------------------
/**
 * This function creates a reference to an Access Point given its SSID.
 * If an Access Point is hidden, it will not show up in the scan. So, its SSID must be known
 * in advance in order to create a reference.
 *
 * @return
 *      - AccessPoint reference to the current Access Point.
 *
 * @note This function fails if called while scan is running.
 */
//--------------------------------------------------------------------------------------------------
le_wifiClient_AccessPointRef_t le_wifiClient_Create
(
    const uint8_t *ssidPtr,
        ///< [IN]
        ///< The SSID as a octet array.

    size_t ssidNumElements
        ///< [IN]
        ///< Length of the SSID in octets.
)
{
    le_wifiClient_AccessPointRef_t returnedRef = NULL;

    if (IsScanRunning())
    {
        LE_ERROR("ERROR: Scan is running");
        return NULL;
    }

    if (NULL == ssidPtr)
    {
        LE_ERROR("ERROR: ssidPtr is NULL");
        return NULL;
    }

    returnedRef = FindAccessPointRefFromSsid(ssidPtr, ssidNumElements);

    // if the access point does not already exist, then create it.
    if (returnedRef == NULL)
    {
        FoundAccessPoint_t* createdAccessPointPtr = le_mem_ForceAlloc(AccessPointPool);

        if (createdAccessPointPtr)
        {
            createdAccessPointPtr->foundInLatestScan = false;

            createdAccessPointPtr->accessPoint.signalStrength = LE_WIFICLIENT_NO_SIGNAL_STRENGTH;
            createdAccessPointPtr->accessPoint.ssidLength = ssidNumElements;
            memcpy(&createdAccessPointPtr->accessPoint.ssidBytes[0],
                ssidPtr,
                ssidNumElements);

            // Create a Safe Reference for this object.
            returnedRef = le_ref_CreateRef(ScanApRefMap, createdAccessPointPtr);

            LE_DEBUG("AP[%p %p] signal strength %d | SSID length %d | SSID: \"%.*s\"",
                createdAccessPointPtr,
                returnedRef,
                createdAccessPointPtr->accessPoint.signalStrength,
                createdAccessPointPtr->accessPoint.ssidLength,
                createdAccessPointPtr->accessPoint.ssidLength,
                (char *)createdAccessPointPtr->accessPoint.ssidBytes);
        }
        else
        {
            LE_ERROR("le_wifiClient_Create le_mem_ForceAlloc failed.");
        }
    }

    return returnedRef;
}

//--------------------------------------------------------------------------------------------------
/**
 * Deletes an accessPointRef.
 *
 * @return
 *      - LE_OK             Function succeeded.
 *      - LE_BAD_PARAMETER  Invalid parameter.
 *      - LE_BUSY           Function called during scan.
 *
 * @note The handle becomes invalid after it has been deleted.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_Delete
(
    le_wifiClient_AccessPointRef_t apRef
        ///< [IN]
        ///< WiFi Access Point reference.
)
{
    le_result_t         result = LE_BAD_PARAMETER;
    FoundAccessPoint_t *apPtr  = le_ref_Lookup(ScanApRefMap, apRef);

    LE_DEBUG("Delete client called");

    if (IsScanRunning())
    {
        LE_ERROR("ERROR: Scan is running");
        return LE_BUSY;
    }

    // verify le_ref_Lookup
    if (NULL != apPtr)
    {
        RemoveAccessPoint(apRef);
        result = LE_OK;
    }
    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Connect to the WiFi Access Point.
 * All authentication must be set prior to calling this function.
 *
 * @return
 *      - LE_OK             Function succeeded.
 *      - LE_BAD_PARAMETER  Invalid parameter.
 *      - LE_DUPLICATE      Duplicated request.
 *      - LE_TIMEOUT        Connection request time out.
 *      - LE_FAULT          The function failed.
 *
 * @note For PSK credentials see le_wifiClient_SetPassphrase() or le_wifiClient_SetPreSharedKey() .
 * @note For WPA-Enterprise credentials see le_wifiClient_SetUserCredentials()
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_Connect
(
    le_wifiClient_AccessPointRef_t apRef
        ///< [IN]
        ///< WiFi access point reference.
)
{
    le_result_t         result = LE_BAD_PARAMETER;
    FoundAccessPoint_t *apPtr  = le_ref_Lookup(ScanApRefMap, apRef);
    uint16_t ssidLen;

    // verify le_ref_Lookup
    if (NULL !=  apPtr)
    {
        ssidLen = apPtr->accessPoint.ssidLength;
        LE_DEBUG("SSID length %d | SSID: \"%.*s\"", ssidLen, ssidLen,
                 (char *)apPtr->accessPoint.ssidBytes);
        result = pa_wifiClient_Connect(apPtr->accessPoint.ssidBytes, ssidLen);
        if (LE_OK == result)
        {
            CurrentConnection = apRef;
        }
    }
    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * Disconnect from the current connected WiFi Access Point.
 *
 * @return
 *      - LE_OK     Function succeeded.
 *      - LE_FAULT  Function failed.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_Disconnect
(
    void
)
{
    LE_DEBUG("Disconnect");
    CurrentConnection = NULL;
    return pa_wifiClient_Disconnect();
}


//--------------------------------------------------------------------------------------------------
/**
 * This function seeks to load the WEP key of a given SSID from the known secured store path, which
 * has been configured there earlier via the le_wifiClient_ConfigureWep() API.
 *
 * @return:
 *     - LE_OK upon the success in retrieving the WEP key; LE_FAULT otherwise
 */
//--------------------------------------------------------------------------------------------------
static le_result_t WifiClient_LoadCfg_Wep
(
    const char *ssidPtr,
    uint8_t *wepKeyPtr,
    size_t *wepKeyPtrSize
)
{
    le_result_t ret;
    char secStorePath[LE_CFG_STR_LEN_BYTES] = {0};

    snprintf(secStorePath, sizeof(secStorePath), "%s/%s/%s", SECSTORE_WIFI_ITEM_ROOT,
             ssidPtr, SECSTORE_NODE_WEP_KEY);
    *wepKeyPtrSize = LE_WIFIDEFS_MAX_WEPKEY_LENGTH;
    ret = le_secStore_Read(secStorePath, wepKeyPtr, wepKeyPtrSize);
    if (ret != LE_OK)
    {
        LE_ERROR("Failed to read WEP key from secStore path %s for SSID %s; retcode %d",
                 secStorePath, ssidPtr, ret);
        wepKeyPtr[0] = '\0';
        *wepKeyPtrSize = 0;
        return LE_FAULT;
    }

    LE_DEBUG("Succeeded to read WEP key from secStore path %s for SSID %s", secStorePath,
             ssidPtr);
    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * This function seeks to load the WPA EAP configs of a given SSID from the known secured store
 * path, which have been configured there earlier via the le_wifiClient_ConfigureEAP() API. These
 * include the user name and password.
 *
 * @return:
 *     - LE_OK upon the success in retrieving both the user name and password; LE_FAULT otherwise
 */
//--------------------------------------------------------------------------------------------------
static le_result_t WifiClient_LoadCfg_WpaEap
(
    const char *ssidPtr,
    le_wifiClient_SecurityProtocol_t secProtocol,
    uint8_t *usernamePtr,
    size_t *usernamePtrSize,
    uint8_t *passwordPtr,
    size_t *passwordPtrSize
)
{
    le_result_t ret;
    char secStorePath[LE_CFG_STR_LEN_BYTES] = {0};

    snprintf(secStorePath, sizeof(secStorePath), "%s/%s/%s", SECSTORE_WIFI_ITEM_ROOT,
             ssidPtr, SECSTORE_NODE_USERPWD);
    *passwordPtrSize = LE_WIFIDEFS_MAX_PASSWORD_LENGTH;
    ret = le_secStore_Read(secStorePath, passwordPtr, passwordPtrSize);
    if (ret != LE_OK)
    {
        LE_ERROR("Failed to read EAP password from secStore path %s for SSID %s; retcode %d",
                 secStorePath, ssidPtr, ret);
        usernamePtr[0] = '\0';
        *usernamePtrSize = 0;
        passwordPtr[0] = '\0';
        *passwordPtrSize = 0;
        return LE_FAULT;
    }

    snprintf(secStorePath, sizeof(secStorePath), "%s/%s/%s", SECSTORE_WIFI_ITEM_ROOT,
             ssidPtr, SECSTORE_NODE_USERNAME);
    *usernamePtrSize = LE_WIFIDEFS_MAX_USERNAME_LENGTH;
    ret = le_secStore_Read(secStorePath, usernamePtr, usernamePtrSize);
    if (ret != LE_OK)
    {
        LE_ERROR("Failed to read EAP username from secStore path %s for SSID %s; retcode %d",
                 secStorePath, ssidPtr, ret);
        usernamePtr[0] = '\0';
        *usernamePtrSize = 0;
        passwordPtr[0] = '\0';
        *passwordPtrSize = 0;
        return LE_FAULT;
    }

    LE_DEBUG("Succeeded to read EAP username & password from secStore for SSID %s", ssidPtr);
    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * This function seeks to set the retrieved WPA passphrase or PSK of a given SSID into wifiClient
 * before le_wifiClient_Connect() can be attempted. Either a passphrase or a pre-shared key needs
 * to be set before connecting.
 *
 * @return:
 *     - LE_OK upon the success in setting the given WPA passphrase or pre-shared key into
 *       wifiClient; LE_FAULT otherwise
 */
//--------------------------------------------------------------------------------------------------
static le_result_t WifiClient_SetCfg_WpaPsk
(
    le_wifiClient_AccessPointRef_t *apRefPtr,
    const char *ssidPtr,
    uint8_t *passPhrasePtr,
    size_t passPhrasePtrSize,
    uint8_t *preSharedKeyPtr,
    size_t preSharedKeyPtrSize
)
{
    if (passPhrasePtrSize > 0)
    {
        // As passPhrasePtr might not have been null terminated, null-terminate it before passing
        // it to le_wifiClient_SetPassphrase() as a char string. It should have enough space
        // to accommodate this 1 more character.
        passPhrasePtr[passPhrasePtrSize] = '\0';
        if (LE_OK != le_wifiClient_SetPassphrase(*apRefPtr, (const char *)passPhrasePtr))
        {
            LE_ERROR("Failed to config passphrase to start connection over SSID %s", ssidPtr);
            return LE_FAULT;
        }
        return LE_OK;
    }

    if (preSharedKeyPtrSize > 0)
    {
        // As preSharedKeyPtr might not have been null terminated, null-terminate it before passing
        // it to le_wifiClient_SetPreSharedKey() as a char string. It should have enough space
        // to accommodate this 1 more character.
        preSharedKeyPtr[preSharedKeyPtrSize] = '\0';
        if (LE_OK != le_wifiClient_SetPreSharedKey(*apRefPtr, (const char *)preSharedKeyPtr))
        {
            LE_ERROR("Failed to config pre-shared key to start connection over SSID %s", ssidPtr);
            return LE_FAULT;
        }
        return LE_OK;
    }

    LE_ERROR("Failed to set WPA parameters to start connection over SSID %s", ssidPtr);
    return LE_FAULT;
}


//--------------------------------------------------------------------------------------------------
/**
 * This function seeks to load the WPA config of a given SSID from the known secured store path,
 * which has been configured there earlier via the le_wifiClient_ConfigurePsk() API. It'll try to
 * retrieve passphrase 1st. If no valid one can be found, then it proceeds to try retrieving PSK
 * config.
 *
 * @return:
 *     - LE_OK upon the success in retrieving a WPA passphrase or pre-shared key; LE_FAULT otherwise
 */
//--------------------------------------------------------------------------------------------------
static le_result_t WifiClient_LoadCfg_WpaPsk
(
    const char *ssidPtr,
    le_wifiClient_SecurityProtocol_t secProtocol,
    uint8_t *passPhrasePtr,
    size_t *passPhrasePtrSize,
    uint8_t *preSharedKeyPtr,
    size_t *preSharedKeyPtrSize
)
{
    le_result_t ret;
    char secStorePath[LE_CFG_STR_LEN_BYTES] = {0};

    // Try to load passphrase 1st. If no valid one is found, don't quit yet but try loading a
    // pre-shared key
    snprintf(secStorePath, sizeof(secStorePath), "%s/%s/%s", SECSTORE_WIFI_ITEM_ROOT,
             ssidPtr, SECSTORE_NODE_PASSPHRASE);
    *passPhrasePtrSize = LE_WIFIDEFS_MAX_PASSPHRASE_LENGTH;
    ret = le_secStore_Read(secStorePath, passPhrasePtr, passPhrasePtrSize);
    if ((ret == LE_OK) && (*passPhrasePtrSize > 0))
    {
        LE_DEBUG("Succeeded to read passphrase from secStore path %s for SSID %s", secStorePath,
                 ssidPtr);
        return LE_OK;
    }

    LE_WARN("Failed to read passphrase from secStore path %s for SSID %s; retcode %d",
             secStorePath, ssidPtr, ret);
    LE_DEBUG("Failed to retrieve a valid WPA passphrase; try pre-shared key now");
    *passPhrasePtrSize = 0;

    // Try to load pre-shared key as it has failed to load passphrase
    snprintf(secStorePath, sizeof(secStorePath), "%s/%s/%s", SECSTORE_WIFI_ITEM_ROOT,
             ssidPtr, SECSTORE_NODE_PSK);
    *preSharedKeyPtrSize = LE_WIFIDEFS_MAX_PSK_LENGTH;
    ret = le_secStore_Read(secStorePath, preSharedKeyPtr, preSharedKeyPtrSize);
    if ((ret != LE_OK) || (*preSharedKeyPtrSize == 0))
    {
        LE_ERROR("Failed to read pre-shared key from secStore path %s for SSID %s; retcode %d",
                 secStorePath, ssidPtr, ret);
        *preSharedKeyPtrSize = 0;
        return LE_FAULT;
    }

    LE_DEBUG("Succeeded to read pre-shared key from secStore path %s for SSID %s", secStorePath,
             ssidPtr);
    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * This function seeks to load the security configs of a given SSID from the known secured store
 * paths and set them into wifiClient for use. It takes care of the various security protocols
 * supported and the their necessary config parameters. Before setting the valid security configs
 * into use, it creates a local AP reference for the given SSID, into which these security
 * configs will be set before the Wifi client proceeds with le_wifiClient_Connect() to  establish
 * a connection over this SSID.
 *
 * @return:
 *     - LE_OK upon the success in retrieving the necessary security configs and setting them into
 *       place for proceeding with connecting; LE_FAULT otherwise
 */
//--------------------------------------------------------------------------------------------------
static le_result_t WifiClient_LoadSecurityConfigs
(
    const char *ssidPtr,
    le_wifiClient_AccessPointRef_t *apRefPtr,
    le_wifiClient_SecurityProtocol_t secProtocol
)
{
    le_result_t ret;

    // Use unions for these strings to save memory space, as they're mutually exclusive
    union {
        uint8_t passphrase[LE_WIFIDEFS_MAX_PASSPHRASE_BYTES];
        uint8_t username[LE_WIFIDEFS_MAX_USERNAME_BYTES];
        uint8_t wepKey[LE_WIFIDEFS_MAX_WEPKEY_BYTES];
    } u1;
    union {
        uint8_t preSharedKey[LE_WIFIDEFS_MAX_PSK_BYTES];
        uint8_t password[LE_WIFIDEFS_MAX_PASSWORD_BYTES];
    } u2;
    size_t size1 = 0, size2 = 0;

    memset(&u1, 0x0, sizeof(u1));
    memset(&u2, 0x0, sizeof(u2));

    // Load security parameters from configs
    switch (secProtocol)
    {
        case LE_WIFICLIENT_SECURITY_NONE:
            break;

        case LE_WIFICLIENT_SECURITY_WEP:
            ret = WifiClient_LoadCfg_Wep(ssidPtr, u1.wepKey, &size1);
            if (ret != LE_OK)
            {
                return ret;
            }
            break;

        case LE_WIFICLIENT_SECURITY_WPA_PSK_PERSONAL:
        case LE_WIFICLIENT_SECURITY_WPA2_PSK_PERSONAL:
            ret = WifiClient_LoadCfg_WpaPsk(ssidPtr, secProtocol, u1.passphrase, &size1,
                                            u2.preSharedKey, &size2);
            if (ret != LE_OK)
            {
                return ret;
            }
            break;

        case LE_WIFICLIENT_SECURITY_WPA_EAP_PEAP0_ENTERPRISE:
        case LE_WIFICLIENT_SECURITY_WPA2_EAP_PEAP0_ENTERPRISE:
            ret = WifiClient_LoadCfg_WpaEap(ssidPtr, secProtocol, u1.username, &size1,
                                            u2.password, &size2);
            if (ret != LE_OK)
            {
                return ret;
            }
            break;

        default:
            LE_ERROR("Invalid security protocol %d", secProtocol);
            return LE_FAULT;
    }

    LE_DEBUG("Successfully retrieved security parameters for protocol %d over SSID %s",
             secProtocol, ssidPtr);

    // Create the Access Point to connect to
    *apRefPtr = le_wifiClient_Create((const uint8_t *)ssidPtr, strlen(ssidPtr));
    if (!*apRefPtr)
    {
        LE_ERROR("Failed to create Access Point to start connection over SSID %s", ssidPtr);
        return LE_FAULT;
    }

    if (LE_OK != le_wifiClient_SetSecurityProtocol(*apRefPtr, secProtocol))
    {
        LE_ERROR("Failed to set security protocol to start connection over SSID %s", ssidPtr);
        (void)le_wifiClient_Delete(*apRefPtr);
        *apRefPtr = 0;
        return LE_FAULT;
    }

    // Set security parameters into le_wifiClient for use
    switch (secProtocol)
    {
        case LE_WIFICLIENT_SECURITY_NONE:
            ret = LE_OK;
            break;

        case LE_WIFICLIENT_SECURITY_WEP:
            // As wepKey might not have been null terminated, null-terminate it before passing
            // it to le_wifiClient_SetWepKey() as a char string. It should have enough space
            // to accommodate this 1 more character.
            u1.wepKey[size1] = '\0';
            ret = le_wifiClient_SetWepKey(*apRefPtr, (const char *)u1.wepKey);
            break;

        case LE_WIFICLIENT_SECURITY_WPA_PSK_PERSONAL:
        case LE_WIFICLIENT_SECURITY_WPA2_PSK_PERSONAL:
            ret = WifiClient_SetCfg_WpaPsk(apRefPtr, ssidPtr, u1.passphrase, size1,
                                           u2.preSharedKey, size2);
            break;

        case LE_WIFICLIENT_SECURITY_WPA_EAP_PEAP0_ENTERPRISE:
        case LE_WIFICLIENT_SECURITY_WPA2_EAP_PEAP0_ENTERPRISE:
            // As they might not have been null terminated, null-terminate the 2 uint8_t arrays
            // before passing them to le_wifiClient_SetUserCredentials() as char strings. They
            // should have enough space to accommodate this 1 more character.
            u1.username[size1] = '\0';
            u2.password[size2] = '\0';
            ret = le_wifiClient_SetUserCredentials(*apRefPtr, (const char *)u1.username,
                                                   (const char *)u2.password);
            break;

        default:
            break;
    }

    if (ret != LE_OK)
    {
        (void)le_wifiClient_Delete(*apRefPtr);
        *apRefPtr = 0;
        return ret;
    }
    LE_INFO("Succeeded to set into wifiClient security parameters of protocol %d for SSID %s",
            secProtocol, ssidPtr);
    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Load the given SSID's configurations as it is selected as the connection to get established,
 * after creating for it an AP reference
 *
 * @return
 *      - LE_OK     Function succeeded.
 *      - LE_FAULT  Function failed.
 *      - LE_BAD_PARAMETER  Invalid parameter.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_LoadSsid
(
    const uint8_t *ssidPtr,                       ///< [IN] SSID which configs are to be installed
    size_t ssidPtrSize,                           ///< [IN] Length of the SSID in octets
    le_wifiClient_AccessPointRef_t *apRefPtr      ///< [OUT] reference to be created
)
{
    // Retrieve data from config tree
    char configPath[LE_CFG_STR_LEN_BYTES] = {0};
    char ssid[LE_WIFIDEFS_MAX_SSID_BYTES] = {0};
    le_cfg_IteratorRef_t cfg;
    le_wifiClient_SecurityProtocol_t secProtocol;
    le_result_t ret;
    bool is_hidden = false;

    if (!apRefPtr)
    {
        LE_ERROR("Invalid AP reference input for setting configs");
        return LE_BAD_PARAMETER;
    }

    *apRefPtr = 0;

    if (!ssidPtr || (ssidPtrSize == 0))
    {
        LE_ERROR("Invalid SSID input for setting configs");
        return LE_BAD_PARAMETER;
    }

    // Copy the ssidPtr input over, in case it's not null terminated and has no extra space behind
    // to set it there
    memcpy(ssid, ssidPtr, ssidPtrSize);

    snprintf(configPath, sizeof(configPath), "%s/%s/%s", CFG_TREE_ROOT_DIR, CFG_PATH_WIFI, ssid);
    cfg = le_cfg_CreateReadTxn(configPath);
    if (!cfg)
    {
        LE_ERROR("Failed to get configs of SSID %s", ssid);
        return LE_BAD_PARAMETER;
    }

    // Security protocol
    if (!le_cfg_NodeExists(cfg, CFG_NODE_SECPROTOCOL))
    {
        LE_DEBUG("No wifi security protocol set at %s/%s", configPath, CFG_NODE_SECPROTOCOL);
        secProtocol = LE_WIFICLIENT_SECURITY_NONE;
    }
    else
    {
        secProtocol = le_cfg_GetInt(cfg, CFG_NODE_SECPROTOCOL,
                                    LE_WIFICLIENT_SECURITY_WPA2_PSK_PERSONAL);
    }
    ret = WifiClient_LoadSecurityConfigs(ssid, apRefPtr, secProtocol);
    if (ret != LE_OK)
    {
        le_cfg_CancelTxn(cfg);
        return ret;
    }

    // Hidden SSID or not
    if (le_cfg_NodeExists(cfg, CFG_NODE_HIDDEN_SSID))
    {
        is_hidden = le_cfg_GetBool(cfg, CFG_NODE_HIDDEN_SSID, false);
    }

    le_cfg_CancelTxn(cfg);

    if (is_hidden && (LE_OK != le_wifiClient_SetHiddenNetworkAttribute(*apRefPtr, true)))
    {
        LE_ERROR("Failed to set as hidden SSID %s with AP reference %p", ssid, *apRefPtr);
        (void)le_wifiClient_Delete(*apRefPtr);
        *apRefPtr = 0;
        return LE_FAULT;
    }

    LE_INFO("Succeeded to create AP reference %p for SSID %s", *apRefPtr, ssid);
    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Configure the given SSID to use WEP and the given WEP key in the respective input argument.
 *
 * @return
 *      - LE_OK     Succeeded to configure the given WEP key for the given SSID.
 *      - LE_FAULT  Failed to configure.
 *      - LE_BAD_PARAMETER  Invalid parameter.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_ConfigureWep
(
    const uint8_t *ssidPtr,    ///< [IN] SSID which configs are to be installed
    size_t ssidPtrSize,        ///< [IN] Length of the SSID in octets
    const uint8_t *wepKeyPtr,  ///< [IN] WEP key used for this SSID
    size_t wepKeyPtrSize       ///< [IN] Length of the WEP key in octets
)
{
    le_result_t ret;
    char configPath[LE_CFG_STR_LEN_BYTES] = {0};
    char ssid[LE_WIFIDEFS_MAX_SSID_BYTES] = {0};
    le_cfg_IteratorRef_t cfg;

    if (!ssidPtr || (ssidPtrSize == 0))
    {
        LE_ERROR("Invalid inputs: SSID size %d", (int)ssidPtrSize);
        return LE_BAD_PARAMETER;
    }

    if (!wepKeyPtr)
    {
        LE_ERROR("Invalid WEP key");
        return LE_BAD_PARAMETER;
    }

    // Copy the ssidPtr input over, in case it's not null terminated and has no extra space behind
    // to set it there
    memcpy(ssid, ssidPtr, ssidPtrSize);

    // Write secProtocol into config tree
    snprintf(configPath, sizeof(configPath), "%s/%s/%s", CFG_TREE_ROOT_DIR, CFG_PATH_WIFI, ssid);
    cfg = le_cfg_CreateWriteTxn(configPath);
    le_cfg_SetInt(cfg, CFG_NODE_SECPROTOCOL, LE_WIFICLIENT_SECURITY_WEP);
    le_cfg_CommitTxn(cfg);

    // Write WEP key into secStore
    snprintf(configPath, sizeof(configPath), "%s/%s/%s", SECSTORE_WIFI_ITEM_ROOT,
             ssid, SECSTORE_NODE_WEP_KEY);
    ret = le_secStore_Write(configPath, wepKeyPtr, wepKeyPtrSize);
    if (ret != LE_OK)
    {
        LE_ERROR("Failed to write WEP key into secStore path %s for SSID %s; retcode %d",
                 configPath, ssid, ret);
        return LE_FAULT;
    }

    LE_INFO("Succeeded to write WEP configs for SSID %s into secStore", ssid);
    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Configure the given SSID to use PSK and the given pass-phrase or pre-shared key in the
 * respective input arguments. The protocol input is mandatory and has to be set to either
 * LE_WIFICLIENT_SECURITY_WPA_PSK_PERSONAL or LE_WIFICLIENT_SECURITY_WPA2_PSK_PERSONAL.
 * Besides, it's mandatory to have at least one of the pass-phrase and pre-shared key supplied. If
 * both are provided as input, the pass-phrase has precedence and will be used. But it fails to
 * authenticate, a second attempt using the provided pre-shared key will not be done.
 *
 * @return
 *      - LE_OK     Succeeded to configure the given passphrase or pre-shared key for the given
 *                  SSID.
 *      - LE_FAULT  Failed to configure.
 *      - LE_BAD_PARAMETER  Invalid parameter.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_ConfigurePsk
(
    const uint8_t *ssidPtr,                     ///< [IN] SSID which configs are to be installed
    size_t ssidPtrSize,                         ///< [IN] Length of the SSID in octets
    le_wifiClient_SecurityProtocol_t protocol,  ///< [IN] security protocol WPA-PSK or WPA2-PSK
    const uint8_t *passPhrasePtr,               ///< [IN] passphrase used for this SSID
    size_t passPhrasePtrSize,                   ///< [IN] Length of the passphrase in octets
    const uint8_t *pskPtr,                      ///< [IN] pre-shared key used for this SSID
    size_t pskPtrSize                           ///< [IN] Length of the pre-shared key in octets
)
{
    le_result_t ret1 = LE_OK, ret2 = LE_OK;
    char configPath[LE_CFG_STR_LEN_BYTES] = {0};
    char ssid[LE_WIFIDEFS_MAX_SSID_BYTES] = {0};
    le_cfg_IteratorRef_t cfg;

    if ((protocol != LE_WIFICLIENT_SECURITY_WPA_PSK_PERSONAL) &&
        (protocol != LE_WIFICLIENT_SECURITY_WPA2_PSK_PERSONAL))
    {
        LE_ERROR("Incorrect protocol type %d for setting PSK", protocol);
        return LE_BAD_PARAMETER;
    }
    if (!ssidPtr || (ssidPtrSize == 0))
    {
        LE_ERROR("Invalid inputs: SSID size %d", (int)ssidPtrSize);
        return LE_BAD_PARAMETER;
    }

    if (!passPhrasePtr && !pskPtr)
    {
        LE_ERROR("Invalid input: both given passphrase and pre-shared key are null");
        return LE_BAD_PARAMETER;
    }

    // Copy the ssidPtr input over, in case it's not null terminated and has no extra space behind
    // to set it there
    memcpy(ssid, ssidPtr, ssidPtrSize);

    // Write secProtocol into config tree
    snprintf(configPath, sizeof(configPath), "%s/%s/%s", CFG_TREE_ROOT_DIR, CFG_PATH_WIFI, ssid);
    cfg = le_cfg_CreateWriteTxn(configPath);
    le_cfg_SetInt(cfg, CFG_NODE_SECPROTOCOL, protocol);
    le_cfg_CommitTxn(cfg);

    // Write passPhrase into secStore
    if (passPhrasePtr)
    {
        if (passPhrasePtrSize > LE_WIFIDEFS_MAX_PASSPHRASE_LENGTH)
        {
            LE_ERROR("Invalid input: passphrase's length %d has to be shorter than %d",
                     (int)passPhrasePtrSize, LE_WIFIDEFS_MAX_PASSPHRASE_LENGTH);
            ret1 = LE_BAD_PARAMETER;
        }
        else if (passPhrasePtrSize < LE_WIFIDEFS_MIN_PASSPHRASE_LENGTH)
        {
            LE_ERROR("Invalid input: passphrase's length %d has to be longer than %d",
                     (int)passPhrasePtrSize, LE_WIFIDEFS_MIN_PASSPHRASE_LENGTH);
            ret1 = LE_BAD_PARAMETER;
        }
        else
        {
            snprintf(configPath, sizeof(configPath), "%s/%s/%s", SECSTORE_WIFI_ITEM_ROOT,
                     ssid, SECSTORE_NODE_PASSPHRASE);
            ret1 = le_secStore_Write(configPath, passPhrasePtr, passPhrasePtrSize);
            if (ret1 != LE_OK)
            {
                LE_ERROR("Failed to write passphrase into secStore path %s for SSID %s; retcode %d",
                         configPath, ssid, ret2);
            }
            else
            {
                LE_DEBUG("Succeeded writing passphrase into secStore");
            }
        }

        if (!pskPtr)
        {
            return ((ret1 == LE_OK) ? LE_OK : ret1);
        }
    }

    if (pskPtr)
    {
        if (pskPtrSize > LE_WIFIDEFS_MAX_PSK_LENGTH)
        {
            LE_ERROR("Invalid input: pre-shared key's length %d has to be shorter than %d",
                     (int)pskPtrSize, LE_WIFIDEFS_MAX_PSK_LENGTH);
            ret2 = LE_BAD_PARAMETER;
        }
        else
        {
            snprintf(configPath, sizeof(configPath), "%s/%s/%s", SECSTORE_WIFI_ITEM_ROOT,
                     ssid, SECSTORE_NODE_PSK);
            ret2 = le_secStore_Write(configPath, pskPtr, pskPtrSize);
            if (ret2 != LE_OK)
            {
                LE_ERROR("Failed to write pre-shared key into secStore path %s for SSID %s; "
                         "retcode %d", configPath, ssid, ret2);
            }
            else
            {
                LE_DEBUG("Succeeded to write pre-shared key into secStore");
            }
        }

        if (!passPhrasePtr)
        {
            return ((ret2 == LE_OK) ? LE_OK : ret2);
        }
    }

    if ((ret1 != LE_OK) && (ret2 != LE_OK))
    {
        return LE_FAULT;
    }

    LE_INFO("Succeeded to write PSK configs for SSID %s into secStore", ssid);
    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Configure the given SSID to use EAP and the given EAP username and password in the respective
 * input arguments. The protocol input is mandatory and has to be set to either
 * LE_WIFICLIENT_SECURITY_WPA_EAP_PEAP0_ENTERPRISE or
 * LE_WIFICLIENT_SECURITY_WPA2_EAP_PEAP0_ENTERPRISE. Besides, both the username and password inputs
 * are mandatory.
 *
 * @return
 *      - LE_OK     Succeeded to configure the given EAP username and password for the given
 *                  SSID.
 *      - LE_FAULT  Failed to configure.
 *      - LE_BAD_PARAMETER  Invalid parameter.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_ConfigureEap
(
    const uint8_t *ssidPtr,                     ///< [IN] SSID which configs are to be installed
    size_t ssidPtrSize,                         ///< [IN] Length of the SSID in octets
    le_wifiClient_SecurityProtocol_t protocol,  ///< [IN] security protocol WPA-EAP or WPA2-EAP
    const uint8_t *usernamePtr,                 ///< [IN] EAP username used for this SSID
    size_t usernamePtrSize,                     ///< [IN] Length of the username in octets
    const uint8_t *passwordPtr,                 ///< [IN] EAP password used for this SSID
    size_t passwordPtrSize                      ///< [IN] Length of the password in octets
)
{
    le_result_t ret;
    char configPath[LE_CFG_STR_LEN_BYTES] = {0};
    char ssid[LE_WIFIDEFS_MAX_SSID_BYTES] = {0};
    le_cfg_IteratorRef_t cfg;

    if ((protocol != LE_WIFICLIENT_SECURITY_WPA_EAP_PEAP0_ENTERPRISE) &&
        (protocol != LE_WIFICLIENT_SECURITY_WPA2_EAP_PEAP0_ENTERPRISE))
    {
        LE_ERROR("Incorrect protocol type %d for setting EAP", protocol);
        return LE_BAD_PARAMETER;
    }
    if (!ssidPtr || (ssidPtrSize == 0))
    {
        LE_ERROR("Invalid inputs: SSID size %d", (int)ssidPtrSize);
        return LE_BAD_PARAMETER;
    }

    if (!usernamePtr || !passwordPtr)
    {
        LE_ERROR("Invalid input: username %p password %p", usernamePtr, passwordPtr);
        return LE_BAD_PARAMETER;
    }

    // Copy the ssidPtr input over, in case it's not null terminated and has no extra space behind
    // to set it there
    memcpy(ssid, ssidPtr, ssidPtrSize);

    // Write secProtocol into config tree
    snprintf(configPath, sizeof(configPath), "%s/%s/%s", CFG_TREE_ROOT_DIR, CFG_PATH_WIFI, ssid);
    cfg = le_cfg_CreateWriteTxn(configPath);
    le_cfg_SetInt(cfg, CFG_NODE_SECPROTOCOL, protocol);
    le_cfg_CommitTxn(cfg);

    // Write username & password into secStore
    snprintf(configPath, sizeof(configPath), "%s/%s/%s", SECSTORE_WIFI_ITEM_ROOT,
             ssid, SECSTORE_NODE_USERNAME);
    ret = le_secStore_Write(configPath, usernamePtr, usernamePtrSize);
    if (ret != LE_OK)
    {
        LE_ERROR("Failed to write username into secStore path %s for SSID %s; retcode %d",
                 configPath, ssid, ret);
        return LE_FAULT;
    }

    snprintf(configPath, sizeof(configPath), "%s/%s/%s", SECSTORE_WIFI_ITEM_ROOT,
             ssid, SECSTORE_NODE_USERPWD);
    ret = le_secStore_Write(configPath, passwordPtr, passwordPtrSize);
    if (ret != LE_OK)
    {
        LE_ERROR("Failed to write user password into secStore path %s for SSID %s; retcode %d",
                 configPath, ssid, ret);
        return LE_FAULT;
    }

    LE_INFO("Succeeded to write EAP configs for SSID %s into secStore", ssid);
    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Remove and clear Wifi's security configurations to use with the given SSID from the config tree
 * and secured store. This includes the security protocol and all the username, password,
 * passphrase, pre-shared key, key, etc., previously configured via le_wifiClient_Configure APIs for
 * WEP, PSK and EAP.
 *
 * @return
 *      - LE_OK upon success to deconfigure the given SSID's configured user credentials;
 *        LE_FAULT otherwise
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_wifiClient_RemoveSsidSecurityConfigs
(
    const uint8_t *ssidPtr,         ///< [IN] SSID which user credentials are to be deconfigured
    size_t ssidPtrSize              ///< [IN] Length of the SSID in octets
)
{
    le_result_t ret;
    char ssid[LE_WIFIDEFS_MAX_SSID_BYTES] = {0};
    char configPath[LE_CFG_STR_LEN_BYTES] = {0};
    le_cfg_IteratorRef_t cfg;

    if (!ssidPtr || (ssidPtrSize == 0))
    {
        LE_ERROR("Invalid inputs: SSID size %d", (int)ssidPtrSize);
        return LE_BAD_PARAMETER;
    }

    // Copy the ssidPtr input over, in case it's not null terminated and has no extra space behind
    // to set it there
    memcpy(ssid, ssidPtr, ssidPtrSize);
    snprintf(configPath, sizeof(configPath), "%s/%s", SECSTORE_WIFI_ITEM_ROOT, ssid);
    ret = le_secStore_Delete(configPath);
    if (ret == LE_NOT_FOUND)
    {
        LE_WARN("Found secStore path %s non-existent to remove", configPath);
    }
    else if (ret == LE_OK)
    {
        // Clear secProtocol setting on config tree
        snprintf(configPath, sizeof(configPath), "%s/%s/%s", CFG_TREE_ROOT_DIR, CFG_PATH_WIFI,
                 ssid);
        cfg = le_cfg_CreateWriteTxn(configPath);
        le_cfg_SetInt(cfg, CFG_NODE_SECPROTOCOL, LE_WIFICLIENT_SECURITY_NONE);
        le_cfg_CommitTxn(cfg);
        LE_INFO("Succeeded to delete from secStore user credentials for SSID %s", ssid);
    }
    else
    {
        LE_ERROR("Failed to delete user credentials for SSID %s on secStore path %s; retcode %d",
                 ssid, configPath, ret);
    }
    return ret;
}


//--------------------------------------------------------------------------------------------------
/**
 *  WiFi Client COMPONENT Init
 */
//--------------------------------------------------------------------------------------------------
void le_wifiClient_Init
(
    void
)
{
    LE_DEBUG("WiFi client service starting...");

    pa_wifiClient_Init();

    // Create the Access Point object pool.
    AccessPointPool = le_mem_CreatePool("le_wifi_FoundAccessPointPool", sizeof(FoundAccessPoint_t));
    le_mem_ExpandPool(AccessPointPool, INIT_AP_COUNT);

    // Create the Safe Reference Map to use for FoundAccessPoint_t object Safe References.
    ScanApRefMap = le_ref_CreateMap("le_wifiClient_AccessPoints", INIT_AP_COUNT);

    // Create an event Id for new WiFi Events
    WifiEventId = le_event_CreateIdWithRefCounting("WifiConnectState");
    WifiEventPool = le_mem_CreatePool("WifiConnectStatePool", sizeof(le_wifiClient_EventInd_t));

    // register for events from PA.
    pa_wifiClient_AddEventIndHandler(PaEventHandler, NULL);

    // Add a handler to handle the close
    le_msg_AddServiceCloseHandler(le_wifiClient_GetServiceRef(), CloseSessionEventHandler, NULL);
}
