# Code Walkthrough

This document explains every source file in the thermostat example, line by line.

## `host/main.c` -- Entry point

The entire file is ~30 lines.
It has three jobs:

1. **Define the CoAP resource table** -- an array of `tw_msg_resource_t` structs mapping URI paths to handler functions.

2. **Fill in `tw_device_config_t`** -- the device name, firmware version, resource table, lifecycle callbacks, and tick interval.

3. **Call `TW_DEVICE_MAIN`** -- a macro that generates the correct `main()` (or `app_main()` on ESP-IDF) and calls `tw_device_run()`.

That's it.
The SDK takes over from there.

## `app/include/thermostat.h` -- Public API

Declares:

- `thermostat_mode_t` enum: `MODE_OFF`, `MODE_ON`, `MODE_AUTO`
- `thermostat_init()` and `thermostat_tick()` -- wired as SDK callbacks
- Five CoAP handler functions (one per resource)

## `app/include/thermostat_pins.h` -- Pin assignments

Four `#define`s for the GPIO pins used by this board:

- `PIN_BUTTON` (0) -- mode cycle button
- `PIN_LED_MODE` (1) -- mode indicator LED
- `PIN_LED_RELAY` (2) -- relay state LED
- `PIN_RELAY` (3) -- actual relay output

On real hardware these match the PCB layout.
On POSIX they're simulated in memory.

## `app/src/thermostat.c` -- The heart of it

### State

Five static variables hold the entire device state:

- `mode` -- current operating mode
- `relay_state` -- whether the relay is energised
- `relay_requested` -- what the hub asked for (AUTO mode only)
- `button`, `led_mode`, `led_relay` -- SDK utility handles
- `safety` -- safety monitor handle

### `thermostat_init()`

1. Restores the mode from NVS (survives reboots)
2. Creates the button with a debounce callback that cycles modes
3. Creates two LEDs
4. Configures the relay GPIO
5. Creates a safety monitor with freeze/overheat thresholds
6. Registers temperature and humidity sensor drivers

### `thermostat_tick()`

Called every 1000 ms by the SDK.
The flow:

1. Poll the button for presses
2. Read the temperature sensor
3. Run the safety check (may force-override the relay)
4. If safety is not overriding, apply the mode logic:
   - OFF → relay off
   - ON → relay on
   - AUTO → relay matches `relay_requested`
5. Write the relay GPIO
6. Update LED patterns: OFF=off, ON=solid, AUTO=blink
7. Tick the LED drivers (for blink timing)

### CoAP handlers

Each handler is a simple function taking a request and response:

- `on_get_temperature` -- returns `tw_sensor_read_int("temperature")`
- `on_get_humidity` -- returns `tw_sensor_read_int("humidity")`
- `on_mode` -- GET returns the mode name, PUT only accepts "auto"
- `on_relay` -- GET returns 0/1, PUT sets `relay_requested` (AUTO only)
- `on_get_status` -- returns a one-line summary string

## What you don't see

The following are handled entirely by the SDK with zero application code:

- Network initialisation (WiFi, Ethernet, or Thread)
- CoAP server setup, UDP socket management
- OTA firmware updates (auto-registered at `/tw/ota/*`)
- Hub heartbeat and mailbox polling
- Deep sleep orchestration
- BLE provisioning
- NVS initialisation
- Logging infrastructure
