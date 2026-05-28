# Remaining Work -- v0.1

> Tracked open items for the current device SDK release.
> Each entry includes a priority, a short description, and the source file(s) where the limitation or placeholder lives.
> **Priority key:** **P0** = blocks production use, **P1** = important for real deployments, **P2** = nice-to-have / future improvement.

---

## ~~1  Transport / Wire Protocol~~ (RESOLVED)

### ~~P0 -- Replace text-based UDP stub with RFC 7252 binary CoAP~~

**RESOLVED:** `proto_coap.c` now uses **libcoap** for binary CoAP (RFC 7252).
The text stub and `TW_COAP_TEXT_*` constants have been removed.
ESP-IDF builds link the `coap` component; POSIX builds pull libcoap via CMake `FetchContent`.

### ~~P0 -- Implement CoAP Block1/Block2 transfers (RFC 7959)~~

**RESOLVED:** `svc_ota.c` and `svc_applet.c` now validate Block1 sequence numbers, echo Block1 options in responses, and reject non-Block1 large payloads.
A new `tw_block_ctx_t` helper (`tw_block.h`) provides opt-in full-payload reassembly for application handlers.
Block2 is handled transparently by libcoap.

### P1 -- Add DTLS support

Port 5684 (`coaps`) is used by convention but no DTLS handshake is performed.
All traffic is cleartext, including heartbeats that carry device identity.

| Source | Location |
|--------|----------|
| `docs/wire-specification.md` | Line 58 |
| `svc/src/svc_heartbeat.c` | Line 36 ("cleartext CoAP") |

---

## 2  Protocol Abstraction

### P2 -- Implement MQTT backend

`proto_mqtt.c` is a compile-time `#error` stub.
The file contains a 7-step implementation roadmap:

1. Implement `tw_protocol_t` vtable (`init`, `poll`, `deinit`, `send`) using an MQTT client library.
2. Implement `tw_msg_respond_*` helpers as MQTT publishes.
3. Map `tw_msg_resource_t` subscriptions to MQTT topic filters.
4. Update `svc_heartbeat.c` for MQTT publish.
5. Update `svc_device.c` to use the MQTT vtable.
6. Add MQTT-specific Kconfig options (broker URI, client ID, QoS, TLS).
7. Test with Mosquitto or similar broker.

| Source | Location |
|--------|----------|
| `protocol/src/proto_mqtt.c` | Lines 2--26 (stub + roadmap) |
| `docs/architecture.md` | Lines 92, 133 |
| `docs/kconfig-reference.md` | Line 82 |
| `CHANGELOG.md` | Known Limitations (line 105) |

---

## 3  Security and Identity

### P0 -- Implement Ed25519 message signing

Only public key export is implemented.
The actual signing path (sign outgoing messages, verify incoming ones) is deferred.

| Source | Location |
|--------|----------|
| `svc/src/svc_identity.c` | Line 74 ("signing path is deferred") |
| `docs/hub-protocol.md` | Lines 108--109 |
| `CHANGELOG.md` | Known Limitations (line 107) |

### P1 -- Implement PSK rotation protocol

No mechanism exists for the hub and device to rotate the pre-shared key after initial provisioning.

| Source | Location |
|--------|----------|
| `CHANGELOG.md` | Known Limitations (line 106) |

### P1 -- Replace POSIX Ed25519 placeholder

The POSIX build returns `priv[i] ^ 0xA5` as the public key -- a deterministic but cryptographically meaningless value.
Host builds that need real Ed25519 must link a proper library (e.g. libsodium, OpenSSL).

| Source | Location |
|--------|----------|
| `svc/src/svc_identity.c` | Lines 130--138 (xor-fold stub) |
| `test/unit/test_identity.c` | Lines 191, 312 (test expects stub) |
| `CHANGELOG.md` | Known Limitations (line 108) |

### P1 -- Add Ed25519 OTA image signature verification

OTA verification step 3 is marked "(future)": verify an Ed25519 signature over the image hash before committing the update.

| Source | Location |
|--------|----------|
| `docs/ota-protocol.md` | Line 73 |

---

## 4  BLE Transport

### P1 -- Parse incoming CoAP-over-BLE requests

The BLE GATT write handler logs the event but does not parse the request, dispatch it through the resource table, or send a response.

| Source | Location |
|--------|----------|
| `transport/src/transport_ble.c` | Lines 61--64 (`TODO`) |

### P1 -- Register TW CoAP-over-BLE GATT service and characteristics

The BLE transport initialises the controller and registers callbacks but never creates the GATT service or starts advertising with the correct characteristics.

| Source | Location |
|--------|----------|
| `transport/src/transport_ble.c` | Lines 104--106 (`TODO`) |

---

## 5  WASM / Applet Runtime

### P1 -- Integrate WAMR runtime

The entire `wasm_runtime.c` is a lifecycle skeleton.
WAMR must be initialised, the flash partition memory-mapped, the WASM module instantiated, and host bindings registered.

| Source | Location |
|--------|----------|
| `examples/applet-device/runtime/src/wasm_runtime.c` | Line 38 (`TODO: Initialise WAMR`) |

### P1 -- Wire applet_tick() call

The per-loop tick handler is empty; it should call the applet's `applet_tick()` export through WAMR.

| Source | Location |
|--------|----------|
| `examples/applet-device/runtime/src/wasm_runtime.c` | Line 58 (`TODO: Call applet_tick()`) |

### P1 -- Forward hub commands to applet on_command() export

`TW_CMD_APP` commands are logged but never forwarded to the WASM module's `on_command()` export.

| Source | Location |
|--------|----------|
| `examples/applet-device/runtime/src/wasm_runtime.c` | Line 72 (`TODO: Forward to on_command()`) |

