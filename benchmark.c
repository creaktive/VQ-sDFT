#include <stdio.h>
#include <time.h>

#include "12tet.h"
#include "vqsdft.h"

#define SAMPLE_RATE 8000
#define BLOCK_SIZE 80

int main(void) {
  FreqBand bands[MAX_BANDS];
  generate_12tet_bands(bands, 36, MAX_BANDS);

  double window[2] = {1.0, 0.5};

  VQsDFT dft_instance;

  vqsdft_init(&dft_instance, bands, MAX_BANDS, window, 2,
              50.0, // temporal smoothing window in ms
              SAMPLE_RATE);

  int32_t q16_samples[BLOCK_SIZE];

  struct timespec start, end;
  timespec_get(&start, TIME_UTC);
  int i;
  for (i = 0; i < BLOCK_SIZE * 1000; i++) {
    int j = i % BLOCK_SIZE;
    double float_sample = sin(2.0 * M_PI * 440.0 * i / SAMPLE_RATE);
    q16_samples[j] = FLOAT_TO_Q16(float_sample);
    if (j == BLOCK_SIZE - 1)
      vqsdft_analyze_block(&dft_instance, q16_samples, BLOCK_SIZE, true);
  }
  timespec_get(&end, TIME_UTC);

  long seconds = end.tv_sec - start.tv_sec;
  long nanoseconds = end.tv_nsec - start.tv_nsec;
  double elapsed_ms =
      ((double)seconds * 1000.0) + ((double)nanoseconds / 1000000.0);
  printf("benchmark: %.0f samples per second\n\n", i / (elapsed_ms / 1000.0));

  for (i = 0; i < dft_instance.num_coeffs; i++)
    printf("band %d\t(%.2f Hz):\t%f\n", i, bands[i].ctr,
           Q16_TO_FLOAT(dft_instance.spectrum_data[i]));

  return 0;
}
