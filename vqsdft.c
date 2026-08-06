/*
 * Single file implementation of variable-Q sliding DFT (VQ-sDFT)
 * Embedded-friendly port of
 * https://gist.github.com/TF3RDL/1810c570bad4f75f77941c981c65723f by TF3RDL
 * This algorithm is derived from the paper "Application of Improved Sliding DFT
 * Algorithm for Non-Integer k" by Carl Q. Howard
 * (https://acoustics.asn.au/conference_proceedings/AAS2021/papers/p6>
 */

#include <stdio.h>
#include <stdlib.h>

#include "vqsdft.h"

void vqsdft_init(VQsDFT *v, const FreqBand *bands, int num_bands,
                 const float *window, int window_len, float time_res,
                 int sample_rate) {
  if (num_bands > MAX_BANDS) {
    printf("%d bands requested while MAX_BANDS=%d!\n", num_bands, MAX_BANDS);
    exit(1);
  }
  v->num_coeffs = num_bands;
  v->buffer_idx = 0;

  for (int i = 0; i < BUFFER_SIZE; i++)
    v->buffer[i] = 0.0f;

  for (int b = 0; b < num_bands; b++) {
    sDFT_Coeff *coeff = &v->coeffs[b];

    float period_float =
        (float)sample_rate /
        (fabsf(bands[b].hi - bands[b].lo) + 1.0f / (time_res / 1000.0f));
    coeff->period = (int)period_float;
    if (coeff->period >= BUFFER_SIZE) {
      printf("period for band %d exceeds BUFFER_SIZE!\n", b);
      exit(2);
    }

    int min_idx = -window_len + 1;
    int max_idx = window_len;
    int kernel_len = max_idx - min_idx;

    if (kernel_len > MAX_KERNEL_LEN) {
      printf("kernel length for band %d is larger than MAX_KERNEL_LEN!\n", b);
      exit(3);
    }
    coeff->kernel_len = kernel_len;

    // Reset filter state arrays
    for (int j = 0; j < coeff->kernel_len; j++) {
      coeff->coeffs1[j] = 0.0f + 0.0f * I;
      coeff->coeffs2[j] = 0.0f + 0.0f * I;
      coeff->coeffs3[j] = 0.0f + 0.0f * I;
      coeff->coeffs4[j] = 0.0f + 0.0f * I;
      coeff->coeffs5[j] = 0.0f + 0.0f * I;
    }

    for (int i = min_idx; i < max_idx && (i - min_idx) < MAX_KERNEL_LEN; i++) {
      int j = i - min_idx;

      float amplitude = window[abs(i)] * (float)(-(abs(i) % 2) * 2 + 1);
      float k =
          bands[b].ctr * (float)coeff->period / (float)sample_rate + (float)i;
      float fid = -2.0f * (float)M_PI * k;
      float twid = 2.0f * (float)M_PI * k / (float)coeff->period;
      float reson = 2.0f * cosf(twid);

      coeff->fiddles[j] = cosf(fid) + sinf(fid) * I;
      coeff->twiddles[j] = cosf(twid) + sinf(twid) * I;
      coeff->reson_coeffs[j] = reson;
      coeff->gains[j] = amplitude / (float)coeff->period;
    }
  }
}

void vqsdft_analyze_block(VQsDFT *v, const float *samples, int num_samples) {
  float complex sums[MAX_BANDS];

  for (int s = 0; s < num_samples; s++) {
    float buf_latest = samples[s];
    BUFFER_WRITE(v, buf_latest);

    for (int i = 0; i < v->num_coeffs; i++) {
      sDFT_Coeff *coeff = &v->coeffs[i];
      float buf_oldest = BUFFER_READN(v, coeff->period);

      float complex sum = 0.0f;

      for (int j = 0; j < coeff->kernel_len; j++) {
        float complex comb = buf_latest * coeff->fiddles[j] - buf_oldest;
        float complex c1 = comb * coeff->twiddles[j] - coeff->coeffs2[j];

        coeff->coeffs1[j] = c1;
        coeff->coeffs2[j] = comb;

        float complex c3 =
            c1 + coeff->reson_coeffs[j] * coeff->coeffs4[j] - coeff->coeffs5[j];

        coeff->coeffs3[j] = c3;
        coeff->coeffs5[j] = coeff->coeffs4[j];
        coeff->coeffs4[j] = c3;

        sum += c3 * coeff->gains[j];
      }

      sums[i] = sum;
    }
  }

  for (int i = 0; i < v->num_coeffs; i++)
    v->spectrum_data[i] = sqrtf(crealf(sums[i]) * crealf(sums[i]) +
                                cimagf(sums[i]) * cimagf(sums[i]));
}
