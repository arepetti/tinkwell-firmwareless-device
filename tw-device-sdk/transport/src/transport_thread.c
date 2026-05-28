/*
 * transport_thread.c -- Thread (802.15.4) transport for ESP-IDF.
 *
 * Uses the OpenThread stack included in ESP-IDF.  Thread provides
 * IPv6 mesh networking over 802.15.4 radio (ESP32-C6, ESP32-H2).
 *
 * After joining the Thread network, the lwIP IPv6 stack provides
 * standard UDP sockets -- no changes needed in pal_net_esp.c.
 *
 * SPDX-License-Identifier: MIT
 */

#ifdef TW_PLATFORM_ESPIDF
#ifdef CONFIG_TW_TRANSPORT_THREAD

#include "transport.h"
#include "pal_log.h"
#include "pal_nvs.h"

#include "esp_openthread.h"
#include "esp_openthread_types.h"
#include "esp_openthread_netif_glue.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "openthread/instance.h"
#include "openthread/thread.h"
#include "openthread/dataset.h"
#include "openthread/tasklet.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <stdio.h>

#define TAG "thread"

#define OT_TASK_STACK_SIZE           8192
#define OT_TASK_PRIORITY             5
#define OT_POLL_INTERVAL_MS          100
#define OT_ATTACH_TIMEOUT_ITERATIONS 300

static bool s_connected;
static bool s_running;

static esp_openthread_platform_config_t s_ot_platform_cfg = {
    .radio_config = ESP_OPENTHREAD_DEFAULT_RADIO_CONFIG(),
    .host_config  = ESP_OPENTHREAD_DEFAULT_HOST_CONFIG(),
    .port_config  = ESP_OPENTHREAD_DEFAULT_PORT_CONFIG(),
};

/**
 * OpenThread FreeRTOS worker: applies dataset, runs the OT main loop/tasklets until
 * stopped, and exposes mesh attachment state so the transport layer can unblock init.
 */
static void ot_task(void *arg)
{
    (void)arg;

    esp_openthread_init(&s_ot_platform_cfg);

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_OPENTHREAD();
    esp_netif_t *netif = esp_netif_new(&netif_cfg);
    esp_openthread_netif_glue_init(netif);

    otInstance *instance = esp_openthread_get_instance();

    /* Try loading dataset from NVS. */
    otOperationalDataset dataset;
    memset(&dataset, 0, sizeof(dataset));

    char network_name[17] = {0};
    char network_key[33]  = {0};

    if (pal_nvs_get_str("thread_name", network_name, sizeof(network_name)) == TW_OK &&
        pal_nvs_get_str("thread_key",  network_key,  sizeof(network_key))  == TW_OK) {
        PAL_LOGI(TAG, "using provisioned Thread network: %s", network_name);
    } else {
#ifdef CONFIG_TW_THREAD_NETWORK_NAME
        strncpy(network_name, CONFIG_TW_THREAD_NETWORK_NAME, sizeof(network_name) - 1);
        strncpy(network_key,  CONFIG_TW_THREAD_NETWORK_KEY,  sizeof(network_key) - 1);
#else
        PAL_LOGE(TAG, "no Thread credentials available");
        vTaskDelete(NULL);
        return;
#endif
    }

    otOperationalDatasetInit(instance, &dataset);
    memcpy(dataset.mNetworkName.m8, network_name, strlen(network_name));
    dataset.mComponents.mIsNetworkNamePresent = true;

    if (network_key[0]) {
        size_t klen = strlen(network_key);
        for (size_t ki = 0; ki < sizeof(dataset.mNetworkKey.m8) && ki * 2 + 1 < klen; ki++) {
            unsigned int b;
            if (sscanf(network_key + ki * 2, "%2x", &b) == 1)
                dataset.mNetworkKey.m8[ki] = (uint8_t)b;
        }
        dataset.mComponents.mIsNetworkKeyPresent = true;
    }

    otDatasetSetActive(instance, &dataset);
    otIp6SetEnabled(instance, true);
    otThreadSetEnabled(instance, true);

    PAL_LOGI(TAG, "OpenThread started, joining network...");

    while (s_running) {
        esp_openthread_mainloop_update();
        otTaskletsProcess(instance);

        otDeviceRole role = otThreadGetDeviceRole(instance);
        bool was_connected = s_connected;
        s_connected = (role >= OT_DEVICE_ROLE_CHILD);

        if (s_connected && !was_connected) {
            PAL_LOGI(TAG, "attached as %s",
                     role == OT_DEVICE_ROLE_CHILD  ? "child"  :
                     role == OT_DEVICE_ROLE_ROUTER ? "router" : "leader");
        }

        vTaskDelay(pdMS_TO_TICKS(OT_POLL_INTERVAL_MS));
    }

    vTaskDelete(NULL);
}

/**
 * Starts OpenThread on a dedicated task and blocks until the device attaches or times out.
 */
tw_err_t tw_transport_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_running = true;
    xTaskCreate(ot_task, "ot_main", OT_TASK_STACK_SIZE, NULL, OT_TASK_PRIORITY, NULL);

    /* Wait for attachment (OT_ATTACH_TIMEOUT_ITERATIONS * OT_POLL_INTERVAL_MS ms). */
    for (int i = 0; i < OT_ATTACH_TIMEOUT_ITERATIONS; i++) {
        if (s_connected) return TW_OK;
        vTaskDelay(pdMS_TO_TICKS(OT_POLL_INTERVAL_MS));
    }

    PAL_LOGE(TAG, "Thread attachment timed out");
    return TW_ERR_TIMEOUT;
}

/** Stops the OpenThread task loop so the radio/stack can be released. */
void tw_transport_deinit(void)
{
    s_running   = false;
    s_connected = false;
}

/** True when the device has reached at least child role on the Thread network. */
bool tw_transport_connected(void)
{
    return s_connected;
}

/** Human-readable transport label for logs and diagnostics. */
const char *tw_transport_name(void)
{
    return "Thread";
}

#endif /* CONFIG_TW_TRANSPORT_THREAD */
#endif /* TW_PLATFORM_ESPIDF */
