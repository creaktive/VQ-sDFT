#include <stdio.h>

#include "12tet.h"
#include "vqsdft.h"

#define SAMPLE_RATE 8000
#define BLOCK_SIZE 80
#define BANDS 61

#ifdef TARGET_PICO

#include "pico/stdlib.h"
#include "pico/time.h"

#define ITERATIONS 1000

uint64_t start;

void benchmark_start(void) { start = time_us_64(); }

void benchmark_end(int i) {
  uint64_t elapsed_us = time_us_64() - start;
  double elapsed_ms = elapsed_us / 1000.0;
  printf("benchmark: %.0f samples per second\n", i / (elapsed_ms / 1000.0));
}

#else

#include <time.h>

#define ITERATIONS 10000

struct timespec start;

void benchmark_start(void) { timespec_get(&start, TIME_UTC); }

void benchmark_end(int i) {
  struct timespec end;
  timespec_get(&end, TIME_UTC);
  long seconds = end.tv_sec - start.tv_sec;
  long nanoseconds = end.tv_nsec - start.tv_nsec;
  double elapsed_ms =
      ((double)seconds * 1000.0) + ((double)nanoseconds / 1000000.0);
  printf("benchmark: %.0f samples per second\n", i / (elapsed_ms / 1000.0));
}

#endif

int main(void) {
#ifdef TARGET_PICO
  stdio_init_all();
#endif

  FreqBand bands[BANDS];
  generate_12tet_bands(bands, 36, BANDS, 0.0);

  double window[2] = {1.0, 0.5};

  VQsDFT dft_instance;
  vqsdft_init(&dft_instance, bands, BANDS, window, 2,
              100.0, // temporal smoothing window in ms
              SAMPLE_RATE);

  int32_t q16_samples[BLOCK_SIZE];

  while (true) {
    benchmark_start();
    int i;
    for (i = 0; i < BLOCK_SIZE * ITERATIONS; i++) {
      int j = i % BLOCK_SIZE;
      double float_sample = sin(2.0 * M_PI * 440.0 * i / SAMPLE_RATE);
      q16_samples[j] = FLOAT_TO_Q16(float_sample);
      if (j == BLOCK_SIZE - 1)
        vqsdft_analyze_block(&dft_instance, q16_samples, BLOCK_SIZE);
    }
    benchmark_end(i);

    continue;

    printf("\n");
    for (i = 0; i < dft_instance.num_coeffs; i++)
      printf("band %d\t(%.2f Hz):\t%f\n", i, bands[i].ctr,
             Q16_TO_FLOAT(dft_instance.spectrum_data[i]));
  }

  return 0;
}
