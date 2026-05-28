/*
 * tw_ota.h -- OTA status query (for application-level status reporting).
 *
 * The OTA push receiver itself is SDK-internal; this header exposes
 * read-only status so the application can include it in /tw/status.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TW_OTA_H
#define TW_OTA_H

#include "tw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief High-level state of the over-the-air firmware update pipeline. */
typedef enum {
    TW_OTA_IDLE,             /**< No transfer or verification in progress. */
    TW_OTA_RECEIVING,        /**< Image bytes are being written to staging storage. */
    TW_OTA_VERIFYING,        /**< Cryptographic or image checks before apply. */
    TW_OTA_REBOOTING,        /**< About to or in the process of rebooting into new image. */
    TW_OTA_PENDING_VERIFY,   /**< New firmware booted; confirmation not yet acknowledged to hub. */
    TW_OTA_ERROR,            /**< Last OTA attempt failed; see logs or hub for detail. */
} tw_ota_state_t;

/**
 * @brief Returns the current OTA pipeline state for status reporting.
 * @return One of ::tw_ota_state_t.
 */
tw_ota_state_t tw_ota_state(void);

/**
 * @brief Approximate completion percentage for an in-progress transfer (0--100).
 *
 * Meaningful primarily while ::TW_OTA_RECEIVING; other states may return 0 or a stale value
 * depending on implementation.
 *
 * @return Progress percentage from 0 through 100.
 */
uint32_t       tw_ota_progress_pct(void);

#ifdef __cplusplus
}
#endif

#endif /* TW_OTA_H */
