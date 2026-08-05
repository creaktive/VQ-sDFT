# VQ-sDFT

Embedded-friendly, fixed-point (Q16.16) implementation of the Variable-Q
Sliding DFT, for real-time constant-Q spectrum analysis on microcontrollers
(e.g. Raspberry Pi Pico) and desktop Linux.

Port of [TF3RDL's VQ-sDFT](https://gist.github.com/TF3RDL/1810c570bad4f75f77941c981c65723f),
based on ["Application of Improved Sliding DFT Algorithm for Non-Integer k"](https://acoustics.asn.au/conference_proceedings/AAS2021/papers/p6)
by Carl Q. Howard.

Unlike a standard FFT, each frequency band is tracked incrementally as new
samples arrive (an O(1) update per sample per band, not a fixed-size block
transform), and each band's bandwidth/period can be set independently,
enabling constant-Q analysis (e.g. one band per musical semitone) without the
frequency-resolution trade-offs of a fixed-size FFT.

## Files

- `vqsdft.h` / `vqsdft.c` - core algorithm: static, fixed-point sliding DFT
  with configurable per-band frequency ranges. No dynamic allocation; all
  buffers sized at compile time (`MAX_BANDS`, `MAX_KERNEL_LEN`, `BUFFER_SIZE`).
- `12tet.h` / `12tet.c` - helper to generate 12-tone equal temperament
  frequency bands (one per semitone) from a starting MIDI note.
- `benchmark.c` - throughput benchmark (samples/sec), runs standalone on
  Linux or on a Pico via `TARGET_PICO`.
- `music-spectrum.c` - Linux demo that captures live audio via ALSA and
  prints a hex-encoded spectrum bar per line (64 semitone bands from
  C2, 8kHz sample rate).

## Building & Running

Linux (benchmark + ALSA demo):

```sh
make
./benchmark
./music-spectrum [alsa-device] # default device: plug:dsnoop
```

Raspberry Pi Pico (benchmark only, via [pico-sdk](https://github.com/raspberrypi/pico-sdk)):

```sh
cmake -DPICO_SDK_PATH=.../pico-sdk -DPICO_BOARD=pico2 -B build
cmake --build build
picotool load -f build/benchmark.uf2
```
