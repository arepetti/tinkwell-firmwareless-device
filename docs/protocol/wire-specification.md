<!--
SPDX-License-Identifier: MIT
-->

# Wire specification: Tinkwell Firmwareless device and edge hub

**Document version:** v0.3   **Transport:** Binary CoAP ([RFC 7252](https://www.rfc-editor.org/rfc/rfc7252)) with Block1 ([RFC 7959](https://www.rfc-editor.org/rfc/rfc7959)) for large raw transfers, implemented via **libcoap**.

This document is the **canonical wire protocol reference** for communication between a Tinkwell Firmwareless device and its edge hub.
Payloads are **Protocol Buffers** (protobuf): **nanopb** on the device, **Google.Protobuf** on .NET.
An implementer can build a compatible hub (for example in Python) or other tooling using this document together with the schema file.
An automated agent may treat this file as the single authoritative description of on-the-wire behavior at the application layer.

---

## Table of contents

1. [Canonical schema](#canonical-schema)
2. [Transport and payload rules](#transport-and-payload-rules)
3. [UDP ports](#udp-ports)
4. [Main app endpoints (device CoAP server)](#main-app-endpoints-device-coap-server)
5. [Provisioning partition endpoints](#provisioning-partition-endpoints)
6. [Hub-side endpoints (device as UDP client)](#hub-side-endpoints-device-as-udp-client)
7. [Hub push command model](#hub-push-command-model)
8. [JSON examples (tooling and `tw-coap-server`)](#json-examples-tooling-and-tw-coap-server)
9. [Encoding and interoperability](#encoding-and-interoperability)
10. [Timing and timeouts](#timing-and-timeouts)
11. [Document history](#document-history)

---

## Canonical schema

The **single source of truth** for message fields and types is:

`proto/tw_protocol.proto` (in the Device SDK tree)

All protobuf message definitions live in that file.
Do not duplicate field semantics in this document beyond summary tables; when in doubt, read the `.proto` file.

---

## Transport and payload rules

The device SDK uses **binary CoAP** over UDP.
Requests and responses are proper CoAP messages with binary headers, options, and payload markers.

### Protobuf by default

Unless listed in [Exceptions (non-protobuf bodies)](#exceptions-non-protobuf-bodies), **request and response bodies are serialized protobuf messages** from `tw_protocol.proto` (`package tw;`).
Serialize the appropriate message type for the resource’s method and direction.

### Exceptions (non-protobuf bodies)

| Resource | Direction | Body |
|----------|-----------|------|
| `PUT /tw/ota/block` | Hub → device | Raw firmware bytes transferred with **Block1** ([RFC 7959](https://www.rfc-editor.org/rfc/rfc7959)) |
| `POST /tw/applet/push` | Hub → device | Raw WASM bytes with **Block1** |
| `GET /tw/identity/pubkey` | Device → client | Raw Ed25519 public key bytes (32 bytes on success) |

All other endpoints in this document use protobuf payloads.

### Request format (RFC 7252 binary CoAP)

| Field | Description |
|-------|-------------|
| Version | CoAP version 1 (2 bits). |
| Type | CON (confirmable) or NON (non-confirmable). |
| Code | Method code: GET (0.01), POST (0.02), PUT (0.03), DELETE (0.04). |
| Message ID | 16-bit identifier for deduplication and retransmission. |
| Token | 0–8 byte request/response correlation token. |
| Options | URI-Path, Block1/Block2, Content-Format, etc. encoded per RFC 7252 section 3.1. |
| Payload | Binary payload after the `0xFF` marker byte. |

### Response format

Responses carry a CoAP response code in the header (for example `2.05 Content` = `0x45`, `2.04 Changed` = `0x44`, `2.01 Created` = `0x41`, `2.31 Continue` = `0x5F`) and an optional payload.
The stack handles ACK/retransmission and deduplication.

### Block1 transfers (RFC 7959)

OTA firmware and applet WASM use **Block1** for large binary streams:

1. The client sends block 0 with `Block1: NUM=0, M=1, SZX=…` (size class per negotiation).
2. The device may respond `2.31 Continue` with Block1 echoed.
3. Repeat until the last block (`M=0`).
4. The device responds `2.04 Changed` for the final block.

Out-of-order blocks typically receive `4.08 Request Entity Incomplete`.
Exact diagnostics are implementation-defined.

---

## UDP ports

| Role | Port | Notes |
|------|------|--------|
| Device (server) | **5683** | RFC 7252 section 12.2; IANA name `coap`. |
| Hub (default client destination for device-originated traffic) | **5684** | IANA name `coaps`; used **by convention** as the hub UDP endpoint even when the stack does not use DTLS. |

---

## Main app endpoints (device CoAP server)

The device listens on UDP **5683** and exposes the resources below.
Application code may register **additional** resources via `tw_device_config_t.resources`; those use the same binary CoAP model unless documented otherwise.

| Endpoint | Method | Request body | Response code / body |
|----------|--------|--------------|----------------------|
| `/tw/reboot` | POST | empty | `2.04 Changed`, no body |
| `/tw/set-config` | POST | `SetConfigCmd` | `2.04 Changed`, no body |
| `/tw/ota-available` | POST | `OtaAvailableCmd` | `2.04 Changed`, no body |
| `/tw/app` | POST | `AppCmd` | `2.04 Changed`, **app-defined** response body (opaque) |
| `/tw/info` | GET | — | `2.05 Content`, `DeviceInfo` |
| `/tw/ota/begin` | POST | `OtaBeginCmd` | `2.01 Created` (no protobuf body) |
| `/tw/ota/block` | PUT | raw bytes (Block1) | `2.31 Continue` / `2.04 Changed` |
| `/tw/ota/commit` | POST | empty | `2.04 Changed`, `OtaCommitReply` |
| `/tw/ota/status` | GET | — | `2.05 Content`, `OtaStatus` |
| `/tw/applet/push` | POST | raw bytes (Block1) | `2.31 Continue` / `2.04 Changed` |
| `/tw/applet/commit` | POST | empty | `2.04 Changed`, `AppletCommitReply` |
| `/tw/applet/status` | GET | — | `2.05 Content`, `AppletStatus` |
| `/tw/identity/pubkey` | GET | — | `2.05 Content`, raw 32-byte public key |

---

## Provisioning partition endpoints

These resources are served from the **provisioning** firmware partition (not the main application image).
Payloads are still defined in `tw_protocol.proto`.

| Endpoint | Method | Request body | Response body |
|----------|--------|--------------|---------------|
| `/tw/provision/factory` | POST | `FactoryProvisionCmd` | `ProvisionReply` |
| `/tw/provision/hub` | POST | `HubProvisionCmd` | `ProvisionReply` |
| `/tw/provision/set` | POST | `ProvisionSetCmd` | `ProvisionReply` |
| `/tw/provision/info` | GET | — | `ProvisionInfo` |

---

## Hub-side endpoints (device as UDP client)

The device opens a CoAP client session to the hub (address and port from provisioning; default port **5684**) and sends **confirmable** requests.
Bodies are protobuf.

| Endpoint | Method | Request body | Response body |
|----------|--------|--------------|---------------|
| `/hub/heartbeat` | POST | `HeartbeatPayload` | `HeartbeatReply` |
| `/hub/telemetry` | POST | `TelemetryPush` | `TelemetryReply` |

---

## Hub push command model

Commands are **not** embedded as bulk text in the heartbeat response.
Instead:

1. The device sends **`POST /hub/heartbeat`** with a **`HeartbeatPayload`** protobuf.
2. The hub responds with **`HeartbeatReply`**, whose **`pending`** field is the number of commands the hub intends to deliver.
3. The hub then initiates **separate CoAP client requests** to the device (UDP **5683**), typically one **`POST` per pending command**, to the appropriate device resources (for example `/tw/set-config`, `/tw/ota-available`, `/tw/reboot`, `/tw/app`), each carrying the matching protobuf message type.

The device acts as CoAP **server** for those inbound pushes; the hub acts as **client**.
Ordering and retry policy are hub policy; the heartbeat reply only advertises how many operations the hub will attempt to push.

### Sequence: heartbeat then per-command POSTs

```mermaid
sequenceDiagram
    participant Device
    participant Hub

    Note over Device,Hub: Device listens on UDP 5683; hub on 5684. For heartbeat, device is CoAP client to hub; for pushes, hub is CoAP client to device.

    Device->>Hub: POST /hub/heartbeat (HeartbeatPayload)
    Hub-->>Device: 2.05 Content (HeartbeatReply, pending = N)

    loop For each of N commands
        Hub->>Device: POST /tw/... (protobuf command)
        Device-->>Hub: 2.04 Changed
    end

    Device->>Hub: POST /hub/telemetry (TelemetryPush), optional
    Hub-->>Device: 2.05 Content (TelemetryReply)
```

---

## JSON examples (tooling and `tw coap server`)

For **`tw coap server`**, manual tests, and external tools, payloads may be represented as **JSON** objects that map to the same field names as **protobuf JSON mapping** (camelCase fields; `bytes` as base64 strings).
The binary on the wire remains protobuf unless the tool explicitly accepts JSON and encodes internally.

Section headings name the protobuf message type each example illustrates.

### Shared

**`ConfigEntry`**

```json
{
  "key": "example-key",
  "value": "example-value"
}
```

### Hub-to-device commands (main app)

**`SetConfigCmd`**

```json
{
  "entries": [
    { "key": "wifi.ssid", "value": "MyNetwork" },
    { "key": "log.level", "value": "info" }
  ]
}
```

**`OtaAvailableCmd`**

```json
{
  "url": "https://updates.example.com/fw/v1.2.3.bin",
  "sha256": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
  "sizeBytes": 524288
}
```

(`sha256` is 32 bytes in protobuf; in JSON, base64.)

**`AppCmd`**

```json
{
  "payload": "c29tZSBvcGFxdWUgYnl0ZXM="
}
```

### Device info and OTA / applet

**`DeviceInfo`**

```json
{
  "id": "AAAAAAAAAAAAAAAAAAAAAA==",
  "vendorId": 1,
  "vendorDisplayName": "Example Vendor",
  "productId": 42,
  "productDisplayName": "Example Product",
  "serialNumber": 12345,
  "variant": "AQ==",
  "fwVersion": "1.0.0",
  "deviceName": "sensor-01"
}
```

**`OtaBeginCmd`**

```json
{
  "sizeBytes": 524288,
  "sha256": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
  "version": "1.2.3"
}
```

**`OtaStatus`**

```json
{
  "state": "downloading",
  "progressPercent": 45,
  "error": ""
}
```

**`OtaCommitReply`**

```json
{
  "success": true,
  "error": ""
}
```

**`AppletStatus`**

```json
{
  "state": "running",
  "sizeBytes": 8192,
  "sha256": "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA="
}
```

**`AppletCommitReply`**

```json
{
  "success": true,
  "error": ""
}
```

### Provisioning

**`FactoryProvisionCmd`**

```json
{
  "vendorId": 1,
  "vendorDisplayName": "Example Vendor",
  "productId": 42,
  "productDisplayName": "Example Product",
  "serialNumber": 1000,
  "variant": "AA==",
  "id": "AAAAAAAAAAAAAAAAAAAAAA==",
  "psk": "",
  "ed25519PrivateKey": "",
  "finalize": false
}
```

**`HubProvisionCmd`**

```json
{
  "id": "AAAAAAAAAAAAAAAAAAAAAA=="
}
```

**`ProvisionSetCmd`**

```json
{
  "fields": [
    { "key": "wifi.ssid", "value": "MySSID" },
    { "key": "hub.url", "value": "coap://192.168.1.10:5684" }
  ]
}
```

**`ProvisionInfo`**

```json
{
  "status": "provisioned",
  "device": {
    "id": "AAAAAAAAAAAAAAAAAAAAAA==",
    "vendorId": 1,
    "vendorDisplayName": "Example Vendor",
    "productId": 42,
    "productDisplayName": "Example Product",
    "serialNumber": 12345,
    "variant": "AQ==",
    "fwVersion": "1.0.0",
    "deviceName": "device-1"
  },
  "factoryDone": true,
  "factoryFinalized": true
}
```

**`ProvisionReply`**

```json
{
  "success": true,
  "error": ""
}
```

### Hub-facing (device → hub)

**`SensorReading`**

```json
{
  "name": "temperature",
  "value": 23.5,
  "timestampMs": 1711800000000
}
```

**`HeartbeatPayload`**

```json
{
  "id": "AAAAAAAAAAAAAAAAAAAAAA==",
  "vendorId": 1,
  "productId": 42,
  "serialNumber": 12345,
  "fwVersion": "1.0.0",
  "uptimeMs": 3600000,
  "freeHeap": 45000,
  "bootReason": 0,
  "appData": "",
  "sensors": [
    {
      "name": "temperature",
      "value": 22.1,
      "timestampMs": 1711800000000
    }
  ]
}
```

**`HeartbeatReply`**

```json
{
  "pending": 2
}
```

**`TelemetryPush`**

```json
{
  "readings": [
    {
      "name": "humidity",
      "value": 55.0,
      "timestampMs": 1711800001000
    }
  ]
}
```

**`TelemetryReply`**

```json
{
  "nextIntervalS": 60
}
```

---

## Encoding and interoperability

- **`tw_protocol.proto`** is the **authoritative** schema.
  Field numbers, `repeated` / `bytes` semantics, and optional fields are defined only there.
- **Power users** may use `protoc` with **`--encode`** and **`--decode`** to convert between binary protobuf and text/JSON workflows, for example:

  ```text
  protoc --encode=tw.HeartbeatPayload tw_protocol.proto < heartbeat.json
  ```

  Use the fully qualified message name as declared in the schema (`package tw;` → `tw.MessageName`).

- **CoAP** framing follows [RFC 7252](https://www.rfc-editor.org/rfc/rfc7252); **block-wise transfer** for firmware and applets follows [RFC 7959](https://www.rfc-editor.org/rfc/rfc7959).

---

## CoAP resource discovery

The device supports CoAP `.well-known/core` (RFC 6690) for resource discovery:

```
GET /.well-known/core

<tw/sensor/temperature>;rt="temperature",
<tw/sensor/humidity>;rt="humidity",
<tw/mode>;rt="mode",
...
```

Application-registered resources are included automatically.

---

## CoAP response codes

| CoAP Code | Meaning |
|-----------|---------|
| 2.01 | Created (OTA/applet begin) |
| 2.04 | Changed (PUT/commit success) |
| 2.05 | Content (GET response) |
| 2.31 | Continue (Block1 acknowledged) |
| 4.00 | Bad Request (missing/invalid params) |
| 4.03 | Forbidden (factory finalized, provisioning locked) |
| 4.04 | Not Found (unknown resource path) |
| 4.05 | Method Not Allowed |
| 4.08 | Request Entity Incomplete (block out of order or no session) |
| 4.13 | Request Entity Too Large (payload exceeds declared size) |
| 5.00 | Internal Server Error |

---

## Timing and timeouts

| Parameter | Typical value |
|-----------|----------------|
| Heartbeat interval (always-on mode) | `CONFIG_TW_HEARTBEAT_INTERVAL_S` (default **60** s) |
| Heartbeat on boot | When enabled via `CONFIG_TW_HEARTBEAT_ON_BOOT` |
| Deep sleep cycle | Sleep `CONFIG_TW_SLEEP_INTERVAL_S` (default **300** s), then wake, tick, heartbeat, listen for `CONFIG_TW_SLEEP_LISTEN_WINDOW_S` (default **30** s), then sleep again |
| Heartbeat send timeout | **5000** ms |
| CoAP poll timeout in provisioning loops | **100** ms |

Exact values are configuration-dependent; consult the SDK `Kconfig` / headers for the target build.

---

## Document history

| Version | Notes |
|---------|--------|
| v0.1 | Initial canonical specification; text-stub UDP transport. |
| v0.2 | Binary CoAP (RFC 7252) with libcoap; Block1 (RFC 7959) for OTA and applet transfers; kvtext payloads. |
| v0.3 | **Protobuf** wire payloads from `tw_protocol.proto` (nanopb / Google.Protobuf); hub push model via heartbeat `pending` plus per-command CoAP POSTs to the device; exceptions documented for raw Block1 and raw public key. |
