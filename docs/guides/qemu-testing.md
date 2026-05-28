# QEMU Testing

Run the full firmware on an emulated ESP32-C3 with WiFi networking -- no hardware required.

## Why ESP32-C3 for QEMU?

Espressif provides QEMU support for ESP32-C3 (RISC-V).
It emulates the CPU, flash, and a virtual network interface so you can test CoAP communication, OTA, deep sleep wakeup, and GPIO from your development machine.

## Prerequisites

1. **ESP-IDF v5.x** installed and sourced.
2. **qemu-system-riscv32** -- install via:
   - Ubuntu: `sudo apt install qemu-system-misc`
   - macOS: `brew install qemu`
   - Or build from Espressif's fork: <https://github.com/espressif/qemu>

## Quick start

```bash
cd examples/thermostat/esp-idf

# Target must be esp32c3 for QEMU.
idf.py set-target esp32c3
idf.py build

# Launch QEMU with networking.
idf.py qemu monitor
```

The `qemu monitor` command:
- Builds the flash image.
- Starts QEMU with a TAP or user-mode network interface.
- Attaches the IDF monitor for log output.

## Network access

By default QEMU uses user-mode networking.
The device gets IP `10.0.2.15` and the host can reach it via port forwarding:

```bash
# Forward host:15683 → QEMU guest:5683 (CoAP).
idf.py qemu monitor -- \
    -nic user,hostfwd=udp::15683-:5683
```

Then query the thermostat from the host:

```bash
tw coap send get /tw/sensor/temperature --port 15683
```

## TAP networking (full bidirectional)

For full bidirectional access (e.g. the device pushing to a hub on the host), use a TAP interface:

```bash
# Create a TAP interface (Linux).
sudo ip tuntap add dev tap0 mode tap user $(whoami)
sudo ip addr add 192.168.7.1/24 dev tap0
sudo ip link set tap0 up

# Launch QEMU with TAP.
idf.py qemu monitor -- \
    -nic tap,ifname=tap0,script=no,downscript=no
```

## What works in QEMU

| Feature            | Status   |
|--------------------|----------|
| WiFi STA (lwIP)    | Yes      |
| CoAP server        | Yes      |
| NVS read/write     | Yes      |
| OTA (A/B)          | Yes      |
| Deep sleep/wake    | Partial  |
| GPIO input/output  | Stubbed  |
| I2C sensors        | No       |
| BLE                | No       |
| Thread (802.15.4)  | No       |

For I2C sensors, the POSIX build with `TW_FAKE_TEMP` / `TW_FAKE_HUMID` environment variables is faster and more flexible.
Use QEMU when you need to validate the actual FreeRTOS task model, OTA partition swaps, or WiFi stack behaviour.

## Debugging

```bash
# Start QEMU paused, waiting for GDB.
idf.py qemu --gdb monitor

# In another terminal:
riscv32-esp-elf-gdb -ex "target remote :1234" build/thermostat.elf
```

## CI integration

```yaml
# GitHub Actions example.
- name: Build for QEMU
  run: |
    . $IDF_PATH/export.sh
    cd examples/thermostat/esp-idf
    idf.py set-target esp32c3
    idf.py build

- name: Run in QEMU (smoke test)
  run: |
    timeout 30 idf.py qemu monitor || true
```

## Related documentation

- [ESP-IDF guide](esp-idf.md) -- build and flash for real hardware
- [Native development](native-development.md) -- POSIX alternative
- [Troubleshooting](troubleshooting.md) -- QEMU networking issues
