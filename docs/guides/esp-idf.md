# ESP-IDF Guide

This guide covers building, flashing, and monitoring Tinkwell device firmware on real ESP32 hardware using the Espressif IoT Development Framework.

## Prerequisites

- [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) installed and sourced (`. $HOME/esp/esp-idf/export.sh`).
- An ESP32-C6, ESP32-C3, or ESP32-H2 dev board.
- USB cable for flashing.

## Quick start

```bash
cd examples/thermostat/esp-idf

# Pick your target chip.
idf.py set-target esp32c6

# Optional: configure WiFi credentials and other settings.
idf.py menuconfig
# → TW Thermostat → WiFi SSID / WiFi Password

# Build.
idf.py build

# Flash and open the serial monitor.
idf.py -p /dev/ttyUSB0 flash monitor
```

On Windows, replace `/dev/ttyUSB0` with the COM port (e.g. `COM3`).

The output mirrors the POSIX build but runs on real hardware with actual I2C sensors, GPIO, deep sleep, and OTA support.

## Using `menuconfig`

`idf.py menuconfig` opens the Kconfig UI.
SDK options are under **Component config → TW Device SDK**.
Key settings:

| Menu path | What it controls |
|-----------|-----------------|
| TW Device SDK → Protocol | CoAP port, block size, max resources |
| TW Device SDK → Transport | WiFi, Ethernet, Thread, BLE |
| TW Device SDK → Power Management | Deep sleep interval, listen windows |
| TW Device SDK → OTA | Enable/disable, SHA-256 verification |
| TW Device SDK → Applet Runtime | Enable WASM, partition name, size limits |
| TW Device SDK → Identity | Vendor/product IDs, key type |

See the [Kconfig reference](../reference/kconfig.md) for the complete list.

## Partition tables

The device tree includes partition table CSVs for different use cases:

| File | Layout | Use case |
|------|--------|----------|
| `partitions_ota.csv` | NVS + otadata + ota_0 + ota_1 + storage | A/B OTA firmware updates |
| `partitions_applet.csv` | NVS + factory + applet + storage | Single firmware + WASM applet partition |
| `partitions_factory.csv` | NVS + otadata + factory + ota_0 + ota_1 + storage | Split provisioning + OTA |

Select the partition table in your example's `sdkconfig.defaults` or via `menuconfig` → Partition Table → Custom partition table CSV.

## Factory image (split provisioning)

For production devices with separate provisioning and main application partitions, use the build/flash scripts:

```bash
# Linux/macOS
./scripts/build_flash.sh thermostat --factory --target esp32c3
```

```powershell
# Windows (PowerShell)
.\scripts\build_flash.ps1 -Example thermostat -Factory -Target esp32c3
```

This:
1. Builds the factory provisioning app (`factory/`)
2. Builds the main example app
3. Merges both binaries using `esptool.py merge_bin`
4. Flashes the combined image

See [`scripts/README.md`](../../scripts/README.md) for all options.

## Target chips

| Chip | Architecture | Status | Notes |
|------|-------------|--------|-------|
| ESP32-C6 | RISC-V | Primary | WiFi 6, BLE 5, Thread (802.15.4) |
| ESP32-C3 | RISC-V | Tested | WiFi 4, BLE 5, QEMU support |
| ESP32-H2 | RISC-V | Tested | Thread + BLE only (no WiFi) |
| ESP32-S3 | Xtensa LX7 | Untested | Same ESP-IDF PAL; expected to work |

Per-chip overrides go in `sdkconfig.defaults.<chip>` files (e.g. `sdkconfig.defaults.esp32c3`).

## QEMU (no hardware)

ESP-IDF supports QEMU for ESP32-C3.
This is the fastest way to test without a physical board:

```bash
cd examples/thermostat/esp-idf
idf.py set-target esp32c3
idf.py build
idf.py qemu monitor
```

See [QEMU testing](qemu-testing.md) for networking, debugging, and CI integration.

## Checking image size

```bash
idf.py size              # Total image size
idf.py size-components   # Per-component breakdown
```

Keep images under the partition table's allocated app slots.

## Troubleshooting

- **`idf.py` not found**: Run `. $HOME/esp/esp-idf/export.sh` first.
- **Flash fails**: Check the port (`-p`), ensure the board is in download mode (hold BOOT, press RESET on some boards).
- **WiFi won't connect**: Verify SSID/password in `menuconfig` or provision via BLE/SoftAP.
- **Build out of memory**: Disable unused features (BLE, Thread, applet runtime) to reduce flash and RAM usage.

See [Troubleshooting](troubleshooting.md) for more common issues.

## Related documentation

- [Quick start](../getting-started/quick-start.md) -- POSIX build first
- [Kconfig reference](../reference/kconfig.md) -- all compile-time options
- [Provisioning](provisioning.md) -- BLE, SoftAP, LAN setup
- [OTA updates](ota-updates.md) -- push firmware from the hub
