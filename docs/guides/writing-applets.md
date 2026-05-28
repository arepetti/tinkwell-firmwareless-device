# Writing Applets

An applet is a `.wasm` binary that exports three functions and imports host functions from the `"env"` module.
Any language that compiles to `wasm32` works.

## The contract

### Your applet must export

| Function | Signature | When called |
|----------|-----------|-------------|
| `applet_init` | `() -> void` | Once, at load time |
| `applet_tick` | `() -> void` | Every tick interval |
| `applet_on_command` | `(i32, i32, i32) -> void` | On hub command |

### Your applet may import

Any function from the [Host API](../reference/host-api.md).
All imports come from the `"env"` module.

## Language guides

### AssemblyScript

Use `@external("env", "function_name")` to declare imports and `export function` for exports:

```typescript
@external("env", "tw_host_write_gpio")
declare function tw_host_write_gpio(pin: i32, value: i32): void;

export function applet_tick(): void {
    tw_host_write_gpio(1, 1);
}
```

Build: `npx asc assembly/index.ts -o build/applet.wasm --optimize`

### Rust

Use `extern "C"` for imports, `#[no_mangle] pub extern "C"` for exports.
Target `wasm32-unknown-unknown` with `#![no_std]`:

```rust
#![no_std]

extern "C" {
    fn tw_host_write_gpio(pin: i32, value: i32);
}

#[no_mangle]
pub extern "C" fn applet_tick() {
    unsafe { tw_host_write_gpio(1, 1); }
}
```

Build: `cargo build --target wasm32-unknown-unknown --release`

### C (freestanding)

Declare imports as `extern`, export by name.
Compile with `clang --target=wasm32 -nostdlib`:

```c
extern void tw_host_write_gpio(int pin, int value);

void applet_tick(void) {
    tw_host_write_gpio(1, 1);
}
```

Build:
```bash
clang --target=wasm32 -nostdlib -Wl,--no-entry \
      -Wl,--export=applet_init \
      -Wl,--export=applet_tick \
      -Wl,--export=applet_on_command \
      -o applet.wasm applet.c
```

### WAT (WebAssembly Text)

Write the instructions directly.
See [`blinky.wat`](../../examples/applet-device/applets/blinky-wat/blinky.wat) for an annotated example.

Build: `wat2wasm applet.wat -o applet.wasm`

## Testing locally

1. Build the applet-device example with POSIX backend
2. Place your `.wasm` file at `$HOME/.tw-device/applet.bin`
3. Run the applet-device binary -- it will load the applet from "flash"

```bash
cp build/my_applet.wasm ~/.tw-device/applet.bin
./build/applet_device
```

## Size guidelines

- Keep applets under 64 KB (configurable via `CONFIG_TW_APPLET_MAX_SIZE`)
- The blinky examples are ~120--200 bytes
- The thermostat applet is ~2--4 KB depending on optimisation
- WAMR interpreter overhead: ~50 KB code + stack + heap from Kconfig

## Related documentation

- [Your first applet](../getting-started/your-first-applet.md) -- 5-minute walkthrough
- [Host API reference](../reference/host-api.md) -- all available host functions
- [Applet runtime](../architecture/applet-runtime.md) -- how WAMR, flash, and lifecycle work
- [Applet protocol](../protocol/applet-protocol.md) -- push and commit over CoAP
- [Choosing your approach](choosing-your-approach.md) -- applets vs native C vs state machines
