/*
 * pal_gpio_esp.c -- GPIO via ESP-IDF driver.
 *
 * SPDX-License-Identifier: MIT
 */

#include "pal_gpio.h"
#include "driver/gpio.h"

tw_err_t pal_gpio_init(int pin, pal_gpio_mode_t mode)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << pin),
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    switch (mode) {
    case PAL_GPIO_INPUT:
        cfg.mode = GPIO_MODE_INPUT;
        break;
    case PAL_GPIO_OUTPUT:
        cfg.mode = GPIO_MODE_OUTPUT;
        break;
    case PAL_GPIO_INPUT_PULLUP:
        cfg.mode = GPIO_MODE_INPUT;
        cfg.pull_up_en = GPIO_PULLUP_ENABLE;
        break;
    case PAL_GPIO_INPUT_PULLDOWN:
        cfg.mode = GPIO_MODE_INPUT;
        cfg.pull_down_en = GPIO_PULLDOWN_ENABLE;
        break;
    }

    return gpio_config(&cfg) == ESP_OK ? TW_OK : TW_ERR_IO;
}

tw_err_t pal_gpio_write(int pin, bool level)
{
    return gpio_set_level((gpio_num_t)pin, level ? 1 : 0) == ESP_OK
               ? TW_OK : TW_ERR_IO;
}

bool pal_gpio_read(int pin)
{
    return gpio_get_level((gpio_num_t)pin) != 0;
}

static pal_gpio_isr_t isr_handlers[GPIO_NUM_MAX];
static void          *isr_contexts[GPIO_NUM_MAX];

static void IRAM_ATTR gpio_isr_dispatch(void *arg)
{
    int pin = (int)(intptr_t)arg;
    if (isr_handlers[pin])
        isr_handlers[pin](pin, isr_contexts[pin]);
}

tw_err_t pal_gpio_set_interrupt(int pin, pal_gpio_edge_t edge,
                                pal_gpio_isr_t handler, void *ctx)
{
    static bool service_installed = false;
    if (!service_installed) {
        gpio_install_isr_service(0);
        service_installed = true;
    }

    gpio_int_type_t type;
    switch (edge) {
    case PAL_GPIO_EDGE_RISING:  type = GPIO_INTR_POSEDGE; break;
    case PAL_GPIO_EDGE_FALLING: type = GPIO_INTR_NEGEDGE; break;
    case PAL_GPIO_EDGE_BOTH:    type = GPIO_INTR_ANYEDGE; break;
    default: return TW_ERR_INVAL;
    }

    isr_handlers[pin] = handler;
    isr_contexts[pin] = ctx;

    gpio_set_intr_type((gpio_num_t)pin, type);
    gpio_isr_handler_add((gpio_num_t)pin, gpio_isr_dispatch,
                         (void *)(intptr_t)pin);
    return TW_OK;
}
