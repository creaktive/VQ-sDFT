/*
 * Single file implementation of variable-Q sliding DFT (VQ-sDFT)
 * Embedded-friendly port of
 * https://gist.github.com/TF3RDL/1810c570bad4f75f77941c981c65723f by TF3RDL
 * This algorithm is derived from the paper "Application of Improved Sliding DFT
 * Algorithm for Non-Integer k" by Carl Q. Howard
 * (https://acoustics.asn.au/conference_proceedings/AAS2021/papers/p6>
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Q16.16 Fixed-Point Configuration
#define Q_SHIFT 16
#define Q_SCALE 65536.0
#define FLOAT_TO_Q16(f) ((int32_t)round((f) * Q_SCALE))
#define Q16_TO_FLOAT(q) ((double)(q) / Q_SCALE)

// Integer multiplication macro: (A * B) >> 16
#define MUL_Q(a, b) ((int32_t)((((int64_t)(a)) * ((int64_t)(b))) >> Q_SHIFT))

// Configuration limits to ensure static sizing without dynamic calls
#define MAX_BANDS 64
#define MAX_KERNEL_LEN 4 // e.g., window size 2 * 2 = 4 max indices
#define BUFFER_SIZE 2048

// Complex integer pair
typedef struct {
  int32_t x;
  int32_t y;
} cplx_q16_t;

// Frequency Band configuration
typedef struct {
  double lo;
  double ctr;
  double hi;
} FreqBand;

// Filter coefficients for a single band using static inline arrays
typedef struct {
  int32_t period;
  int kernel_length;

  cplx_q16_t fiddles[MAX_KERNEL_LEN];
  cplx_q16_t twiddles[MAX_KERNEL_LEN];
  int32_t resonCoeffs[MAX_KERNEL_LEN];
  int32_t gains[MAX_KERNEL_LEN];

  // Filter states
  cplx_q16_t coeffs1[MAX_KERNEL_LEN];
  cplx_q16_t coeffs2[MAX_KERNEL_LEN];
  cplx_q16_t coeffs3[MAX_KERNEL_LEN];
  cplx_q16_t coeffs4[MAX_KERNEL_LEN];
  cplx_q16_t coeffs5[MAX_KERNEL_LEN];
} sDFT_Coeff;

// Main Context containing only statically sized arrays
typedef struct {
  sDFT_Coeff coeffs[MAX_BANDS];
  int num_coeffs;

  int32_t buffer[BUFFER_SIZE]; // Q16.16 sample history
  int buffer_idx;

  int32_t spectrumData[MAX_BANDS]; // Output array
} VQsDFT;

// Integer Square Root (Newton-Raphson)
int32_t isqrt_q16(int64_t n) {
  if (n <= 0)
    return 0;
  int64_t x = n;
  int64_t y = (x + 1) >> 1;
  while (y < x) {
    x = y;
    y = (x + n / x) >> 1;
  }
  return (int32_t)x;
}

void vqsdft_init(VQsDFT *v, const FreqBand *bands, int num_bands,
                 const double *window, int window_len, double timeRes,
                 int sampleRate) {
  if (num_bands > MAX_BANDS) {
    printf("%d bands requested while MAX_BANDS=%d!\n", num_bands, MAX_BANDS);
    exit(1);
  }
  v->num_coeffs = num_bands;
  v->buffer_idx = BUFFER_SIZE - 1;

  // Zero-initialize buffers
  for (int i = 0; i <= BUFFER_SIZE - 1; i++)
    v->buffer[i] = 0;
  for (int i = 0; i < MAX_BANDS; i++)
    v->spectrumData[i] = 0;

  for (int b = 0; b < num_bands; b++) {
    sDFT_Coeff *c = &v->coeffs[b];

    double periodFloat = sampleRate / (fabs(bands[b].hi - bands[b].lo) +
                                       1.0 / (timeRes / 1000.0));
    if (periodFloat > BUFFER_SIZE - 1) {
      printf("period for band %d exceeds BUFFER_SIZE!\n", b);
      exit(2);
    }
    c->period = (int32_t)periodFloat;

    int minIdx = -window_len + 1;
    int maxIdx = window_len;
    int kernel_len = maxIdx - minIdx;

    if (kernel_len > MAX_KERNEL_LEN) {
      printf("kernel length for band %d is larger than MAX_KERNEL_LEN!\n", b);
      exit(3);
    }
    c->kernel_length = kernel_len;

    // Reset filter state arrays
    for (int j = 0; j < MAX_KERNEL_LEN; j++) {
      c->coeffs1[j] = (cplx_q16_t){0, 0};
      c->coeffs2[j] = (cplx_q16_t){0, 0};
      c->coeffs3[j] = (cplx_q16_t){0, 0};
      c->coeffs4[j] = (cplx_q16_t){0, 0};
      c->coeffs5[j] = (cplx_q16_t){0, 0};
    }

    for (int i = minIdx; i < maxIdx && (i - minIdx) < MAX_KERNEL_LEN; i++) {
      int j = i - minIdx;

      double amplitude = window[abs(i)] * (-(abs(i) % 2) * 2 + 1);
      double k = bands[b].ctr * c->period / sampleRate + i;
      double fid = -2.0 * M_PI * k;
      double twid = 2.0 * M_PI * k / c->period;
      double reson = 2.0 * cos(twid);

      c->fiddles[j].x = FLOAT_TO_Q16(cos(fid));
      c->fiddles[j].y = FLOAT_TO_Q16(sin(fid));
      c->twiddles[j].x = FLOAT_TO_Q16(cos(twid));
      c->twiddles[j].y = FLOAT_TO_Q16(sin(twid));
      c->resonCoeffs[j] = FLOAT_TO_Q16(reson);
      c->gains[j] = FLOAT_TO_Q16(amplitude);
    }
  }
}

void vqsdft_analyze_block(VQsDFT *v, const int32_t *samples_q16,
                          int num_samples) {
  for (int i = 0; i < v->num_coeffs; i++)
    v->spectrumData[i] = 0;

  for (int s = 0; s < num_samples; s++) {
    v->buffer_idx = (v->buffer_idx + 1) % BUFFER_SIZE;
    v->buffer[v->buffer_idx] = samples_q16[s];

    for (int i = 0; i < v->num_coeffs; i++) {
      sDFT_Coeff *coeff = &v->coeffs[i];

      int oldestIdx = (v->buffer_idx - coeff->period) % BUFFER_SIZE;
      if (oldestIdx < 0)
        oldestIdx += BUFFER_SIZE;

      int32_t bufLatest = v->buffer[v->buffer_idx];
      int32_t bufOldest = v->buffer[oldestIdx];

      int32_t sumX = 0;
      int32_t sumY = 0;

      for (int j = 0; j < coeff->kernel_length; j++) {
        int32_t combX = MUL_Q(bufLatest, coeff->fiddles[j].x) - bufOldest;
        int32_t combY = MUL_Q(bufLatest, coeff->fiddles[j].y);

        int32_t c1x = MUL_Q(combX, coeff->twiddles[j].x) -
                      MUL_Q(combY, coeff->twiddles[j].y) - coeff->coeffs2[j].x;
        int32_t c1y = MUL_Q(combX, coeff->twiddles[j].y) +
                      MUL_Q(combY, coeff->twiddles[j].x) - coeff->coeffs2[j].y;

        coeff->coeffs1[j].x = c1x;
        coeff->coeffs1[j].y = c1y;
        coeff->coeffs2[j].x = combX;
        coeff->coeffs2[j].y = combY;

        int32_t c3x = c1x + MUL_Q(coeff->resonCoeffs[j], coeff->coeffs4[j].x) -
                      coeff->coeffs5[j].x;
        int32_t c3y = c1y + MUL_Q(coeff->resonCoeffs[j], coeff->coeffs4[j].y) -
                      coeff->coeffs5[j].y;

        coeff->coeffs3[j].x = c3x;
        coeff->coeffs3[j].y = c3y;
        coeff->coeffs5[j].x = coeff->coeffs4[j].x;
        coeff->coeffs5[j].y = coeff->coeffs4[j].y;
        coeff->coeffs4[j].x = c3x;
        coeff->coeffs4[j].y = c3y;

        sumX += MUL_Q(c3x, coeff->gains[j]) / coeff->period;
        sumY += MUL_Q(c3y, coeff->gains[j]) / coeff->period;
      }

      int32_t magSq = 0;
      magSq = MUL_Q(sumX, sumX) + MUL_Q(sumY, sumY);

      if (v->spectrumData[i] < magSq)
        v->spectrumData[i] = magSq;
    }
  }

  /*
  for (int i = 0; i < v->num_coeffs; i++) {
      int64_t magSq_shifted = (int64_t)v->spectrumData[i] << Q_SHIFT;
      v->spectrumData[i] = isqrt_q16(magSq_shifted);
  }
  */
}

