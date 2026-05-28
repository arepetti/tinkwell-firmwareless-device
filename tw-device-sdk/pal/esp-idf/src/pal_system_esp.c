/*
 * pal_system_esp.c -- System info via ESP-IDF.
 *
 * SPDX-License-Identifier: MIT
 */

#include "pal_system.h"
#include "pal_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

#include <stdio.h>

#define TAG "system"

void pal_system_reboot(void)
{
    esp_restart();
}

void pal_system_reboot_to_factory(void)
{
    const esp_partition_t *factory = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    if (factory) {
        esp_ota_set_boot_partition(factory);
        PAL_LOGI(TAG, "rebooting to factory partition");
        esp_restart();
    } else {
        PAL_LOGW(TAG, "factory partition not found, normal reboot");
        esp_restart();
    }
}

pal_boot_reason_t pal_system_boot_reason(void)
{
    esp_reset_reason_t r = esp_reset_reason();
    switch (r) {
    case ESP_RST_POWERON:  return PAL_BOOT_COLD;
    case ESP_RST_SW:       return PAL_BOOT_OTA;
    case ESP_RST_PANIC:
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      return PAL_BOOT_WATCHDOG;
    case ESP_RST_DEEPSLEEP: return PAL_BOOT_DEEPSLEEP;
    default:                return PAL_BOOT_UNKNOWN;
    }
}

uint32_t pal_system_free_heap(void)
{
    return (uint32_t)esp_get_free_heap_size();
}

const char *pal_system_chip_info(void)
{
    static char buf[64];
    esp_chip_info_t info;
    esp_chip_info(&info);

    const char *model;
    switch (info.model) {
    case CHIP_ESP32:   model = "ESP32";    break;
    case CHIP_ESP32S2: model = "ESP32-S2"; break;
    case CHIP_ESP32S3: model = "ESP32-S3"; break;
    case CHIP_ESP32C3: model = "ESP32-C3"; break;
    case CHIP_ESP32C6: model = "ESP32-C6"; break;
    case CHIP_ESP32H2: model = "ESP32-H2"; break;
    default:           model = "unknown";  break;
    }
    snprintf(buf, sizeof(buf), "%s rev%d %dcore",
             model, info.revision, info.cores);
    return buf;
}
