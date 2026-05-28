# Blinky Applet (Rust)

A trivial LED blinker written in Rust, compiled to WASM.

## Build

```bash
cargo build --target wasm32-unknown-unknown --release
```

Produces `target/wasm32-unknown-unknown/release/blinky.wasm`.

## What it does

Toggles GPIO pin 1 on every tick (once per second).
That's it.

## How it works

Uses `extern "C"` to import host functions and `#[no_mangle]` to export the applet entry points.
Built with `#![no_std]` -- no standard library, no allocator, no WASI.
