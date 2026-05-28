/*
 * pal_i2c_posix.c -- Fake I2C: returns configurable sensor data.
 *
 * The environment variable TW_FAKE_TEMP (tenths of C) and TW_FAKE_HUMID
 * (tenths of %RH) control the values returned by reads to the default
 * sensor addresses.
 *
 * SPDX-License-Identifier: MIT
 */

#include "pal_i2c.h"
#include "pal_log.h"
#include <stdlib.h>
#include <string.h>

#define TAG "i2c"

/* Default fake sensor address (SHT30-like). */
#define FAKE_SENSOR_ADDR 0x44

tw_err_t pal_i2c_init(const pal_i2c_config_t *cfg)
{
    PAL_LOGI(TAG, "bus %d initialised (simulated, sda=%d scl=%d)",
             cfg->bus, cfg->sda_pin, cfg->scl_pin);
    return TW_OK;
}

tw_err_t pal_i2c_read(int bus, uint8_t addr, uint8_t reg,
                      uint8_t *buf, size_t len)
{
    TW_UNUSED(bus);
    TW_UNUSED(reg);

    if (addr == FAKE_SENSOR_ADDR && len >= 2) {
        const char *env = getenv("TW_FAKE_TEMP");
        int16_t val = env ? (int16_t)atoi(env) : 215; /* 21.5 C */
        buf[0] = (uint8_t)(val >> 8);
        buf[1] = (uint8_t)(val & 0xFF);
        return TW_OK;
    }

    memset(buf, 0, len);
    return TW_OK;
}

tw_err_t pal_i2c_write(int bus, uint8_t addr, uint8_t reg,
                       const uint8_t *buf, size_t len)
{
    TW_UNUSED(bus); TW_UNUSED(addr); TW_UNUSED(reg);
    TW_UNUSED(buf); TW_UNUSED(len);
    return TW_OK;
}

void pal_i2c_deinit(int bus)
{
    TW_UNUSED(bus);
}
