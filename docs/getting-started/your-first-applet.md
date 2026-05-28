# Your First Applet

This guide gets a WASM applet running on a device in five minutes.
No C code, no hardware.
You will build an AssemblyScript applet, load it into the applet-device runtime, and see it execute.

## Prerequisites

- Node.js 16+ (for AssemblyScript)
- GCC or Clang + CMake 3.16+ (to build the device runtime)
- Linux, macOS, or WSL

## 1. Clone

```bash
git clone --recursive https://github.com/arepetti/tinkwell-firmwareless-device.git
cd tinkwell-firmwareless-device
```

## 2. Build the device runtime

The `applet-device` example is a device that runs WASM applets instead of compiled C logic:

```bash
cd examples/applet-device
cmake -B build -DPAL_BACKEND=posix
cmake --build build
cd ../..
```

## 3. Build the blinky applet (AssemblyScript)

```bash
cd examples/applet-device/applets/blinky-as
npm install
npm run build
cd ../../../..
```

This produces `examples/applet-device/applets/blinky-as/build/blinky.wasm`.

## 4. Load the applet

The POSIX runtime reads the applet from `~/.tw-device/applet.bin`:

```bash
mkdir -p ~/.tw-device
cp examples/applet-device/applets/blinky-as/build/blinky.wasm \
   ~/.tw-device/applet.bin
```

## 5. Run

```bash
./examples/applet-device/build/applet_device
```

You should see the device boot, load the applet, and start calling `applet_tick()` every second.
The blinky applet toggles a GPIO pin on each tick.

## 6. Hot-swap: load a different applet

Build the thermostat applet (also AssemblyScript):

```bash
cd examples/applet-device/applets/thermostat-as
npm install
npm run build
cd ../../../..

cp examples/applet-device/applets/thermostat-as/build/thermostat.wasm \
   ~/.tw-device/applet.bin
```

Restart the runtime and you will see the thermostat logic running instead -- reading sensors, applying safety overrides, and controlling a relay.
Same device runtime, different behavior, no recompilation.

## What just happened?

The device runtime provides the hardware abstraction (sensors, GPIO, LEDs, CoAP, heartbeat) while the applet provides the application logic.
The two connect through the [Host API](../reference/host-api.md): the applet imports functions like `tw_host_read_sensor_sn` and `tw_host_write_gpio`.

In production, the hub pushes `.wasm` binaries to the device over CoAP.
The device stores them in flash and hot-swaps without rebooting.
See the [applet runtime](../architecture/applet-runtime.md) for details.

## Other languages

The same applet contract works in any language that compiles to `wasm32`:

| Language | Example | Build command |
|----------|---------|---------------|
| AssemblyScript | `applets/blinky-as/` | `npm run build` |
| Rust | `applets/blinky-rs/` | `cargo build --target wasm32-unknown-unknown --release` |
| C (freestanding) | `applets/blinky-c/` | `make` |
| WAT (hand-written) | `applets/blinky-wat/` | `wat2wasm blinky.wat -o blinky.wasm` |

See [Writing applets](../guides/writing-applets.md) for language-specific guides and the full contract specification.

## Next steps

- [Writing applets](../guides/writing-applets.md) -- full contract, language guides
- [Host API reference](../reference/host-api.md) -- all functions available to applets
- [Choosing your approach](../guides/choosing-your-approach.md) -- when to use applets vs native C
- [Quick start](quick-start.md) -- the native C path if you prefer compiled firmware
