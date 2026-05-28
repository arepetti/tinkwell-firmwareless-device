/*
 * transport_eth.c -- Ethernet transport for ESP-IDF.
 *
 * Initialises the internal EMAC or SPI-based PHY (e.g. W5500)
 * and waits for an IP address via DHCP.
 *
 * SPDX-License-Identifier: MIT
 */

#ifdef TW_PLATFORM_ESPIDF
#ifdef CONFIG_TW_TRANSPORT_ETHERNET

#include "transport.h"
#include "pal_log.h"

#include "esp_eth.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include <string.h>

#define TAG "eth"

#define ETH_CONNECTED_BIT BIT0
#define ETH_FAIL_BIT      BIT1

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

/** When DHCP wait duration is zero, still cap wait for link/IP using this fallback (ms). */
#define DHCP_FALLBACK_TIMEOUT_MS 30000

static esp_netif_t *s_netif;

static EventGroupHandle_t s_eth_ev;
static bool               s_connected;

/**
 * Ethernet/IP event sink: tracks link loss and signals when IPv4 is assigned so
 * tw_transport_init() can proceed or apply static IP after DHCP timeout.
 */
static void event_handler(void *arg, esp_event_base_t base,
                          int32_t id, void *event_data)
{
    (void)arg;
    if (base == ETH_EVENT) {
        switch (id) {
        case ETHERNET_EVENT_CONNECTED:
            PAL_LOGI(TAG, "link up");
            break;
        case ETHERNET_EVENT_DISCONNECTED:
            PAL_LOGW(TAG, "link down");
            s_connected = false;
            xEventGroupSetBits(s_eth_ev, ETH_FAIL_BIT);
            break;
        default:
            break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_ETH_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)event_data;
        PAL_LOGI(TAG, "got ip:" IPSTR, IP2STR(&ev->ip_info.ip));
        s_connected = true;
        xEventGroupSetBits(s_eth_ev, ETH_CONNECTED_BIT);
    }
}

/**
 * Installs the EMAC/PHY stack, waits for DHCP (or timeout), then may configure a
 * static IPv4 address so development boards stay on the network without a DHCP server.
 */
tw_err_t tw_transport_init(void)
{
    s_eth_ev = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    s_netif = esp_netif_new(&netif_cfg);
    esp_netif_t *netif = s_netif;

    eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();

#if CONFIG_TW_ETH_USE_INTERNAL_EMAC
    eth_esp32_emac_config_t emac_cfg = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac_cfg, &mac_cfg);
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_cfg);
#else
    /* SPI-based Ethernet (e.g. W5500). Adjust pins via Kconfig. */
    PAL_LOGE(TAG, "SPI Ethernet not yet configured -- add your PHY here");
    return TW_ERR_NOT_READY;
#endif

    esp_eth_config_t eth_cfg = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth_handle = NULL;
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_cfg, &eth_handle));
    ESP_ERROR_CHECK(esp_netif_attach(netif, esp_eth_new_netif_glue(eth_handle)));

    esp_event_handler_instance_t inst_eth, inst_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        ETH_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &inst_eth));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_ETH_GOT_IP, &event_handler, NULL, &inst_ip));

    ESP_ERROR_CHECK(esp_eth_start(eth_handle));

    uint32_t dhcp_timeout_ms = (uint32_t)CONFIG_TW_NET_DHCP_TIMEOUT_S * 1000;
    if (!CONFIG_TW_NET_USE_DHCP)
        dhcp_timeout_ms = 0;

    EventBits_t bits = xEventGroupWaitBits(
        s_eth_ev, ETH_CONNECTED_BIT | ETH_FAIL_BIT,
        pdFALSE, pdFALSE,
        pdMS_TO_TICKS(dhcp_timeout_ms > 0 ? dhcp_timeout_ms : DHCP_FALLBACK_TIMEOUT_MS));

    if (!(bits & ETH_CONNECTED_BIT)) {
        PAL_LOGW(TAG, "DHCP timed out, applying static IP: %s",
                 CONFIG_TW_NET_STATIC_IP);
        if (s_netif) {
            esp_netif_dhcpc_stop(s_netif);
            esp_netif_ip_info_t ip_info = {0};
            esp_netif_str_to_ip4(CONFIG_TW_NET_STATIC_IP, &ip_info.ip);
            esp_netif_str_to_ip4(CONFIG_TW_NET_STATIC_NETMASK, &ip_info.netmask);
            esp_netif_str_to_ip4(CONFIG_TW_NET_STATIC_GW, &ip_info.gw);
            esp_netif_set_ip_info(s_netif, &ip_info);
            s_connected = true;
            PAL_LOGI(TAG, "static IP configured: %s", CONFIG_TW_NET_STATIC_IP);
        } else {
            PAL_LOGE(TAG, "Ethernet connection timed out");
            return TW_ERR_TIMEOUT;
        }
    }
    return TW_OK;
}

/** Placeholder teardown: full driver release is left to platform integration. */
void tw_transport_deinit(void)
{
    s_connected = false;
}

/** True once IPv4 is configured (DHCP or static fallback). */
bool tw_transport_connected(void)
{
    return s_connected;
}

/** Human-readable transport label for logs and diagnostics. */
const char *tw_transport_name(void)
{
    return "Ethernet";
}

#endif /* CONFIG_TW_TRANSPORT_ETHERNET */
#endif /* TW_PLATFORM_ESPIDF */
