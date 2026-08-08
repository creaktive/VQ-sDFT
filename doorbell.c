#include <stdio.h>
#include <time.h>

#include "input.h"
#include "vqsdft.h"

#define SAMPLE_RATE 8000 // Hz - matches original doorbell.cpp
#define BLOCK_SIZE 64    // samples per block = 8 ms latency
#define TIME_RES 600.0f  // vqsdft time resolution (converged ≥400)
#define COOLDOWN 10      // seconds between notifications

// EMA smoothing: alpha=0.008 → ~125-block time constant (~1.5s at 64
// samples/block)
#define ALPHA 0.008f
#define BANDS 4

int main(int argc, char *argv[]) {
  input_open(argc == 2 ? argv[1] : NULL, SAMPLE_RATE);

  float window[2] = {1.0, 0.5};

  FreqBand bands[] = {
      // downstairs melody - peaks at 567 Hz and 729 Hz (measured)
      {563.f, 567.f, 571.f}, // low  ±4 Hz around measured peak
      {725.f, 729.f, 733.f}, // high ±4 Hz around measured peak
      // upstairs melody - peaks at 489 Hz and 978 Hz (measured)
      {485.f, 489.f, 493.f}, // low  ±4 Hz around measured peak
      {974.f, 978.f, 982.f}, // high ±4 Hz around measured peak
  };

  VQsDFT dft_instance;
  vqsdft_init(&dft_instance, bands, BANDS, window, 2, TIME_RES, SAMPLE_RATE);

  float samples[BLOCK_SIZE];
  float ema[4] = {0}; // EMA for each band
  time_t ignore_until = 0;

  while (1) {
    time_t now = time(NULL);

    ssize_t len;
    if ((len = input_read(samples, BLOCK_SIZE)) != BLOCK_SIZE) {
      if (len == 0)
        break;
      continue;
    }

    vqsdft_analyze_block(&dft_instance, samples, BLOCK_SIZE);

    // Update EMA for each band
    for (int i = 0; i < BANDS; i++) {
      ema[i] = ALPHA * dft_instance.spectrum_data[i] + (1.0f - ALPHA) * ema[i];
    }

    if (now < ignore_until)
      continue;

    float d_sum = ema[0] + ema[1]; // downstairs pair
    float u_sum = ema[2] + ema[3]; // upstairs pair

    // Downstairs: mutual exclusion (>3x), both bands present, roughly equal
    // share
    int d_ok = 0;
    if (d_sum >= 0.001f && u_sum < d_sum / 3.0f) {
      float b0_share = ema[0] / (d_sum + 0.000001f);
      float b1_share = ema[1] / (d_sum + 0.000001f);
      if (ema[0] >= 0.0003f && ema[1] >= 0.0003f && b0_share < 0.7f &&
          b1_share < 0.7f)
        d_ok = 1;
    }

    // Upstairs: mutual exclusion (>3x), band 3 dominant, band 2 present
    int u_ok = 0;
    if (u_sum >= 0.001f && d_sum < u_sum / 3.0f) {
      float b3_share = ema[3] / (u_sum + 0.000001f);
      if (ema[3] >= 0.0005f && ema[2] >= 0.0001f && b3_share > 0.6f)
        u_ok = 1;
    }

    if (d_ok) {
      ignore_until = now + COOLDOWN;
      printf("DOWNSTAIRS DOORBELL\n");
    } else if (u_ok) {
      ignore_until = now + COOLDOWN;
      printf("UPSTAIRS DOORBELL\n");
    }
  }

  input_close();
  return 0;
}
