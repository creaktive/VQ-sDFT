#ifndef VQSDFT
#define VQSDFT

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

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
#define BUFFER_SIZE 8192 // must be a power of two!

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
  int32_t reson_coeffs[MAX_KERNEL_LEN];
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

  int32_t spectrum_data[MAX_BANDS]; // Output array
} VQsDFT;

void vqsdft_init(VQsDFT *v, const FreqBand *bands, int num_bands,
                 const double *window, int window_len, double time_res,
                 int sample_rate);

void vqsdft_analyze_block(VQsDFT *v, const int32_t *samples_q16,
                          int num_samples, bool apply_sqrt);

#endif