### P2 -- Implement applet CoAP messaging

GET requests to the applet endpoint currently return a hard-coded "applet messaging not yet implemented" string instead of forwarding to the applet's `on_coap()` export.

| Source | Location |
|--------|----------|
| `examples/applet-device/runtime/src/wasm_runtime.c` | Lines 93--95 (`TODO: Forward to applet_on_coap()`) |

---

## 6  Platform Support

### P2 -- Add Zephyr PAL

Architecture diagrams show Zephyr as a future PAL target alongside ESP-IDF and POSIX.
No implementation exists.

| Source | Location |
|--------|----------|
| `docs/architecture.md` | Line 35 ("(future)") |
| `../README.md` (device) | Line 58 |

### P2 -- Implement full Ethernet teardown

`transport_eth.c` has a placeholder teardown; full driver release is left to platform integration.

| Source | Location |
|--------|----------|
| `transport/src/transport_eth.c` | Teardown function comment |

### P2 -- QEMU emulation gaps

Several subsystems are not fully emulated under ESP-IDF's QEMU target:

| Subsystem | Status |
|-----------|--------|
| Deep sleep / wake | Partial |
| GPIO input/output | Stubbed |
| I2C sensors | Not available |
| BLE | Not available |
| Thread (802.15.4) | Not available |

| Source | Location |
|--------|----------|
| `docs/qemu-testing.md` | Line 82 |

---

## ~~7  Documentation Consistency~~ (RESOLVED)

- All documentation updated to reflect the hub-push protobuf model.
- `tw_cmd_type_t` / `tw_hub_command_t` references removed from all docs.
- `on_hub_command` replaced by `on_command` everywhere.
- Endpoint tables use protobuf message names.
- Mermaid diagrams added for provisioning lifecycle, hub-push flow, and multi-country manufacturing.
- Kconfig reference updated with all new options.

---

## ~~8  Tooling~~ (RESOLVED)

- **`provision_ble.py` deleted.** The legacy script has been removed from `scripts/` and all references cleaned from `scripts/README.md`.
  `provision.py ble` is the sole BLE provisioning path.

---

## ~~9  Hub Simulator~~ (RESOLVED)

### ~~P1 -- Migrate `fake_hub.py` to `Tinkwell.Coap.Server`~~

**RESOLVED:** `fake_hub.py` is now a thin Python wrapper (~50 lines) that invokes `tw coap server --mailbox /hub/heartbeat` under the hood.
All hub simulation traffic uses binary CoAP (RFC 7252) via `Tinkwell.Coap.Server`.
The `tw` CLI must be on PATH.

### P1 -- Update `provision.py` tw CLI dependency

`provision.py` now requires the `tw` CLI (Tinkwell CLI) on PATH for LAN/SoftAP CoAP interactions.
The `tw` CLI must be documented as a prerequisite in `scripts/README.md` and the getting-started guide.

---

## ~~10  Hub-Push Protobuf Migration~~ (RESOLVED)

All wire protocol endpoints migrated from text/kvtext to Protocol Buffers (nanopb on device, Google.Protobuf on hub/.NET):

- **`tw_protocol.proto`** -- canonical schema with 18 messages.
- **`tw_protocol.options`** -- nanopb constraints (max_size, max_count, fixed_length for GUIDs and SHA-256 digests).
- **Heartbeat** -- `HeartbeatPayload` / `HeartbeatReply` replace raw kvtext.
  Optional `repeated SensorReading` in heartbeat payload.
- **Hub-push commands** -- Hub POSTs to `/tw/<command>` with protobuf payloads; device decodes, executes, replies with protobuf.
- **Provisioning** -- `FactoryProvisionCmd` (idempotent, with `finalize` flag), `HubProvisionCmd` (id-only), `ProvisionInfo` (with `factory_done` and `factory_finalized` flags).
- **OTA / Applet** -- `OtaBeginCmd`, `OtaStatus`, `OtaCommitReply`, `AppletStatus`, `AppletCommitReply` replace text responses.
- **Sensor telemetry** -- `TelemetryPush` / `TelemetryReply` for dedicated push endpoint.
- **.NET CLI** -- `tw coap server --queue command[:json]` encodes commands to protobuf, decodes device responses to JSON.
- **BLE defaults off** -- `TW_TRANSPORT_BLE` and `TW_TRANSPORT_BLE_PROVISIONING` default to `n`.
- **GPIO reset button** -- Kconfig-gated re-provisioning via GPIO hold.

---

## ~~11  Testing~~ (PARTIAL)

### ~~P1 -- Device-side unit tests~~

**RESOLVED:** Test build infrastructure (`test/CMakeLists.txt`) created with Unity framework and nanopb support.
New protobuf tests added:

- **test_proto_roundtrip** (21 tests) -- encode/decode roundtrip for all 18 protobuf messages, verifying nanopb `.options` constraints.
- **test_cmd_handlers** (13 tests) -- hub-pushed command endpoint handlers via `svc_cmd_resources[]` with protobuf payloads.
- **test_provision_proto** (13 tests) -- provisioning state machine, NVS flags, POSIX provisioning path, protobuf message validation.

Pre-existing tests (kvtext, coap dispatch, identity, heartbeat, button, safety, OTA, applet) also wired into the CTest runner.

Build: `cd test && cmake -B build && cmake --build build && ctest --test-dir build`

### P1 -- .NET unit tests for CoapServerCommand

The `CoapServerCommand` protobuf encoding/decoding (queue parsing, `HeartbeatReply` generation, `HeartbeatPayload` / `TelemetryPush` decoding) should have unit tests.

| Source | Location |
|--------|----------|
| `src/Tinkwell.Cli.Commands.Coap/` | CoapServerCommand.cs |
