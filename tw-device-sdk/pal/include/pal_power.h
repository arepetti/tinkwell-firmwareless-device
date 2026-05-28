/*
 * pal_power.h -- Sleep and wake-source abstraction.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef PAL_POWER_H
#define PAL_POWER_H

#include "tw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Reason the platform resumed from a low-power or deep-sleep state. */
typedef enum {
    PAL_WAKE_TIMER,    /**< Wake due to internal timer or RTC alarm. */
    PAL_WAKE_GPIO,     /**< Wake due to a configured GPIO level or edge. */
    PAL_WAKE_UNKNOWN,  /**< Cause not available or not classified. */
} pal_wake_reason_t;

/**
 * @brief Enter deep sleep for approximately the given duration.
 *
 * Backend contract: Configure the lowest practical sleep mode for ~@p duration_ms, set a wake
 * timer if supported, and suspend until wake. Actual sleep granularity depends on the RTC or
 * low-speed clock. Successful entry may end in reset or resume without a normal return (see @note).
 *
 * Thread-safety: Must be called from a task context with network and peripherals quiesced per
 * platform requirements; not ISR-safe.
 *
 * @param duration_ms Target sleep duration in milliseconds (implementation may quantize).
 * @retval TW_OK       Sleep entered (caller may not return after this; see note).
 * @retval TW_ERR_INVAL Duration not supported.
 * @retval TW_ERR_IO    Hardware cannot enter sleep.
 *
 * @note On success the CPU often resets or resumes without returning; error returns apply when
 *       sleep cannot be entered. Backend authors should document actual control flow.
 */
tw_err_t          pal_power_deep_sleep(uint32_t duration_ms);

/**
 * @brief Query why the last wake from deep sleep occurred.
 *
 * Backend contract: After reset/resume from deep sleep, return the classified wake source.
 * After cold boot, may return @c PAL_WAKE_UNKNOWN.
 *
 * Thread-safety: Safe from main task during early boot; document ISR use for the port.
 *
 * @return Classified wake reason.
 */
pal_wake_reason_t pal_power_wake_reason(void);

/**
 * @brief Configure a GPIO pin as a wake source (level-sensitive typically).
 *
 * Backend contract: Before deep sleep, enable wake when @p pin reads as @p level (true = high).
 * May replace any previous GPIO wake configuration. Exact electrical behavior (pulls, debounce)
 * is platform-specific.
 *
 * Thread-safety: Call from task context before pal_power_deep_sleep, not from ISR unless documented.
 *
 * @param pin   GPIO index valid for wake on this SoC.
 * @param level @c true to wake on high level, @c false on low.
 * @retval TW_OK       Wake source configured.
 * @retval TW_ERR_INVAL Pin cannot be used as wake source.
 * @retval TW_ERR_IO    Configuration failed.
 */
tw_err_t          pal_power_set_wake_gpio(int pin, bool level);

#ifdef __cplusplus
}
#endif

#endif /* PAL_POWER_H */
