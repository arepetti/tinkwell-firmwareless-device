/*
 * transport_ble.c -- BLE transport for ESP-IDF.
 *
 * BLE does NOT provide IP sockets.  This transport registers a
 * custom GATT service that tunnels CoAP-like request/response
 * pairs over BLE characteristics.  The PAL networking layer is
 * bypassed; instead, proto_coap.c calls into a transport-specific
 * dispatch when CONFIG_TW_TRANSPORT_BLE is active.
 *
 * SPDX-License-Identifier: MIT
 */

#ifdef TW_PLATFORM_ESPIDF
#ifdef CONFIG_TW_TRANSPORT_BLE

#include "transport.h"
#include "pal_log.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"

#include <string.h>

#define TAG "ble"

/*
 * TW CoAP-over-BLE GATT service UUID: a9e11001-e237-4f6a-9f5b-bc3b9a1c3d6a
 *
 * Characteristics:
 *   REQUEST  (write):  a9e11002-...  -- client writes CoAP-like request
 *   RESPONSE (notify): a9e11003-...  -- server sends CoAP-like response
 *
 * CoAP-over-BLE here is a proprietary TW tunnel over GATT (not IETF CoAP over BLE;
 * see RFC 7252 for UDP CoAP; there is no single standard CoAP-over-BLE mapping).
 */

static bool s_connected;

/**
 * GATT server callback: tracks connection state and will eventually dispatch
 * tunneled application payloads to the CoAP resource layer.
 */
static void gatts_event_handler(esp_gatts_cb_event_t event,
                                esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_CONNECT_EVT:
        PAL_LOGI(TAG, "BLE peer connected");
        s_connected = true;
        break;
    case ESP_GATTS_DISCONNECT_EVT:
        PAL_LOGW(TAG, "BLE peer disconnected");
        s_connected = false;
        esp_ble_gap_start_advertising(NULL);
        break;
    case ESP_GATTS_WRITE_EVT:
        /*
         * TODO: Parse the incoming CoAP-over-BLE request from
         * param->write.value, dispatch through the CoAP resource
         * table, and send the response via NOTIFY on the response
         * characteristic.
         */
        PAL_LOGD(TAG, "GATT write: handle=%d len=%d",
                 param->write.handle, param->write.len);
        break;
    default:
        break;
    }
}

/**
 * GAP callback: logs advertising lifecycle so bring-up can confirm the stack is usable.
 */
static void gap_event_handler(esp_gap_ble_cb_event_t event,
                              esp_ble_gap_cb_param_t *param)
{
    (void)param;
    if (event == ESP_GAP_BLE_ADV_START_COMPLETE_EVT) {
        PAL_LOGI(TAG, "advertising started");
    }
}

/**
 * Enables the BLE controller and Bluedroid so the device can expose the TW tunnel
 * service once GATT registration is completed.
 */
tw_err_t tw_transport_init(void)
{
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    esp_ble_gatts_register_callback(gatts_event_handler);
    esp_ble_gap_register_callback(gap_event_handler);

    /*
     * TODO: Register the TW CoAP-over-BLE GATT service and
     * characteristics, then start advertising.
     */

    PAL_LOGI(TAG, "BLE transport initialised");
    return TW_OK;
}

/** Tears down BLE so another transport or subsystem can use the radio. */
void tw_transport_deinit(void)
{
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    s_connected = false;
}

/** Reports whether a GATT client is connected (used to gate outbound work). */
bool tw_transport_connected(void)
{
    return s_connected;
}

/** Human-readable transport label for logs and diagnostics. */
const char *tw_transport_name(void)
{
    return "BLE";
}

#endif /* CONFIG_TW_TRANSPORT_BLE */
#endif /* TW_PLATFORM_ESPIDF */
