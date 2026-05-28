# Kconfig Reference

All SDK configuration options, set via `idf.py menuconfig` (ESP-IDF) or `-DCONFIG_xxx=value` (POSIX CMake).

---

## Thread Safety

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_TW_THREAD_SAFETY` | bool | y | Enable mutex protection for shared SDK state. Disable only for strictly single-threaded builds; doing so removes all lock overhead but any concurrent access to SDK state becomes undefined behavior. |

## Protobuf

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_TW_USE_PROTOBUF` | bool | y | Use nanopb for wire encoding/decoding. When disabled, endpoints use raw bytes or kvtext fallback; application code is responsible for payload interpretation. |
| `CONFIG_TW_CMD_PATH_PREFIX` | string | `"tw"` | CoAP path prefix for hub-pushed commands. Endpoints become `/<prefix>/reboot`, `/<prefix>/set-config`, etc. Change if another service occupies the `/tw/` namespace. |

## Transport

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_TW_TRANSPORT_WIFI` | bool | y | Enable WiFi station transport. |
| `CONFIG_TW_TRANSPORT_ETHERNET` | bool | n | Enable Ethernet transport. |
| `CONFIG_TW_TRANSPORT_THREAD` | bool | n | Enable Thread 802.15.4 transport. |
| `CONFIG_TW_TRANSPORT_BLE` | bool | **n** | Enable BLE transport in the **main app** binary. Default **n** saves ~100 KB flash and ~35 KB RAM. The provisioning partition enables BLE independently in its own `sdkconfig`. Enable here only if BLE is used as a **runtime** transport (not just for provisioning). |
| `CONFIG_TW_TRANSPORT_BLE_PROVISIONING` | bool | **n** | Use BLE for network provisioning in this binary. Depends on `TW_TRANSPORT_BLE`. Normally the provisioning partition sets this to `y` in its own `sdkconfig`. |

## Network

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_TW_NET_USE_DHCP` | bool | y | Try DHCP first. If it fails within the timeout, fall back to the static IP below. |
| `CONFIG_TW_NET_DHCP_TIMEOUT_S` | int | 15 | DHCP timeout in seconds before static IP fallback. Depends on `TW_NET_USE_DHCP`. |
| `CONFIG_TW_NET_STATIC_IP` | string | `"192.168.1.100"` | Static IP address (fallback or primary when DHCP is disabled). |
| `CONFIG_TW_NET_STATIC_NETMASK` | string | `"255.255.255.0"` | Static netmask. |
| `CONFIG_TW_NET_STATIC_GW` | string | `"192.168.1.1"` | Static gateway. |

## Provisioning

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_TW_PROVISION_SOFTAP` | bool | y | Enable SoftAP provisioning (requires WiFi). When unprovisioned, starts a WiFi access point for LAN-based provisioning via CoAP. |
| `CONFIG_TW_PROVISION_SOFTAP_SSID_PREFIX` | string | `"TW-Prov"` | SoftAP SSID prefix. The actual SSID is `<prefix>-<last 2 MAC bytes>` (e.g. `TW-Prov-A1B2`). |
| `CONFIG_TW_PROVISION_SOFTAP_PASSWORD` | string | `""` (empty) | WPA2-PSK password for the provisioning AP. **Empty = open network** -- anyone in radio range can connect and provision the device. Set a password for production deployments. |
| `CONFIG_TW_PROVISION_SOFTAP_MAX_CONN` | int | 4 | Maximum simultaneous SoftAP client connections during provisioning. |
| `CONFIG_TW_PROVISION_POLL_INTERVAL_MS` | int | 500 | Sleep interval (ms) between CoAP server polls during provisioning (SoftAP, LAN, and BLE paths). Lower = more responsive, higher = less CPU usage. |

## Provisioning Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_TW_FACTORY_PROVISION_REQUIRED` | bool | n | Require at least one factory provisioning pass before hub provisioning is allowed. When **n** (default), hub provisioning proceeds immediately with compile-time identity defaults. When **y**, `POST /tw/provision/hub` returns **4.03 Forbidden** until `POST /tw/provision/factory` has been called at least once. Factory provisioning is idempotent and can be called multiple times until `finalize=true` locks it permanently. |

