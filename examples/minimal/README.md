# Minimal Example

The simplest possible device using the TW Device SDK: one sensor, one LED, one CoAP resource, ~60 lines of C.

## What it does

- Blinks an LED slowly
- Reads a fake temperature sensor
- Exposes `GET /tw/sensor/temperature` via CoAP

## Build & run

```bash
cmake -B build -DPAL_BACKEND=posix
cmake --build build
./build/minimal
```

## Query

```bash
tw coap send get /tw/sensor/temperature
```

## Use as a template

This is the best starting point if you want to build something from scratch without the thermostat's state machine complexity.
