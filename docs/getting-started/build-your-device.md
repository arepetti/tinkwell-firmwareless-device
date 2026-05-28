# Build Your Own Device

This tutorial walks through turning the thermostat example into your own device -- say, an irrigation controller.

## 1. Start from the thermostat

```bash
mkdir my-irrigation && cd my-irrigation
git init

# Add the SDK as a submodule (or copy it)
git submodule add https://github.com/arepetti/tw-device-sdk.git sdk

# Copy the thermostat skeleton
cp -r sdk/../examples/thermostat/* .
```

## 2. Rename things

- Edit `CMakeLists.txt`: change `project(thermostat C)` to `project(irrigation C)` and the executable name
- Edit `host/main.c`: change `config.name` to `"irrigation-controller"`

## 3. Replace the state machine

Open `app/src/thermostat.c` and replace the contents with your logic.
The structure stays the same:

- An `init` function that registers sensors and sets up GPIO
- A `tick` function that reads sensors and applies your logic
- CoAP handlers for the resources you want to expose

For an irrigation controller you might have:

```c
tw_err_t irrigation_tick(const tw_device_config_t *dev) {
    int32_t moisture = tw_sensor_read_int("moisture");

    if (mode == MODE_AUTO) {
        valve_open = (moisture < MOISTURE_THRESHOLD);
    }

    pal_gpio_write(PIN_VALVE, valve_open);
    return TW_OK;
}
```

## 4. Define your CoAP resources

In `host/main.c`, change the resource table:

```c
static tw_msg_resource_t resources[] = {
    { "/tw/sensor/moisture", TW_MSG_GET, on_get_moisture },
    { "/tw/valve",           TW_MSG_GET | TW_MSG_PUT, on_valve },
    { "/tw/status",          TW_MSG_GET, on_get_status },
    TW_MSG_RESOURCE_END
};
```

## 5. Adjust pins

Edit `app/include/thermostat_pins.h` (rename it to `irrigation_pins.h`):

```c
#define PIN_VALVE         3
#define PIN_LED_STATUS    1
#define PIN_MOISTURE_PWR  4
```

## 6. Build and test

On POSIX, the fake I2C layer reads `TW_FAKE_TEMP` and `TW_FAKE_HUMID` for synthetic readings.
Set those to match what your logic expects.

```bash
cmake -B build -DPAL_BACKEND=posix
cmake --build build
TW_FAKE_TEMP=300 ./build/irrigation
```

Query it:

```bash
tw coap send get /tw/sensor/moisture
tw coap send put /tw/valve -d "1"
```

## 7. Build for hardware

```bash
idf.py set-target esp32c6
idf.py build
idf.py flash monitor
```

See the [ESP-IDF guide](../guides/esp-idf.md) for full details.

## What you did NOT have to write

- Network initialisation
- CoAP server setup
- OTA firmware updates
- Hub heartbeat/mailbox
- Power management
- BLE provisioning
- NVS storage
- Logging

## Alternative: use applets instead

If you would rather write your logic in TypeScript, Rust, or another language and push it to the device at runtime, see [Choosing your approach](../guides/choosing-your-approach.md) and [Writing applets](../guides/writing-applets.md).
