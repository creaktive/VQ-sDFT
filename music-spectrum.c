#include <stdio.h>
#include <time.h>

#include "12tet.h"
#include "input.h"
#include "vqsdft.h"

#define SAMPLE_RATE 24000
#define BLOCK_SIZE 240
#define BANDS MAX_BANDS

uint8_t mag_to_u8_clamped(float v) {
  if (v >= 1.0f)
    return 255;
  return (uint8_t)(v * 255.0f);
}

void dump_spectrum(const VQsDFT *v, int threshold) {
  static const char HEX_LUT[] = "0123456789abcdef";

  for (int i = 0; i < v->num_coeffs; i++) {
    uint8_t val = mag_to_u8_clamped(2.0f * v->spectrum_data[i]);
    if (val < threshold)
      val = 0;
    putchar(HEX_LUT[val >> 4]);
    putchar(HEX_LUT[val & 0x0F]);
  }

  putchar('\n');
}

int main(int argc, char *argv[]) {
  input_open(argc == 2 ? argv[1] : NULL, SAMPLE_RATE);

  FreqBand bands[BANDS];
  generate_12tet_bands(bands, 36, BANDS, 0.0);

  float window[2] = {1.0, 0.5};

  VQsDFT dft_instance;
  vqsdft_init(&dft_instance, bands, BANDS, window, 2,
              0.1f, // temporal smoothing window in seconds
              SAMPLE_RATE);

  float samples[BLOCK_SIZE];

  while (1) {
    ssize_t len;
    if ((len = input_read(samples, BLOCK_SIZE)) != BLOCK_SIZE) {
      if (len == 0)
        break;
      continue;
    }

    vqsdft_analyze_block(&dft_instance, samples, BLOCK_SIZE);

    dump_spectrum(&dft_instance, 4);
  }

  input_close();
  return 0;
}
