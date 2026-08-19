# VQ-sDFT — Agent Notes

## Build

```sh
make                    # Linux: benchmark + music-spectrum (needs libasound)
make clean              # removes *.d, *.o, benchmark, music-spectrum
make pretty             # clang-format -i on all .c/.h
```

Pico build requires `pico-sdk` and `picotool`:

```sh
cmake -DPICO_SDK_PATH=.../pico-sdk -DPICO_BOARD=pico2 -B build
cmake --build build
```

## Architecture

- **Core library:** `vqsdft.c/h` — static allocation only (`MAX_BANDS=64`, `BUFFER_SIZE=8192`). No dynamic memory.
- **Helper:** `12tet.c/h` — generates 12-TET frequency bands from a MIDI note.
- **Input abstraction:** `input.h` with `input-alsa.c` (Linux ALSA) and `input-file.c`.
- **Entry points:** `benchmark.c` (throughput, infinite loop), `music-spectrum.c` (ALSA live audio → hex spectrum).

## Gotchas

- `BUFFER_SIZE` must be a power of two (uses bitmask wrap-around).
- All buffers are stack/static sized at compile time — changing `MAX_BANDS`, `MAX_KERNEL_LEN`, or `BUFFER_SIZE` affects all modules.
- `music-spectrum` defaults to ALSA device `plug:dsnoop`; pass a custom device as argument.
- Pico build defines `TARGET_PICO`, which swaps the timer and stdio setup in `benchmark.c`.