## GPIO Reset Button

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_TW_REPROVISION_GPIO` | bool | n | Enable GPIO button for user-initiated re-provisioning. When triggered, clears hub provisioning and reboots to the provisioning partition. **No** automatic re-provisioning on WiFi failure. |
| `CONFIG_TW_REPROVISION_GPIO_PIN` | int | 9 | GPIO pin for reset button (active low with internal pull-up). Default 9 is the BOOT button on ESP32-C3 DevKit. Depends on `TW_REPROVISION_GPIO`. |
| `CONFIG_TW_REPROVISION_GPIO_HOLD_S` | int | 5 | Button must be held continuously for this many seconds to trigger re-provisioning. Brief presses are ignored (debounce). Must be > 0. Depends on `TW_REPROVISION_GPIO`. |

## Power Management

These options only take effect when `CONFIG_TW_POWER_DEEP_SLEEP=y`.
In **always-on mode** the device uses `CONFIG_TW_HEARTBEAT_INTERVAL_S` for heartbeat timing and remains fully awake.

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_TW_POWER_DEEP_SLEEP` | bool | n | Enable deep sleep mode. Device alternates between wake (heartbeat + listen window) and deep sleep cycles. |
| `CONFIG_TW_SLEEP_INTERVAL_S` | int | 300 | Seconds between wake cycles. Must be > 0 (setting to 0 causes platform-dependent behavior, potentially a tight loop). |
| `CONFIG_TW_SLEEP_LISTEN_WINDOW_S` | int | 30 | Seconds to stay awake after heartbeat when the hub has **pending commands** (`HeartbeatReply.pending > 0`). The CoAP server polls for inbound requests during this window. |
| `CONFIG_TW_SLEEP_LISTEN_WINDOW_IDLE_S` | int | 5 | Shorter listen window used when `HeartbeatReply.pending == 0`. **Set to 0 to sleep immediately** after the heartbeat with no listen window (no inbound requests will be processed). Non-zero = seconds to poll before sleeping. |
| `CONFIG_TW_SLEEP_LISTEN_EXTEND_ON_CMD` | bool | n | Extend listen window on command receipt. When enabled, each processed inbound CoAP request resets the listen window timer back to `TW_SLEEP_LISTEN_WINDOW_S`, keeping the device awake as long as the hub is actively sending commands. |

## OTA

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_TW_OTA_ENABLED` | bool | y | Enable push-based OTA updates via Block1 transfers. |
| `CONFIG_TW_OTA_VERIFY_SIGNATURE` | bool | y | Require SHA-256 hash verification on firmware images. **Disabling allows unauthenticated firmware to be flashed** -- only disable for development. |
| `CONFIG_TW_OTA_ROLLBACK_TIMEOUT_S` | int | 60 | Seconds after boot to confirm new firmware. If `svc_ota_mark_valid()` is not called within this window, the bootloader rolls back to the previous image on next reboot. |

## Identity

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_TW_IDENTITY_VENDOR_ID` | int | 0 | Compile-time default vendor ID. **0 = unknown/generic/default.** Non-zero values identify the OEM. Can be overridden during factory provisioning. |
| `CONFIG_TW_IDENTITY_PRODUCT_ID` | int | 0 | Compile-time default product ID. **0 = unknown/generic/default.** Non-zero values identify the product line. Can be overridden during factory provisioning. |
| `CONFIG_TW_IDENTITY_KEY_TYPE_DEFAULT` | int | 0 | Default identity key type. **0** = no signing (cleartext, no key material). **1** = Ed25519 (RFC 8032; 32-byte key pair; public key exportable via `GET /tw/identity/pubkey`). **2** = PSK (symmetric HMAC-SHA256; 32-byte pre-shared key). The actual key material must be provisioned separately via factory provisioning. |

