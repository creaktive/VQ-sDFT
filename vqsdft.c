/*
 * Single file implementation of variable-Q sliding DFT (VQ-sDFT)
 * Embedded-friendly port of
 * https://gist.github.com/TF3RDL/1810c570bad4f75f77941c981c65723f by TF3RDL
 * This algorithm is derived from the paper "Application of Improved Sliding DFT
 * Algorithm for Non-Integer k" by Carl Q. Howard
 * (https://acoustics.asn.au/conference_proceedings/AAS2021/papers/p6>
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "vqsdft.h"

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
                 const double *window, int window_len, double time_res,
                 int sample_rate) {
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
    v->spectrum_data[i] = 0;

  for (int b = 0; b < num_bands; b++) {
    sDFT_Coeff *c = &v->coeffs[b];

    double period_float = sample_rate / (fabs(bands[b].hi - bands[b].lo) +
                                         1.0 / (time_res / 1000.0));
    if (period_float > BUFFER_SIZE - 1) {
      printf("period for band %d exceeds BUFFER_SIZE!\n", b);
      exit(2);
    }
    c->period = (int32_t)period_float;

    int min_idx = -window_len + 1;
    int max_idx = window_len;
    int kernel_len = max_idx - min_idx;

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

    for (int i = min_idx; i < max_idx && (i - min_idx) < MAX_KERNEL_LEN; i++) {
      int j = i - min_idx;

      double amplitude = (window[abs(i)] * (-(abs(i) % 2) * 2 + 1)) / c->period;
      double k = bands[b].ctr * c->period / sample_rate + i;
      double fid = -2.0 * M_PI * k;
      double twid = 2.0 * M_PI * k / c->period;
      double reson = 2.0 * cos(twid);

      c->fiddles[j].x = FLOAT_TO_Q16(cos(fid));
      c->fiddles[j].y = FLOAT_TO_Q16(sin(fid));
      c->twiddles[j].x = FLOAT_TO_Q16(cos(twid));
      c->twiddles[j].y = FLOAT_TO_Q16(sin(twid));
      c->reson_coeffs[j] = FLOAT_TO_Q16(reson);
      c->gains[j] = FLOAT_TO_Q16(amplitude);
    }
  }
}

void vqsdft_analyze_block(VQsDFT *v, const int32_t *samples_q16,
                          int num_samples) {
  for (int i = 0; i < v->num_coeffs; i++)
    v->spectrum_data[i] = 0;

  for (int s = 0; s < num_samples; s++) {
    v->buffer_idx = (v->buffer_idx + 1) & (BUFFER_SIZE - 1);
    v->buffer[v->buffer_idx] = samples_q16[s];

    for (int i = 0; i < v->num_coeffs; i++) {
      sDFT_Coeff *coeff = &v->coeffs[i];

      int oldest_idx = (v->buffer_idx - coeff->period);
      if (oldest_idx < 0)
        oldest_idx += BUFFER_SIZE;
      oldest_idx &= (BUFFER_SIZE - 1);

      int32_t bufLatest = v->buffer[v->buffer_idx];
      int32_t bufOldest = v->buffer[oldest_idx];

      int32_t sum_x = 0;
      int32_t sum_y = 0;

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

        int32_t c3x = c1x + MUL_Q(coeff->reson_coeffs[j], coeff->coeffs4[j].x) -
                      coeff->coeffs5[j].x;
        int32_t c3y = c1y + MUL_Q(coeff->reson_coeffs[j], coeff->coeffs4[j].y) -
                      coeff->coeffs5[j].y;

        // Force states to collapse to zero rather than getting locked in Q16
        // rounding. Branchless active pull-down using bitwise arithmetic:
        c3x -= (c3x >> 31) - (-c3x >> 31);
        c3y -= (c3y >> 31) - (-c3y >> 31);

        coeff->coeffs3[j].x = c3x;
        coeff->coeffs3[j].y = c3y;
        coeff->coeffs5[j].x = coeff->coeffs4[j].x;
        coeff->coeffs5[j].y = coeff->coeffs4[j].y;
        coeff->coeffs4[j].x = c3x;
        coeff->coeffs4[j].y = c3y;

        sum_x += MUL_Q(c3x, coeff->gains[j]);
        sum_y += MUL_Q(c3y, coeff->gains[j]);
      }

      int32_t mag_sq = 0;
      mag_sq = MUL_Q(sum_x, sum_x) + MUL_Q(sum_y, sum_y);

      if (v->spectrum_data[i] < mag_sq)
        v->spectrum_data[i] = mag_sq;
    }
  }

  for (int i = 0; i < v->num_coeffs; i++) {
    int64_t mag_sq_shifted = (int64_t)v->spectrum_data[i] << Q_SHIFT;
    v->spectrum_data[i] = isqrt_q16(mag_sq_shifted);
  }
}
