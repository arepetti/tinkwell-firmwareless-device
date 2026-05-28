/*
 * tw_device.h -- Main entry point for the TW Device SDK.
 *
 * A user fills in a tw_device_config_t struct with their application
 * callbacks and message resource table, then uses TW_DEVICE_MAIN() to
 * generate the platform-appropriate entry point.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TW_DEVICE_H
#define TW_DEVICE_H

#include "tw_types.h"
#include "tw_msg.h"
#include "tw_identity.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Heartbeat payload callback
 * -------------------------------------------------------------------------*/

/**
 * @brief Optional hook to append custom bytes to each heartbeat payload before send.
 *
 * Used so status, sensor snapshots, or other domain data reach the hub without
 * hard-coding formats inside the SDK.
 *
 * @param buf Destination buffer for extra payload bytes.
 * @param buf_size Maximum number of bytes that may be written to @a buf.
 * @param out_len On success, set to the number of bytes written (may be zero).
 * @retval TW_OK to include the written bytes in the heartbeat; non-success causes the SDK to omit the extra payload for that cycle.
 */
typedef tw_err_t (*tw_heartbeat_fn_t)(uint8_t *buf, size_t buf_size,
                                      size_t *out_len);

/* ---------------------------------------------------------------------------
 * Device configuration
 * -------------------------------------------------------------------------*/

/** @brief Application-supplied configuration for ::tw_device_run and identity defaults. */
typedef struct tw_device_config {
    const char           *name;                  /**< Short device name exposed to users and hub (UTF-8). */
    const char           *fw_version;            /**< Firmware version string reported in status and OTA. */

    /** Compile-time identity defaults; may be superseded by NVS or factory provisioning. */
    int32_t               vendor_id;             /**< OEM/vendor identifier. */
    int32_t               product_id;            /**< Product identifier within the vendor namespace. */
    const char           *vendor_display_name;   /**< Human-readable vendor name for UI and provisioning. */
    const char           *product_display_name;   /**< Human-readable product name for UI and provisioning. */
    uint8_t               variant;               /**< Hardware or SKU variant discriminator. */

    /** Application REST-style resource table; must end with ::TW_MSG_RESOURCE_END. */
    tw_msg_resource_t    *resources;

    /** Optional lifecycle: called once after PAL and services are up. */
    tw_err_t (*on_init)(const struct tw_device_config *dev);
    /** Optional lifecycle: called every @a tick_interval_ms in the main loop. */
    tw_err_t (*on_tick)(const struct tw_device_config *dev);
    /** Optional lifecycle: device is leaving sleep (platform-specific). */
    tw_err_t (*on_wake)(const struct tw_device_config *dev);
    /** Optional lifecycle: device is entering sleep (platform-specific). */
    tw_err_t (*on_sleep)(const struct tw_device_config *dev);

    /**
     * Optional: hub-pushed command callback.
     * Called for /tw/app and as a notification for other commands.
     * @param dev     Device configuration.
     * @param command Command name (e.g. "app", "set-config", "ota-available").
     * @param payload Raw payload bytes (protobuf-encoded if TW_USE_PROTOBUF).
     * @param payload_len Length of payload in bytes.
     */
    tw_err_t (*on_command)(const struct tw_device_config *dev,
                           const char *command,
                           const uint8_t *payload, size_t payload_len);
    /** Optional: extra heartbeat payload builder; NULL skips custom payload. */
    tw_heartbeat_fn_t heartbeat_payload;

    /** Optional: replace default BLE provisioning; NULL uses SDK built-in flow. */
    tw_err_t (*on_provision)(const struct tw_device_config *dev);

    uint32_t tick_interval_ms;                   /**< Period between ::on_tick invocations when applicable. */

    void *user_data;                             /**< Opaque pointer for application state; never read by the SDK. */
} tw_device_config_t;

/* ---------------------------------------------------------------------------
 * SDK main loop
 * -------------------------------------------------------------------------*/

/**
 * @brief Initializes the platform abstraction, transport, protocol server, sends boot heartbeat, then runs the device loop.
 *
 * In deep-sleep oriented builds, typically performs one sensor/heartbeat cycle,
 * opens a listen window, then sleeps. In always-on builds, loops until
 * ::tw_device_request_shutdown is called.
 *
 * @param cfg Non-NULL configuration; must remain valid for the duration of the call.
 * @retval TW_OK when the loop exits cleanly after shutdown was requested.
 * @retval Other ::tw_err_t codes on fatal initialization or unrecoverable errors.
 */
tw_err_t tw_device_run(const tw_device_config_t *cfg);

/**
 * @brief Requests a graceful exit from ::tw_device_run (idempotent).
 *
 * After the current iteration, the SDK tears down the protocol, flushes non-volatile
 * storage as needed, and returns from ::tw_device_run. Safe from any thread or
 * from message and hub callbacks.
 */
void tw_device_request_shutdown(void);

/* ---------------------------------------------------------------------------
 * Platform entry-point macro
 * -------------------------------------------------------------------------*/

#ifdef TW_PLATFORM_ESPIDF
  /** @brief Defines `app_main` for ESP-IDF that calls ::tw_device_run with @a cfg (static or global). */
  #define TW_DEVICE_MAIN(cfg)                          \
      void app_main(void) { tw_device_run(cfg); }
#else
  /** @brief Defines standard `main` that returns 0 on ::TW_OK and 1 otherwise. */
  #define TW_DEVICE_MAIN(cfg)                          \
      int main(int argc, char **argv) {                \
          (void)argc; (void)argv;                      \
          return tw_device_run(cfg) == TW_OK ? 0 : 1;  \
      }
#endif

#ifdef __cplusplus
}
#endif

#endif /* TW_DEVICE_H */