void generate_12tet_bands(FreqBand *bands, int start_midi_note, int num_notes) {
  for (int i = 0; i < num_notes; i++) {
    int m = start_midi_note + i;

    double ctr = 440.0 * pow(2.0, (m - 69.0) / 12.0);

    bands[i].ctr = ctr;
    bands[i].lo = ctr * pow(2.0, -1.0 / 24.0);
    bands[i].hi = ctr * pow(2.0, 1.0 / 24.0);
  }
}

int main() {
  FreqBand bands[61];
  generate_12tet_bands(bands, 36, 61);

  double window[2] = {1.0, 0.5};

  VQsDFT dft_instance;

  vqsdft_init(&dft_instance, bands, 61, window, 2,
              50.0, // temporal smoothing window in ms
              44100);

  int block_size = 128;
  int32_t q16_samples[128];

  struct timespec start, end;
  timespec_get(&start, TIME_UTC);
  int i;
  for (i = 0; i < block_size * 10000; i++) {
    int j = i % block_size;
    double float_sample = sin(2.0 * M_PI * 440.0 * i / 44100.0);
    /// double float_sample = ((i % 100) / 50.) - 1.; // sawtooth wave, 441Hz
    q16_samples[j] = FLOAT_TO_Q16(float_sample);
    if (j == block_size - 1)
      vqsdft_analyze_block(&dft_instance, q16_samples, block_size);
  }
  timespec_get(&end, TIME_UTC);

  long seconds = end.tv_sec - start.tv_sec;
  long nanoseconds = end.tv_nsec - start.tv_nsec;
  double elapsed_ms = (seconds * 1000.0) + ((double)nanoseconds / 1000000.0);
  printf("benchmark: %.0f samples per second\n\n", i / (elapsed_ms / 1000.0));

  for (i = 0; i < dft_instance.num_coeffs; i++)
    printf("band %d\t(%.2f Hz):\t%f\n", i, bands[i].ctr,
           Q16_TO_FLOAT(dft_instance.spectrumData[i]));

  return 0;
}
