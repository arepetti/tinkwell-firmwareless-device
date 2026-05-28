/*
 * blinky.c -- Blinky applet in freestanding C.
 *
 * Compiled with:
 *   clang --target=wasm32 -nostdlib -Wl,--no-entry \
 *         -Wl,--export=applet_init \
 *         -Wl,--export=applet_tick \
 *         -Wl,--export=applet_on_command \
 *         -o blinky.wasm blinky.c
 *
 * No WASI, no SDK, no external dependencies.  Just a C compiler.
 *
 * SPDX-License-Identifier: MIT
 */

/* Host function imports (provided by the device runtime). */
extern void tw_host_write_gpio(int pin, int value);
extern void tw_host_log_s(const char *msg, unsigned int msg_len, int level);

#define PIN_LED 1

static int state = 0;

void applet_init(void)
{
    tw_host_log_s("blinky-c: init", 14, 2);
}

void applet_tick(void)
{
    state = !state;
    tw_host_write_gpio(PIN_LED, state);
}

void applet_on_command(int cmd_type, const void *data, int len)
{
    (void)cmd_type;
    (void)data;
    (void)len;
}
