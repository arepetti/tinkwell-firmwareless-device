# tw-device-sdk

The **TW Device SDK** (`tw-device-sdk`) is a portable C11 library for constrained IoT MCUs: provisioning, hub connectivity, telemetry, OTA, command dispatch, heartbeat/mailbox handling, optional deep sleep, and an optional WebAssembly (WAMR) applet runtime.
It ships a **binary CoAP-over-UDP** stack (libcoap / ESP-IDF `coap` component), **nanopb**-backed payloads from `tw_protocol.proto`, and a **Platform Abstraction Layer (`pal_*`)** so services and protocol code stay free of vendor headers.
Embedded engineers integrating Tinkwell Firmwareless devices are the primary audience.
**Production targets today center on Espressif ESP32-family silicon via ESP-IDF** (see [ESP-IDF guide](../docs/guides/esp-idf.md)): ESP32-C6 (primary), ESP32-C3 and ESP32-H2 (tested), with ESP32-S3 noted as expected-compatible but untested.
**POSIX** (Linux, macOS, WSL) provides a desktop PAL for CI and bring-up **without hardware**.
**ESP-IDF QEMU** (ESP32-C3) exercises the RTOS/network path without a board ([QEMU testing](../docs/guides/qemu-testing.md)).
**Mock PAL** under `pal/mock/` is for **unit tests** only (injectable hooks, not a shipping target).

## Status and scope

This tree lives under `extras/firmwareless/device/tw-device-sdk/` inside the Tinkwell monorepo.

Documentation and examples are maintained alongside it under `extras/firmwareless/device/`.

The same layout is expected to match a future standalone clone (for example `tinkwell-firmwareless-device`), but **paths in this repository are authoritative here**.

The SDK is **actively developed**: interfaces are stabilizing, some protocol choices are still CoAP-first with MQTT reserved as a future backend (see [Architecture overview](../docs/architecture/overview.md)), and **ports beyond ESP-IDF + POSIX are documentation-first** until additional PAL backends land.

## At a glance

- **Provisioning** — SoftAP + CoAP, LAN CoAP, BLE-oriented flows (Kconfig-gated); see [Provisioning](../docs/guides/provisioning.md).
- **OTA** — Push-based firmware update with verification hooks on the PAL; see [OTA updates](../docs/guides/ota-updates.md).
- **Telemetry and sensors** — Registry and periodic paths integrated with the main loop; see [C API](../docs/reference/api.md).
- **Hub link** — Heartbeat and mailbox-style command delivery over the configured protocol; see [Hub protocol](../docs/protocol/hub-protocol.md).
- **Command dispatch** — Named commands and CoAP resources via `tw_cmd.h` and services; see [Wire specification](../docs/protocol/wire-specification.md).
- **Applet runtime** — WAMR-based WASM load from flash, push/commit from hub; see [Applet runtime](../docs/architecture/applet-runtime.md).
- **CoAP** — `proto_coap.c` implements the `tw_protocol_t` backend; Block1/Block2 for large payloads.
- **Transports (ESP-IDF)** — Wi-Fi, Ethernet, Thread, BLE (`transport_*.c`), plus **POSIX stub** transport for host builds.
- **PAL** — Single porting surface (`pal/include/pal_*.h`); **ESP-IDF** and **POSIX** implementations ship in-tree; **mock** supports tests.

## Architecture

**Application code** (native C resources, optional applet) calls the **public API** in `include/` and small **utilities** (`tw_button`, `tw_led`, `tw_safety`, …).

The **services** layer (`svc/src/svc_*.c`) orchestrates lifecycle, provisioning, identity, heartbeat, OTA, applets, power, telemetry, commands, sensors, and config.

Services talk to the hub through **`tw_protocol_t`** (see `tw_msg.h`): today **CoAP** is implemented in `protocol/src/proto_coap.c`; MQTT exists as a stub.

**Transport** selects the network path (`transport/src/transport_*.c`).

Everything below that goes through **PAL** — **no `#include` of `esp_*`, `freertos/`, or other vendor headers above PAL**.

```text
  Application  +  Utilities (tw_button, tw_led, tw_kvtext, …)
              |
           tw_device / tw_* headers (include/)
              |
           Services (svc/src/svc_*.c)
              |
           Protocol (protocol/src/proto_coap.c, …)
              |
           Transport (transport/src/)
              |
           PAL (pal/include/*.h → pal/esp-idf/, pal/posix/, pal/mock/)
```

