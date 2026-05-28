# Testing Guide

## Test Levels

### 1. Unit Tests (mock PAL, no I/O)

```bash
cmake -B build-test -DPAL_BACKEND=mock -DBUILD_TESTS=ON
cmake --build build-test
ctest --test-dir build-test -V
```

Tests:
- `test_thermostat.c` -- state machine transitions, safety overrides
- SDK tests -- CoAP dispatch, heartbeat, OTA state, button, safety

### 2. Native Integration Tests (POSIX PAL)

```bash
# Terminal 1: start the device
cmake -B build -DPAL_BACKEND=posix && cmake --build build
./build/thermostat &

# Terminal 2: run integration tests
pytest test/integration/ -v
```

Tests:
- `test_thermostat_coap.py` -- query all CoAP resources
- `test_thermostat_ota.py` -- OTA push flow

### 3. QEMU Integration Tests

```bash
cd esp-idf
idf.py set-target esp32c3
idf.py build

python ../../scripts/qemu_run.py \
    --project . --forward 5683 \
    --test "pytest ../test/integration/ -v"
```

### 4. Hardware Tests

Flash to a real board and run integration tests against the physical device's IP address:

```bash
TW_DEVICE_HOST=192.168.1.42 pytest test/integration/ -v
```

## Simulating Sensor Data

```bash
# Static temperature (tenths of C):
TW_FAKE_TEMP=180 ./build/thermostat

# Dynamic: use fake_sensor.py
python ../../scripts/fake_sensor.py --wave --period 30

# In another terminal:
./build/thermostat
```

## Simulating the Hub

```bash
# Start the fake hub:
python ../../scripts/fake_hub.py -v

# The device sends heartbeats to the hub automatically.
# Queue a command:
python ../../scripts/fake_hub.py -v \
    --queue "cmd:128:mode=on"
```

## Test Coverage

To measure coverage (GCC):

```bash
cmake -B build-cov -DPAL_BACKEND=mock -DBUILD_TESTS=ON \
      -DCMAKE_C_FLAGS="--coverage"
cmake --build build-cov
ctest --test-dir build-cov
gcovr --root . --html coverage.html
```
