# Blinky Applet (C, freestanding)

Same blinky in plain C.  No WASI, no SDK, no dependencies.
Just a C compiler that can target wasm32.

## Build

```bash
make
```

Or manually:

```bash
clang --target=wasm32 -nostdlib -Wl,--no-entry \
      -Wl,--export=applet_init \
      -Wl,--export=applet_tick \
      -Wl,--export=applet_on_command \
      -o blinky.wasm blinky.c
```

Produces `blinky.wasm` (~200 bytes).
