# Build blinky.wasm from freestanding C.
# Requires clang with wasm32 target support.

CC = clang
CFLAGS = --target=wasm32 -nostdlib -O2
LDFLAGS = -Wl,--no-entry \
          -Wl,--export=applet_init \
          -Wl,--export=applet_tick \
          -Wl,--export=applet_on_command

all: blinky.wasm

blinky.wasm: blinky.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	rm -f blinky.wasm

.PHONY: all clean
