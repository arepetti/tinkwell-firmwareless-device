# Quick Start

This guide gets the thermostat example running on your machine in under five minutes.
No hardware needed.

## Prerequisites

- GCC or Clang (any recent version with C11 support)
- CMake 3.16+
- Linux, macOS, or WSL
- Internet connection (first build fetches libcoap via CMake FetchContent)
- `tw` CLI (Tinkwell CLI) for CoAP interactions (provisioning, testing)

## 1. Clone

```bash
git clone --recursive https://github.com/arepetti/tinkwell-firmwareless-device.git
cd tinkwell-firmwareless-device/examples/thermostat
```

## 2. Build

```bash
cmake -B build -DPAL_BACKEND=posix
cmake --build build
```

## 3. Run

```bash
./build/thermostat
```

You should see:

```
I (0.000) [device] --- TW Device SDK ---
I (0.000) [device] device : thermostat
I (0.000) [device] fw     : 0.1.0
I (0.000) [device] chip   : posix-host
I (0.001) [nvs] NVS loaded 0 keys from /home/you/.tw-device/nvs.dat
I (0.001) [net] POSIX networking ready
I (0.001) [coap] CoAP server listening on :5683
I (0.001) [thermo] restored mode=2 from NVS
I (0.001) [thermo] thermostat initialised
I (0.001) [device] entering main loop (tick every 1000 ms)
```

## 4. Query it

In another terminal:

```bash
# Read temperature (tenths of C, default 215 = 21.5°C)
tw coap send get /tw/sensor/temperature

# Read mode
tw coap send get /tw/mode

# Read status
tw coap send get /tw/status

# Set relay ON (only works in AUTO mode)
tw coap send put /tw/relay -d "1"
```

For provisioning flows against a running device, see [`provision.py`](../../scripts/provision.py).

Protocol details (paths, protobuf, hub mailbox) are in the [wire specification](../protocol/wire-specification.md).

## 5. Simulate different temperatures

```bash
# Trigger freeze protection (3.0°C threshold)
TW_FAKE_TEMP=20 ./build/thermostat

# Trigger overheat protection (40.0°C threshold)
TW_FAKE_TEMP=410 ./build/thermostat
```

---

## Building for ESP32 (real hardware)

For full ESP-IDF instructions (menuconfig, partitions, factory images), see the [ESP-IDF guide](../guides/esp-idf.md).
The short version:

```bash
cd examples/thermostat/esp-idf
idf.py set-target esp32c6
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

---

## What about applets?

If you prefer not to write C, see [Your first applet](your-first-applet.md) -- push WASM logic from any language without touching the firmware.

## Next steps

- [Build your own device](build-your-device.md) -- turn this into your own project
- [Architecture overview](../architecture/overview.md) -- understand the layer design
- [Native development](../guides/native-development.md) -- POSIX workflow details
- [QEMU testing](../guides/qemu-testing.md) -- run on emulated ESP32
- [Choosing your approach](../guides/choosing-your-approach.md) -- applets vs native C vs state machines
