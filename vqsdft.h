#ifndef VQSDFT
#define VQSDFT

#include <complex.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

// Configuration limits to ensure static sizing without dynamic calls
#define MAX_BANDS 64
#define MAX_KERNEL_LEN 4 // e.g., window size 2 * 2 = 4 max indices

/* Ring buffer accessors - macros for branchless wrap-around.
 * BUFFER_WRITE(buf, v) inserts v at the latest position, auto-increments index.
 * BUFFER_READN(buf, n) reads from n-th position (0=latest, 1=previous).
 */
#define BUFFER_SIZE 8192 // must be a power of two!
#define BUFFER_MASK (BUFFER_SIZE - 1)
#define BUFFER_WRITE(p, v) ((p->buffer)[(p->buffer_idx++) & BUFFER_MASK] = (v))
#define BUFFER_READN(p, n) ((p->buffer)[(p->buffer_idx + (~n)) & BUFFER_MASK])

// Frequency Band configuration
typedef struct {
  float lo;
  float ctr;
  float hi;
} FreqBand;

// Filter coefficients for a single band using static inline arrays
typedef struct {
  int period;
  int kernel_length;

  complex float fiddles[MAX_KERNEL_LEN];
  complex float twiddles[MAX_KERNEL_LEN];
  float reson_coeffs[MAX_KERNEL_LEN];
  float gains[MAX_KERNEL_LEN];

  // Filter states
  complex float coeffs1[MAX_KERNEL_LEN];
  complex float coeffs2[MAX_KERNEL_LEN];
  complex float coeffs3[MAX_KERNEL_LEN];
  complex float coeffs4[MAX_KERNEL_LEN];
  complex float coeffs5[MAX_KERNEL_LEN];
} sDFT_Coeff;

// Main Context containing only statically sized arrays
typedef struct {
  sDFT_Coeff coeffs[MAX_BANDS];
  int num_coeffs;

  float buffer[BUFFER_SIZE]; // sample history
  int buffer_idx;

  float spectrum_data[MAX_BANDS]; // Output array (magnitude)
} VQsDFT;

void vqsdft_init(VQsDFT *v, const FreqBand *bands, int num_bands,
                 const float *window, int window_len, float time_res,
                 int sample_rate);

void vqsdft_analyze_block(VQsDFT *v, const float *samples, int num_samples);

#endif
