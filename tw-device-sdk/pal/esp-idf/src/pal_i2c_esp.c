/*
 * pal_i2c_esp.c -- I2C via ESP-IDF driver (new driver API).
 *
 * SPDX-License-Identifier: MIT
 */

#include "pal_i2c.h"
#include "pal_log.h"
#include "driver/i2c.h"

#define TAG          "i2c"
#define ACK_CHECK_EN 0x1
#define TIMEOUT_MS   100

tw_err_t pal_i2c_init(const pal_i2c_config_t *cfg)
{
    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = cfg->sda_pin,
        .scl_io_num       = cfg->scl_pin,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = cfg->freq_hz,
    };

    esp_err_t err = i2c_param_config((i2c_port_t)cfg->bus, &conf);
    if (err != ESP_OK) return TW_ERR_IO;

    err = i2c_driver_install((i2c_port_t)cfg->bus, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return TW_ERR_IO;

    PAL_LOGI(TAG, "bus %d: sda=%d scl=%d freq=%lu",
             cfg->bus, cfg->sda_pin, cfg->scl_pin,
             (unsigned long)cfg->freq_hz);
    return TW_OK;
}

tw_err_t pal_i2c_read(int bus, uint8_t addr, uint8_t reg,
                      uint8_t *buf, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
    if (len > 1)
        i2c_master_read(cmd, buf, len - 1, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, buf + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin((i2c_port_t)bus, cmd,
                                          pdMS_TO_TICKS(TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return err == ESP_OK ? TW_OK : TW_ERR_IO;
}

tw_err_t pal_i2c_write(int bus, uint8_t addr, uint8_t reg,
                       const uint8_t *buf, size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_write(cmd, buf, len, ACK_CHECK_EN);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin((i2c_port_t)bus, cmd,
                                          pdMS_TO_TICKS(TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return err == ESP_OK ? TW_OK : TW_ERR_IO;
}

void pal_i2c_deinit(int bus)
{
    i2c_driver_delete((i2c_port_t)bus);
}