For diagrams, threading, and security, read [Architecture overview](../docs/architecture/overview.md) and [Applet runtime](../docs/architecture/applet-runtime.md).

## Repository layout (this directory)

| Path | Role |
|------|------|
| `include/` | **Public headers** consumed by firmware and examples. |
| `svc/` | Built-in **services** (`svc/src/svc_device.c`, `svc_heartbeat.c`, `svc_ota.c`, `svc_provision.c`, `svc_identity.c`, `svc_applet.c`, `svc_sensor.c`, `svc_telemetry.c`, `svc_cmd.c`, `svc_config.c`, `svc_power.c`, `svc_reset_button.c`, …). |
| `protocol/` | Wire stack — **CoAP** implementation and protocol helpers (`protocol/src/proto_coap.c`, `proto_mqtt.c` stub). |
| `transport/` | **Pluggable transports** — POSIX stub; ESP-IDF: Wi‑Fi, Ethernet, Thread, BLE (`transport/src/transport_*.c`). |
| `pal/` | **Platform Abstraction Layer** — contracts in `pal/include/`; **ESP-IDF** and **POSIX** sources; **`pal/mock/`** for unit tests. |
| `util/` | Shared helpers (`tw_kvtext`, `tw_block`, `tw_safety`, `tw_button`, `tw_led`). |
| `proto/` | `tw_protocol.proto`, nanopb **`.options`**, generated sources under the build dir. |
| `test/` | **Unity** unit tests plus **pytest** integration tests; Mock PAL wired in CMake. |

Authoritative prose for firmwareless devices (getting started, guides, protocol, reference) is in **`../docs/`** — start from the [documentation index](../docs/README.md).

## Quick start

1. **Get the toolchain** — Follow [Build your device](../docs/getting-started/build-your-device.md) to turn examples into your product skeleton.
2. **Run on ESP32 or the POSIX host / QEMU** — [Quick start](../docs/getting-started/quick-start.md) runs the thermostat on the host in minutes; for emulated silicon, pair that with [QEMU testing](../docs/guides/qemu-testing.md) and [ESP-IDF](../docs/guides/esp-idf.md) as needed.
3. **Author a WASM applet** — [Your first applet](../docs/getting-started/your-first-applet.md) walks through hub-pushed applet flow.

## Targets and PAL backends

| Target | Status | Notes |
|--------|--------|-------|
| **ESP32 / ESP32-C3 / ESP32-C6 / ESP32-H2 (ESP-IDF)** | Supported | Primary ecosystem: PAL under `pal/esp-idf/`, transports `transport_wifi`, `transport_eth`, `transport_thread`, `transport_ble`; chip-specific defaults in examples’ `sdkconfig.defaults.*`; see [ESP-IDF](../docs/guides/esp-idf.md). |
| **POSIX (Linux / macOS / WSL)** | Supported | CMake standalone library with `pal/posix/` and **`transport_posix.c`** stub (assumes existing IP connectivity); used for thermostat and CI-style builds; see [Native development](../docs/guides/native-development.md). |
| **QEMU (ESP32-C3 via ESP-IDF)** | Supported (network / core flows) | `idf.py qemu monitor`; not all peripherals (BLE, Thread RF, I2C in QEMU) — see limitation table in [QEMU testing](../docs/guides/qemu-testing.md). |
| **Mock PAL** | Tests only | `pal/mock/` + Unity in `test/`; not linked for firmware images. |

## Public API

The stable surface is the headers under `include/`, compiled with **`-std=c11`**.

- `tw_device.h` — Entry point (`tw_device_run`, `tw_device_config_t`, `TW_DEVICE_MAIN`).
- `tw_msg.h`, `tw_types.h`, `tw_coap_codes.h` — Message model and protocol façade (`tw_protocol_t`).
- `tw_cmd.h` — Command resources and helpers for hub-invoked handlers.
- `tw_hub.h`, `tw_ota.h`, `tw_applet.h`, `tw_identity.h` — Hub session, OTA, applet lifecycle, identity.
- `tw_sensor.h`, `tw_config.h`, `tw_button.h`, `tw_led.h` — Sensors, persisted config, UX primitives.
- `tw_kvtext.h`, `tw_block.h`, `tw_safety.h`, `tw_lock.h` — INI-ish wire helpers, Block transfer, monitors, optional mutex no-ops (`CONFIG_TW_THREAD_SAFETY`).

Generated protobuf types accompany **`proto/tw_protocol.proto`** at build time.
Full prototypes and prose: [C API reference](../docs/reference/api.md) and **applet-visible** [Host API](../docs/reference/host-api.md).

