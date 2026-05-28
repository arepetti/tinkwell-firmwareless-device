/*
 * mock_pal_i2c.c
 * SPDX-License-Identifier: MIT
 */

#include "pal_i2c.h"
#include "mock_pal.h"
#include <string.h>

tw_err_t pal_i2c_init(const pal_i2c_config_t *cfg)
{
    mock_record("pal_i2c_init", NULL);
    (void)cfg;
    return TW_OK;
}

tw_err_t pal_i2c_read(int bus, uint8_t addr, uint8_t reg,
                      uint8_t *buf, size_t len)
{
    mock_record("pal_i2c_read", NULL);
    (void)bus; (void)addr;

    if (len >= 2) {
        int16_t val = (reg == 0x00)
            ? (int16_t)g_mock_cfg.i2c_read_temp
            : (int16_t)g_mock_cfg.i2c_read_humid;
        buf[0] = (uint8_t)(val >> 8);
        buf[1] = (uint8_t)(val & 0xFF);
    }
    return TW_OK;
}

tw_err_t pal_i2c_write(int bus, uint8_t addr, uint8_t reg,
                       const uint8_t *buf, size_t len)
{
    mock_record("pal_i2c_write", NULL);
    (void)bus; (void)addr; (void)reg; (void)buf; (void)len;
    return TW_OK;
}

void pal_i2c_deinit(int bus)
{
    mock_record("pal_i2c_deinit", NULL);
    (void)bus;
}
