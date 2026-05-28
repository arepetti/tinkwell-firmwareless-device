# Thermostat Example

A reference implementation of a thermostat controlled by an external Tinkwell edge hub, built on the [TW Device SDK](../../tw-device-sdk/).

## What it does

- Reads temperature and humidity sensors
- Three modes: **OFF**, **ON**, **AUTO** (cycled by a physical button)
  - OFF: relay off (freeze protection can override)
  - ON: relay on (overheat protection can override)
  - AUTO: relay controlled by the hub via CoAP
- Safety overrides are always active regardless of mode
- Two LEDs: mode indicator (off/solid/blink) and relay state
- Exposes data via CoAP for the hub to read and control

## CoAP resources

| Method | Path | Description |
|--------|------|-------------|
| GET | `/tw/sensor/temperature` | Current temperature (tenths of °C) |
| GET | `/tw/sensor/humidity` | Current humidity (tenths of %RH) |
| GET | `/tw/mode` | Current mode: "off", "on", "auto" |
| PUT | `/tw/mode` | Set mode (hub can only set "auto") |
| GET | `/tw/relay` | Relay state: 0 or 1 |
| PUT | `/tw/relay` | Set relay (only effective in AUTO mode) |
| GET | `/tw/status` | Summary: mode, relay, temp, safety |

## Building (native)

```bash
cmake -B build -DPAL_BACKEND=posix
cmake --build build
./build/thermostat
```

## Building (ESP-IDF)

```bash
idf.py set-target esp32c6
idf.py build
idf.py flash monitor
```

## Simulating sensor values

On the native build, set environment variables:

```bash
TW_FAKE_TEMP=215 TW_FAKE_HUMID=450 ./build/thermostat
```

Values are in tenths (215 = 21.5°C, 450 = 45.0%RH).

## Safety thresholds

Configurable via Kconfig (or CMake defines on POSIX):

| Threshold | Default | Effect |
|-----------|---------|--------|
| `CONFIG_SAFETY_FREEZE_TEMP_C` | 30 (3.0°C) | Forces relay ON |
| `CONFIG_SAFETY_MAX_TEMP_C` | 400 (40.0°C) | Forces relay OFF |

## Files

| File | Purpose |
|------|---------|
| `host/main.c` | Entry point -- fills `tw_device_config_t` |
| `app/src/thermostat.c` | State machine, safety, CoAP handlers |
| `app/include/thermostat.h` | Public API |
| `app/include/thermostat_pins.h` | GPIO pin assignments |

The entire application is ~200 lines of C.  Everything else is handled by the SDK.

## Documentation

- [Walkthrough](docs/walkthrough.md) -- step-by-step code explanation
- [Hardware](docs/hardware.md) -- wiring, pin assignments, sensor datasheets
- [Testing](docs/testing.md) -- unit and integration tests
- [Quick start](../../docs/getting-started/quick-start.md) -- build and run in 5 minutes
- [Build your own device](../../docs/getting-started/build-your-device.md) -- turn this into your project
