# Documentation

## Start here

| Document | Description |
|----------|-------------|
| [Quick start](getting-started/quick-start.md) | Build and run the thermostat example in 5 minutes (native C path) |
| [Your first applet](getting-started/your-first-applet.md) | Push a WASM applet to a device with zero C code |
| [Choosing your approach](guides/choosing-your-approach.md) | WASM applets vs compiled state machines vs native C -- trade-offs and decision matrix |

## Build something

| Document | Description |
|----------|-------------|
| [Build your own device](getting-started/build-your-device.md) | Turn the thermostat skeleton into your custom project |
| [Writing applets](guides/writing-applets.md) | Applet contract, language guides (TypeScript, Rust, C, WAT), testing locally |
| [Native development](guides/native-development.md) | POSIX build workflow, debugging, simulating sensors |
| [ESP-IDF](guides/esp-idf.md) | Build, flash, and monitor on ESP32 hardware |

## Go deeper

| Document | Description |
|----------|-------------|
| [Provisioning](guides/provisioning.md) | BLE GATT, SoftAP + CoAP, LAN CoAP -- multi-method device setup |
| [OTA updates](guides/ota-updates.md) | Push firmware from the hub with SHA-256 verification |
| [Power management](guides/power-management.md) | Always-on vs deep sleep, wake intervals, listen windows |
| [QEMU testing](guides/qemu-testing.md) | Run firmware in QEMU without hardware |
| [Porting to other boards](guides/porting.md) | ARM Cortex-M, Zephyr, and other targets |
| [Troubleshooting](guides/troubleshooting.md) | Common issues and how to fix them |

## Reference

| Document | Description |
|----------|-------------|
| [C API reference](reference/api.md) | Public SDK headers (`tw_device.h`, `tw_msg.h`, etc.) |
| [Host API reference](reference/host-api.md) | WASM host functions available to applets |
| [PAL reference](reference/pal.md) | Platform Abstraction Layer interface and porting guide |
| [Kconfig reference](reference/kconfig.md) | All compile-time configuration options |
| [Scripts reference](../scripts/README.md) | Provisioning, demo, build/flash, QEMU, fake hub/sensor |

## Protocol specifications

| Document | Description |
|----------|-------------|
| [Wire specification](protocol/wire-specification.md) | Canonical on-the-wire protocol reference (CoAP, protobuf, kvtext) |
| [Hub protocol](protocol/hub-protocol.md) | Heartbeat, mailbox, command dispatch |
| [Applet protocol](protocol/applet-protocol.md) | WASM applet push, commit, hot-swap |

## Architecture

| Document | Description |
|----------|-------------|
| [Architecture overview](architecture/overview.md) | Layered design, data flow, security model, thread safety |
| [Applet runtime](architecture/applet-runtime.md) | WAMR integration, flash storage, lifecycle, hot-swap |
