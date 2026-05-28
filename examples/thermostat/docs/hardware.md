# Hardware Guide

## Recommended Boards

| Board | Chip | WiFi | BLE | Thread | Notes |
|-------|------|------|-----|--------|-------|
| ESP32-C6-DevKitC-1 | ESP32-C6 | WiFi 6 | 5.0 | Yes | Primary target, full features |
| ESP32-C3-DevKitM-1 | ESP32-C3 | WiFi 4 | 5.0 | No | Best QEMU support |
| ESP32-H2-DevKitM-1 | ESP32-H2 | No | 5.0 | Yes | Thread-only, low power |

## Pin Assignments

Default pins (override in `thermostat_pins.h`):

| Function | GPIO | Notes |
|----------|------|-------|
| Button (mode cycle) | 9 | Internal pull-up, active low |
| LED (mode indicator) | 8 | Active high |
| LED (relay indicator) | 10 | Active high |
| Relay control | 4 | Active high, drives MOSFET gate |
| I2C SDA | 6 | For SHT30/BME280 sensor |
| I2C SCL | 7 | 100 kHz default |

## Wiring Diagram

```
                  ESP32-C6-DevKitC
                  ┌──────────────┐
                  │              │
    [Button] ──── │  GPIO 9      │
                  │              │
    [LED1]  ←──── │  GPIO 8      │
    [LED2]  ←──── │  GPIO 10     │
                  │              │
    [Relay] ←──── │  GPIO 4      │───→ [MOSFET] ──→ [Heater]
                  │              │
    [SHT30] ───── │  GPIO 6 SDA  │
            ───── │  GPIO 7 SCL  │
                  │              │
                  │  3.3V / GND  │
                  └──────────────┘
```

## BOM (Bill of Materials)

| Component | Part | Qty | Notes |
|-----------|------|-----|-------|
| MCU board | ESP32-C6-DevKitC-1 | 1 | |
| Sensor | SHT30-DIS-B | 1 | I2C temp+humidity |
| Button | 6mm tactile | 1 | Normally open |
| LED (green) | 3mm | 1 | Mode indicator |
| LED (red) | 3mm | 1 | Relay/heating indicator |
| Resistor | 330Ω | 2 | LED current limiting |
| Resistor | 10kΩ | 1 | Button pull-up (optional, internal used) |
| MOSFET | IRLZ44N | 1 | Relay driver |
| Relay/SSR | 5V | 1 | Heater switching |
| Power | USB-C cable | 1 | Dev board power |

## Sensor Alternatives

The PAL I2C interface is generic.
To use a different sensor:

1. Edit `thermostat.c` to change the I2C address and register map.
2. Or write a thin sensor driver and call it from `thermostat_tick()`.

Tested sensors:
- **SHT30**: I2C addr 0x44, temp+humidity
- **BME280**: I2C addr 0x76, temp+humidity+pressure
- **Si7021**: I2C addr 0x40, temp+humidity
