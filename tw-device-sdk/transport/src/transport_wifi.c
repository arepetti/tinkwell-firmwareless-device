/*
 * transport_wifi.c -- WiFi Station transport for ESP-IDF.
 *
 * Credentials come from NVS (written during provisioning).  If
 * CONFIG_TW_WIFI_SSID / CONFIG_TW_WIFI_PASSWORD are set at build time
 * they are used as fallback for development.
 *
 * SPDX-License-Identifier: MIT
 */

#ifdef TW_PLATFORM_ESPIDF

#include "transport.h"
#include "pal_log.h"
#include "pal_nvs.h"
#include "tw_net_constants.h"

#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <string.h>

#define TAG "wifi"

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1
#define MAX_RETRY           5

/** When DHCP wait duration is zero (static IP mode), still cap wait for STA connect using this fallback (ms). */
#define DHCP_FALLBACK_TIMEOUT_MS 30000

#ifndef CONFIG_TW_NET_USE_DHCP
#define CONFIG_TW_NET_USE_DHCP 1
#endif
#ifndef CONFIG_TW_NET_DHCP_TIMEOUT_S
#define CONFIG_TW_NET_DHCP_TIMEOUT_S 15
#endif
#ifndef CONFIG_TW_NET_STATIC_IP
#define CONFIG_TW_NET_STATIC_IP "192.168.1.100"
#endif
#ifndef CONFIG_TW_NET_STATIC_NETMASK
#define CONFIG_TW_NET_STATIC_NETMASK "255.255.255.0"
#endif
#ifndef CONFIG_TW_NET_STATIC_GW
#define CONFIG_TW_NET_STATIC_GW "192.168.1.1"
#endif

static EventGroupHandle_t s_wifi_ev;
static int                s_retry_count;
static bool               s_connected;

/**
 * ESP-IDF WiFi/IP event sink: drives STA reconnect with bounded retries, signals
 * success or failure to the wait in tw_transport_init() so provisioning can
 * fall back to a static address when DHCP never completes.
 */
static void event_handler(void *arg, esp_event_base_t base,
                          int32_t id, void *event_data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        if (s_retry_count < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_count++;
            PAL_LOGW(TAG, "retry %d/%d", s_retry_count, MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_ev, WIFI_FAIL_BIT);
            PAL_LOGE(TAG, "connection failed after %d retries", MAX_RETRY);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)event_data;
        PAL_LOGI(TAG, "got ip:" IPSTR, IP2STR(&ev->ip_info.ip));
        s_retry_count = 0;
        s_connected   = true;
        xEventGroupSetBits(s_wifi_ev, WIFI_CONNECTED_BIT);
    }
}

/**
 * Brings up WiFi STA from NVS or Kconfig credentials, waits for L3 (or timeout),
 * then optionally applies static IP so the device remains reachable in lab setups.
 */
tw_err_t tw_transport_init(void)
{
    s_wifi_ev = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_cfg));

    esp_event_handler_instance_t inst_any, inst_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &inst_any));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &inst_ip));

    wifi_config_t sta_cfg;
    memset(&sta_cfg, 0, sizeof(sta_cfg));

    /* Try NVS first (set during provisioning). */
    char ssid[TW_WIFI_SSID_BUF_SIZE] = {0};
    char pass[TW_WIFI_PASS_BUF_SIZE] = {0};
    if (pal_nvs_get_str("wifi_ssid", ssid, sizeof(ssid)) == TW_OK &&
        pal_nvs_get_str("wifi_pass", pass, sizeof(pass)) == TW_OK) {
        PAL_LOGI(TAG, "using provisioned SSID: %s", ssid);
    } else {
#ifdef CONFIG_TW_WIFI_SSID
        strncpy(ssid, CONFIG_TW_WIFI_SSID, sizeof(ssid) - 1);
        strncpy(pass, CONFIG_TW_WIFI_PASSWORD, sizeof(pass) - 1);
        PAL_LOGI(TAG, "using Kconfig SSID: %s", ssid);
#else
        PAL_LOGE(TAG, "no WiFi credentials available");
        return TW_ERR_NOT_READY;
#endif
    }

    memcpy(sta_cfg.sta.ssid, ssid, sizeof(sta_cfg.sta.ssid));
    memcpy(sta_cfg.sta.password, pass, sizeof(sta_cfg.sta.password));
    sta_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    uint32_t dhcp_timeout_ms = (uint32_t)CONFIG_TW_NET_DHCP_TIMEOUT_S * 1000;
    if (!CONFIG_TW_NET_USE_DHCP)
        dhcp_timeout_ms = 0;

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_ev, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE, pdFALSE,
        pdMS_TO_TICKS(dhcp_timeout_ms > 0 ? dhcp_timeout_ms : DHCP_FALLBACK_TIMEOUT_MS));

    if (!(bits & WIFI_CONNECTED_BIT)) {
        PAL_LOGW(TAG, "DHCP timed out, applying static IP: %s",
                 CONFIG_TW_NET_STATIC_IP);

        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif) {
            esp_netif_dhcpc_stop(netif);
            esp_netif_ip_info_t ip_info = {0};
            esp_netif_str_to_ip4(CONFIG_TW_NET_STATIC_IP, &ip_info.ip);
            esp_netif_str_to_ip4(CONFIG_TW_NET_STATIC_NETMASK, &ip_info.netmask);
            esp_netif_str_to_ip4(CONFIG_TW_NET_STATIC_GW, &ip_info.gw);
            esp_netif_set_ip_info(netif, &ip_info);
            s_connected = true;
            PAL_LOGI(TAG, "static IP configured: %s", CONFIG_TW_NET_STATIC_IP);
        } else {
            PAL_LOGE(TAG, "WiFi STA connection timed out");
            return TW_ERR_TIMEOUT;
        }
    }
    return TW_OK;
}

/** Stops WiFi to free RF and power for another transport or sleep. */
void tw_transport_deinit(void)
{
    esp_wifi_stop();
    esp_wifi_deinit();
    s_connected = false;
}

/** True after STA has obtained IPv4 (or static fallback was applied successfully). */
bool tw_transport_connected(void)
{
    return s_connected;
}

/** Human-readable transport label for logs and diagnostics. */
const char *tw_transport_name(void)
{
    return "WiFi";
}

#endif /* TW_PLATFORM_ESPIDF */