## NVS Security

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_TW_NVS_ENCRYPT` | bool | n | Enable NVS encryption on ESP32 (requires flash encryption + `nvs_key` partition in the partition table). When enabled, uses `nvs_flash_secure_init()`. **When disabled, identity keys and WiFi credentials are stored in plaintext flash.** On POSIX builds this has no effect (file permissions are always restricted to 0600). |

## Hub Communication

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_TW_HEARTBEAT_INTERVAL_S` | int | 60 | Heartbeat period in **always-on mode** (seconds). In deep-sleep mode, the wake interval (`TW_SLEEP_INTERVAL_S`) controls heartbeat timing instead. |
| `CONFIG_TW_HEARTBEAT_ON_BOOT` | bool | y | Send heartbeat immediately on boot (before the first timer-driven heartbeat). |

## Protocol

| Option | Description |
|--------|-------------|
| `CONFIG_TW_PROTOCOL_COAP` | CoAP (RFC 7252) via libcoap -- **implemented; default**. |
| `CONFIG_TW_PROTOCOL_MQTT` | MQTT -- **not yet implemented**; selecting this produces a compile-time error. |

### CoAP Settings

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_TW_COAP_PORT` | int | 5683 | CoAP server UDP port. **5683** is the IANA-assigned CoAP port (RFC 7252 section 12.2). Change only if another service occupies this port. |
| `CONFIG_TW_COAP_MAX_RESOURCES` | int | 16 | Maximum registered CoAP resources. Increase if your application registers many custom endpoints. |
| `CONFIG_TW_COAP_BLOCK_SIZE` | int | 1024 | Block transfer chunk size in bytes (RFC 7959). Must be a power of 2 in the range 16..1024. Larger values mean fewer round-trips but more RAM per block. |

## Sensor Telemetry

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_TW_SENSOR_PUSH` | bool | n | Push sensor readings to hub. Master switch for both in-heartbeat and dedicated telemetry modes. |
| `CONFIG_TW_SENSOR_PUSH_IN_HEARTBEAT` | bool | y | Include sensor readings in the `HeartbeatPayload.sensors` repeated field. Zero extra round-trips. Depends on `TW_SENSOR_PUSH`. **Can be enabled simultaneously** with `TW_SENSOR_PUSH_DEDICATED`. |
| `CONFIG_TW_SENSOR_PUSH_DEDICATED` | bool | n | Push to a dedicated telemetry endpoint on its own schedule, independent of the heartbeat interval. Depends on `TW_SENSOR_PUSH`. |
| `CONFIG_TW_SENSOR_PUSH_INTERVAL_S` | int | 10 | Dedicated telemetry push interval in seconds. Must be > 0. **The hub can override this at runtime** via `TelemetryReply.next_interval_s` (clamped to 1..3600). Depends on `TW_SENSOR_PUSH_DEDICATED`. |
| `CONFIG_TW_SENSOR_PUSH_PATH` | string | `"/hub/telemetry"` | Hub endpoint for dedicated telemetry. Depends on `TW_SENSOR_PUSH_DEDICATED`. |

## Applet Runtime

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_TW_APPLET_ENABLED` | bool | n | Enable WASM applet runtime. Adds WAMR dependency (~50 KB code footprint). |
| `CONFIG_TW_APPLET_FLASH_PARTITION` | string | `"applet"` | Flash partition name for applet storage. Depends on `TW_APPLET_ENABLED`. |
| `CONFIG_TW_APPLET_MAX_SIZE` | int | 65536 | Maximum applet binary size in bytes. Depends on `TW_APPLET_ENABLED`. |
| `CONFIG_TW_WASM_STACK_SIZE` | int | 8192 | WASM interpreter stack size in bytes. Depends on `TW_APPLET_ENABLED`. |
| `CONFIG_TW_WASM_HEAP_SIZE` | int | 32768 | WASM interpreter heap size in bytes. Depends on `TW_APPLET_ENABLED`. |

## ESP-IDF PAL

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `CONFIG_TW_PAL_I2C_BUS` | int | 0 | Default I2C bus number. |
| `CONFIG_TW_PAL_I2C_SDA` | int | 6 | Default I2C SDA pin. |
| `CONFIG_TW_PAL_I2C_SCL` | int | 7 | Default I2C SCL pin. |
| `CONFIG_TW_PAL_I2C_FREQ` | int | 100000 | Default I2C frequency (Hz). |
