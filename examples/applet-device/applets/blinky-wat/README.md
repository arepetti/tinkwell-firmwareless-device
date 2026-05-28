# Blinky Applet (WAT -- WebAssembly Text Format)

The same blinky logic, hand-written in raw WASM text format.
This is a teaching tool: it shows exactly what the WASM ABI looks like at the lowest level, with every instruction annotated.

## Build

```bash
wat2wasm blinky.wat -o blinky.wasm
```

Requires [wabt](https://github.com/WebAssembly/wabt) (the WebAssembly Binary Toolkit).

## What you see

The ~50 lines of WAT show:

1. **Imports**: `tw_host_write_gpio` and `tw_host_log_s` from the `"env"` module -- these are the host functions the device provides.

2. **Memory**: A single 64 KB page with a string literal stored at offset 0.

3. **Global**: A mutable `i32` for the LED state.

4. **Exports**: `applet_init`, `applet_tick`, `applet_on_command` -- the same three entry points that every applet must provide, regardless of source language.

## Size

The assembled `blinky.wasm` is ~120 bytes.
