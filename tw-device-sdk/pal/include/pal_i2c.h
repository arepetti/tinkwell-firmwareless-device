/*
 * pal_i2c.h -- I2C bus abstraction.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef PAL_I2C_H
#define PAL_I2C_H

#include "tw_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief I2C controller configuration for pal_i2c_init.
 *
 * Backend contract: @p bus identifies a logical bus instance. @p sda_pin and @p scl_pin are
 * platform GPIO numbers for the data and clock lines. @p freq_hz is the target clock frequency
 * in hertz (standard-mode / fast-mode per hardware capability).
 */
typedef struct {
    int bus;          /**< Logical bus index (platform-specific, non-negative). */
    int sda_pin;      /**< GPIO index for SDA. */
    int scl_pin;      /**< GPIO index for SCL. */
    uint32_t freq_hz; /**< Target I2C clock frequency in Hz. */
} pal_i2c_config_t;

/**
 * @brief Initialize an I2C bus from configuration.
 *
 * Backend contract: Acquire the bus @p cfg->bus, configure pins and clock, and leave the bus
 * in a state ready for pal_i2c_read / pal_i2c_write. Repeated init for the same bus may
 * reconfigure or return @c TW_ERR_BUSY per implementation.
 *
 * Thread-safety: Call once per bus during startup from the main task or a dedicated init path
 * unless the implementation documents otherwise.
 *
 * @param cfg  Non-NULL pointer to bus configuration.
 * @retval TW_OK       Bus initialized.
 * @retval TW_ERR_INVAL Invalid @p cfg (pins, bus id, or frequency).
 * @retval TW_ERR_IO    Peripheral or GPIO setup failed.
 * @retval TW_ERR_BUSY  Bus already owned.
 */
tw_err_t pal_i2c_init(const pal_i2c_config_t *cfg);

/**
 * @brief Read registers from an I2C peripheral (write-then-read / combined transaction).
 *
 * Backend contract: Address the 7-bit device @p addr (implementation-defined whether @p addr
 * includes the R/W bit in the low bit; callers must match platform convention). Write @p reg
 * as the first byte(s) per typical register addressing, then clock in @p len bytes into @p buf.
 * For single-byte register addresses, one byte is written before the read phase.
 *
 * Thread-safety: Not safe concurrent use of the same @p bus from multiple tasks without
 * external locking unless the implementation serializes internally.
 *
 * @param bus  Logical bus index matching pal_i2c_init.
 * @param addr 7-bit I2C slave address (platform convention).
 * @param reg  Register address byte written before the read.
 * @param buf  Destination buffer for read data (must hold @p len bytes).
 * @param len  Number of bytes to read.
 * @retval TW_OK          Data read into @p buf.
 * @retval TW_ERR_INVAL    Invalid arguments.
 * @retval TW_ERR_IO       NACK, bus error, or hardware fault.
 * @retval TW_ERR_TIMEOUT  Slave did not respond in time.
 */
tw_err_t pal_i2c_read(int bus, uint8_t addr, uint8_t reg,
                      uint8_t *buf, size_t len);

/**
 * @brief Write a register block to an I2C peripheral.
 *
 * Backend contract: Address @p addr, then transmit @p reg followed by @p len bytes from @p buf
 * in a single write transaction (or equivalent segmented writes if the platform requires).
 *
 * Thread-safety: Same as pal_i2c_read for concurrent access to @p bus.
 *
 * @param bus  Logical bus index matching pal_i2c_init.
 * @param addr 7-bit I2C slave address (platform convention).
 * @param reg  Register address byte sent first.
 * @param buf  Data to write after the register byte (may be NULL if @p len is 0).
 * @param len  Number of payload bytes after @p reg.
 * @retval TW_OK          Write completed.
 * @retval TW_ERR_INVAL    Invalid arguments.
 * @retval TW_ERR_IO       NACK, bus error, or hardware fault.
 * @retval TW_ERR_TIMEOUT  Slave did not respond in time.
 */
tw_err_t pal_i2c_write(int bus, uint8_t addr, uint8_t reg,
                       const uint8_t *buf, size_t len);

/**
 * @brief Release an I2C bus and associated resources.
 *
 * Backend contract: Disable the controller for @p bus, release GPIOs if applicable, and allow
 * pal_i2c_init to run again. In-flight transactions must be completed or aborted safely.
 *
 * Thread-safety: Do not call concurrently with read/write on the same bus.
 *
 * @param bus Logical bus index to shut down.
 */
void     pal_i2c_deinit(int bus);

#ifdef __cplusplus
}
#endif

#endif /* PAL_I2C_H */
