/*
 * Factory provisioning application.
 *
 * This runs from the "factory" flash partition.  It performs
 * factory identity provisioning (Phase 1) and optionally hub
 * provisioning (Phase 2), then reboots into the main OTA
 * application.
 *
 * The factory app shares the same NVS namespace as the main app,
 * so identity fields written here are immediately visible after
 * reboot.
 *
 * SPDX-License-Identifier: MIT
 */

#include "tw_identity.h"
#include "tw_kvtext.h"
#include "tw_device.h"
#include "pal_nvs.h"
#include "pal_log.h"
#include "pal_os.h"
#include "pal_system.h"

#define TAG "factory"

static const tw_device_config_t factory_cfg = {
    .name                 = "factory-provisioning",
    .fw_version           = "0.0.0",
    .vendor_id            = 0,
    .product_id           = 0,
    .vendor_display_name  = "Unknown",
    .product_display_name = "Unknown",
    .variant              = 0,
    .resources            = NULL,
    .on_init              = NULL,
    .on_tick              = NULL,
    .tick_interval_ms     = 1000,
};

void app_main(void)
{
    PAL_LOGI(TAG, "=== TW Factory Provisioning ===");

    pal_nvs_init();
    tw_identity_init(&factory_cfg);

    const tw_device_identity_t *id = tw_identity_get();
    PAL_LOGI(TAG, "current identity: vendor=%d product=%d factory=%d hub=%d",
             id->vendor_id, id->product_id,
             id->factory_provisioned, id->hub_provisioned);

    /*
     * The provisioning flow is handled by svc_provision_run().
     * On BLE-capable chips it starts BLE GATT advertising.
     * On WiFi chips it can use SoftAP + CoAP.
     *
     * The factory app only starts provisioning if the device has
     * NOT already been factory-provisioned.  If it has, it
     * immediately reboots to the main app.
     */
    if (id->factory_provisioned) {
        PAL_LOGI(TAG, "already factory-provisioned, booting main app");
    } else {
        PAL_LOGI(TAG, "device not factory-provisioned, starting provisioning...");

        extern bool     svc_provision_is_needed(void);
        extern tw_err_t svc_provision_run(const tw_device_config_t *cfg);

        tw_err_t err = svc_provision_run(&factory_cfg);
        if (err != TW_OK) {
            PAL_LOGW(TAG, "provisioning returned %d", err);
        }
    }

    PAL_LOGI(TAG, "switching to main application...");
    pal_sleep_ms(500);

    /* Set boot partition to OTA-0 (the main app). */
    extern void pal_system_reboot(void);
    pal_system_reboot();
}