## Configuration (Kconfig)

ESP-IDF builds expose SDK options under **Component config → TW Device SDK** (transports, network, power, OTA, identity, applets, hub timing, …).
Standalone POSIX builds select comparable behavior via compile definitions where applicable.
Detailed menu reference: [Kconfig reference](../docs/reference/kconfig.md).

## Porting

New silicon or RTOS ports **implement `pal_*` and wire transports** — the service and protocol layers should remain untouched.

**Examples** under **`../examples/`** follow the same top-down layering as [Architecture](#architecture) above (**application → services → protocol → transport → PAL**).
Use them as a porting checklist, not only as demos.

Concrete steps and module checklist: [Porting](../docs/guides/porting.md).
PAL contracts: [PAL reference](../docs/reference/pal.md).

## Protocols

- [Wire specification](../docs/protocol/wire-specification.md) — CoAP paths, nanopb payloads, kvtext conventions.
- [Hub protocol](../docs/protocol/hub-protocol.md) — Heartbeats, mailbox, command flow.
- [Applet protocol](../docs/protocol/applet-protocol.md) — Push/commit and hot-swap.

## Provisioning

BLE, SoftAP, and LAN-oriented setup share the same provisioning service code paths; product policy is mostly Kconfig + partition layout.

Guide: [Provisioning](../docs/guides/provisioning.md).

Wire details: [Wire specification](../docs/protocol/wire-specification.md).

## OTA

A/B style updates use `pal_ota` and `svc_ota.c` with integrity checks as configured.

Guide: [OTA updates](../docs/guides/ota-updates.md).

Hub interaction: [Hub protocol](../docs/protocol/hub-protocol.md).

## Applets

WAMR loads WASM from flash; the hub pushes bytes and commits a new revision.

Guides: [Writing applets](../docs/guides/writing-applets.md).

Runtime internals: [Applet runtime](../docs/architecture/applet-runtime.md), [Applet protocol](../docs/protocol/applet-protocol.md).

## Examples

Upstream examples ship next to this SDK:

- [`../examples/thermostat/`](../examples/thermostat/) — Full native firmware sample (POSIX + ESP-IDF tree).
- [`../examples/applet-device/`](../examples/applet-device/) — WASM applets pushed from tooling/hub workflows.
- [`../examples/minimal/`](../examples/minimal/) — Smallest illustrative `TW_DEVICE_MAIN` sketch.

## Testing

- **`test/unit/`** — **Unity** tests for protobuf round-trip, provisioning, heartbeat, identity, CoAP dispatch, OTA/applet state, `tw_kvtext`, button debounce, safety, with **`pal/mock`** injecting PAL behavior.

  To add a new C suite, follow **`test/CMakeLists.txt`** (`add_sdk_test`, includes, mocked PAL sources).

- **`test/integration/`** — pytest modules (`test_sdk_coap_flow.py`, `test_sdk_heartbeat.py`, `test_provision.py`, …) for higher-level scenarios.

Requires **CMake**, **Unity**, and **nanopb** fetched by **`test/CMakeLists.txt`**, plus **`protoc`** for unit builds.

Integration tests expect **pytest** available on your `PATH**.

```bash
# C unit tests (cwd: tw-device-sdk/)
cd test
cmake -B build -S .
cmake --build build
ctest --test-dir build
```

Use the **`pytest`** line whose path matches **`cwd`** (two equivalent entry points below).

```bash
# cwd: repository root
pytest extras/firmwareless/device/tw-device-sdk/test/integration
```

```bash
# cwd: tw-device-sdk/
pytest test/integration
```

For QEMU-based validation of firmware images rather than POSIX unit tests alone, follow [QEMU testing](../docs/guides/qemu-testing.md).

## Documentation map

| Area | Contents |
|------|----------|
| `../docs/getting-started/` | Toolchain, quick start, minimal bring-up, applet first steps |
| `../docs/architecture/` | Layering, applet runtime |
| `../docs/guides/` | ESP-IDF, porting, OTA, provisioning, power, native dev, applets, QEMU, troubleshooting |
| `../docs/protocol/` | Wire, hub, applet specifications |
| `../docs/reference/` | C API, host API, PAL, Kconfig |

Index with tables: [Documentation](../docs/README.md).

## License and contributing

Licensed under the same terms as the repository — see [**LICENSE**](../../../../LICENSE).
Contribution expectations: [**CONTRIBUTING.md**](../../../../CONTRIBUTING.md).
