# Changelog

All notable changes to the Tinkwell Firmwareless Device SDK are documented in this file.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
This project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-03-28

### Added

#### Core SDK
- Three-layer architecture: Application, Services, Platform Abstraction Layer (PAL)
- Protocol abstraction layer with vtable-based backend selection
- CoAP protocol backend (binary RFC 7252 via libcoap, with Block1/Block2 per RFC 7959)
- MQTT backend stub with Kconfig option and compile-time error guard
- Heartbeat service with kvtext payload format
- Mailbox command dispatch (hub-to-device: SET_CONFIG, OTA_AVAILABLE, REBOOT, SET_IDENTITY, APP)
- Sensor registry with named sensor model and poll callback
- Configuration get/set with NVS persistence
- Graceful shutdown via `tw_device_request_shutdown()`

#### Identity and Provisioning
- Two-phase provisioning: factory (identity) and hub (network + UUID)
- BLE GATT provisioning service (ESP-IDF, full 128-bit service UUID)
- SoftAP + CoAP provisioning (ESP-IDF, auto-generated AP SSID)
- LAN CoAP provisioning (ESP-IDF + POSIX)
- POSIX environment variable provisioning for development
- Device identity: UUID, vendor/product ID, display names, variant, serial number
- Ed25519 public key export via CoAP endpoint
- PSK (SHA-256) and Ed25519 identity verification (Kconfig-selectable)
- kvtext wire format: simple key=value text protocol (128-byte max line)

#### OTA Updates
- Push-based OTA via CoAP (begin/block/commit/status)
- SHA-256 image verification (Kconfig-gated)
- A/B partition support with rollback
- Progress tracking and state machine

#### WASM Applets
- Applet push/store/load service (CoAP endpoints)
- Flash partition storage with optional XIP memory mapping
- WAMR runtime integration in applet-device example
- Example applets in AssemblyScript, Rust, C (freestanding), and WAT

#### Power Management
- Always-on mode with configurable heartbeat interval
- Deep sleep mode with configurable sleep/listen windows
- GPIO wake source support
- Integrated sleep cycle in main loop

#### Platform Support
- ESP-IDF PAL (ESP32-C3, ESP32-C6): GPIO, I2C, NVS, WiFi, BLE, flash, OTA, crypto
- POSIX PAL: file-based NVS, UDP sockets, pthreads, fake sensors
- Mock PAL for unit testing
- DHCP with static IP fallback (Kconfig defaults)

#### Thread Safety
- Optional mutex protection via `tw_lock.h`
- Kconfig option `CONFIG_TW_THREAD_SAFETY` (default: enabled)

#### Security
- NVS encryption: ESP32 encrypted partitions, POSIX chmod 0600
- OTA SHA-256 verification
- Ed25519 key storage and public key derivation (PSA Crypto on ESP-IDF)

#### Transport Layer
- WiFi transport (ESP-IDF)
- Ethernet transport (ESP-IDF)
- Thread (OpenThread) transport (ESP-IDF)
- BLE transport (ESP-IDF)
- POSIX transport (loopback/localhost)

#### Examples
- Thermostat: temperature monitoring, safety limits, mode control (off/on/auto)
- Applet device: WASM runtime with hub-pushed logic
- Minimal: bare-minimum SDK usage template
- Factory: dedicated factory provisioning partition

#### Scripts (`device/scripts/`)
- `provision.py`: interactive provisioning CLI (LAN, SoftAP, BLE modes)
- `demo.sh` / `demo.ps1`: one-command full lifecycle demo
- `fake_hub.py`: development hub simulator (wraps `tw coap server`)
- `fake_sensor.py`: fake I2C sensor data feeder
- `build_flash.sh` / `build_flash.ps1`: unified ESP-IDF build + flash
- `qemu_run.py`: QEMU emulator runner

#### Documentation
- Wire specification: canonical protocol reference for hub implementers
- Architecture overview with layer diagram
- Hub protocol (heartbeat, mailbox, commands)
- OTA protocol (begin/block/commit/status)
- Applet protocol (push/commit/status)
- Provisioning guide (BLE, SoftAP, LAN, POSIX)
- Getting started guide (5-minute walkthrough)
- Build your own device tutorial
- ARM Cortex-M porting notes
- Kconfig reference
- API reference
- Tools reference (man-style docs)
- Doxygen-style documentation on all public headers

### Known Limitations
- MQTT backend is a compile-time stub (not implemented)
- PSK rotation protocol with hub is deferred
- Ed25519 signing path is deferred (only public key export implemented)
- POSIX Ed25519 derivation uses a placeholder (not cryptographically correct)
- `fake_hub.py` now wraps `tw coap server --mailbox` (binary CoAP); requires `tw` CLI on PATH
- DTLS is not yet enabled (libcoap supports it; can be wired later)
- No CI pipeline (SDK is designed as a Git submodule; CI belongs in consuming project)
