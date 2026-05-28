/*
 * pal_gpio.h -- GPIO abstraction.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef PAL_GPIO_H
#define PAL_GPIO_H

#include "tw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Digital I/O direction and pull configuration for a GPIO pin. */
typedef enum {
    PAL_GPIO_INPUT,           /**< Input, no pull resistor (floating unless hardware defaults apply). */
    PAL_GPIO_OUTPUT,          /**< Push-pull output. */
    PAL_GPIO_INPUT_PULLUP,    /**< Input with internal pull-up enabled. */
    PAL_GPIO_INPUT_PULLDOWN,  /**< Input with internal pull-down enabled. */
} pal_gpio_mode_t;

/** @brief Edge(s) that trigger a GPIO interrupt. */
typedef enum {
    PAL_GPIO_EDGE_RISING,   /**< Rising edge (low to high). */
    PAL_GPIO_EDGE_FALLING,  /**< Falling edge (high to low). */
    PAL_GPIO_EDGE_BOTH,     /**< Both rising and falling edges. */
} pal_gpio_edge_t;

/**
 * @brief GPIO interrupt service routine callback.
 *
 * Implementations typically invoke this from interrupt context. The handler must be
 * non-blocking and must not call PAL routines that sleep, lock mutexes for long periods,
 * or assume it runs on a normal task stack unless the platform documents otherwise.
 *
 * @param pin  Platform GPIO index passed to pal_gpio_set_interrupt.
 * @param ctx  User context pointer passed to pal_gpio_set_interrupt.
 */
typedef void (*pal_gpio_isr_t)(int pin, void *ctx);

/**
 * @brief Configure a GPIO pin for the given mode.
 *
 * Backend contract: Configure the hardware pin identified by @p pin for @p mode. If the pin
 * was previously used (including as interrupt), leave it in a defined state suitable for @p mode.
 * Safe to call again to change mode on the same pin.
 *
 * Thread-safety: Must be called from task context unless the implementation documents ISR use.
 *
 * @param pin  Platform-specific GPIO index (non-negative).
 * @param mode Desired digital mode and pull configuration.
 * @retval TW_OK          Pin configured successfully.
 * @retval TW_ERR_INVAL    Invalid @p pin or unsupported @p mode.
 * @retval TW_ERR_IO       Hardware or driver error.
 */
tw_err_t pal_gpio_init(int pin, pal_gpio_mode_t mode);

/**
 * @brief Drive an output GPIO to a logical level.
 *
 * Backend contract: Set the output level for a pin previously initialized as output (or
 * implementation-defined behavior if not). Level @c true is logic high, @c false is logic low.
 *
 * Thread-safety: Implementation-defined if concurrent with ISR on the same pin; prefer
 * external synchronization or use implementation docs.
 *
 * @param pin   GPIO index.
 * @param level @c true for high, @c false for low.
 * @retval TW_OK       Level applied.
 * @retval TW_ERR_INVAL Pin is not an output or invalid @p pin.
 * @retval TW_ERR_IO    Hardware error.
 */
tw_err_t pal_gpio_write(int pin, bool level);

/**
 * @brief Sample the current logical level of a GPIO.
 *
 * Backend contract: Return the instantaneous digital level for @p pin configured as input
 * (or output read-back if supported).
 *
 * Thread-safety: Safe from multiple tasks if the implementation documents it; ISR may race
 * without synchronization.
 *
 * @param pin GPIO index.
 * @return @c true if the pin reads high, @c false if low.
 */
bool     pal_gpio_read(int pin);

/**
 * @brief Enable or change a GPIO interrupt on the given edge(s).
 *
 * Backend contract: Install @p handler (and @p ctx) for @p pin. When the selected @p edge
 * occurs, call @p handler from interrupt context. If interrupts were already enabled on this
 * pin, replace the previous handler. Passing a NULL @p handler may disable the interrupt if
 * the implementation supports it; otherwise document required usage.
 *
 * Thread-safety: Must be called from task context. Do not call from @p handler unless documented.
 *
 * @param pin     GPIO index (input-capable pin).
 * @param edge    Which edge(s) fire the interrupt.
 * @param handler Callback invoked from ISR, or NULL to disable if supported.
 * @param ctx     Opaque pointer passed to @p handler.
 * @retval TW_OK       Interrupt configured.
 * @retval TW_ERR_INVAL Invalid @p pin, @p edge, or unsupported combination.
 * @retval TW_ERR_IO    Hardware or driver error.
 */
tw_err_t pal_gpio_set_interrupt(int pin, pal_gpio_edge_t edge,
                                pal_gpio_isr_t handler, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* PAL_GPIO_H */
