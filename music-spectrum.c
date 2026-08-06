#include <alsa/asoundlib.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>

#include "12tet.h"
#include "vqsdft.h"

#define SAMPLE_RATE 8000
#define BLOCK_SIZE 80
#define BANDS 61

snd_pcm_t *alsa_init(const char *device) {
  int err;
  snd_pcm_hw_params_t *params;
  snd_pcm_t *handle;

  if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_CAPTURE, 0)) < 0) {
    printf("Cannot open audio device: %s\n", snd_strerror(err));
    exit(1);
  }

  snd_pcm_hw_params_alloca(&params);
  snd_pcm_hw_params_any(handle, params);
  snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_channels(handle, params, 1);
  snd_pcm_hw_params_set_rate(handle, params, SAMPLE_RATE, 0);

  if ((err = snd_pcm_hw_params(handle, params)) < 0) {
    printf("Cannot set hardware parameters: %s\n", snd_strerror(err));
    exit(2);
  }

  snd_pcm_prepare(handle);
  return handle;
}

ssize_t alsa_read(snd_pcm_t *handle, int16_t *buffer, size_t frames) {
  int len = (int)snd_pcm_readi(handle, buffer, frames);
  if (len < 0) {
    len = snd_pcm_recover(handle, len, 0);
    if (len < 0) {
      printf("Read error: %s\n", snd_strerror(len));
      exit(3);
    }
    return 0;
  }
  return len;
}

static inline uint8_t q16_to_u8_clamped(int32_t v) {
  if (v <= 0)
    return 0;
  if (v >= 65536)
    return 255;
  return (uint8_t)(((v * 255) + 32768) >> Q_SHIFT);
}

void dump_spectrum(const VQsDFT *v, int threshold) {
  static const char HEX_LUT[] = "0123456789abcdef";

  for (int i = 0; i < v->num_coeffs; i++) {
    uint8_t val = q16_to_u8_clamped(v->spectrum_data[i]);
    if (val < threshold)
      val = 0;
    putchar(HEX_LUT[val >> 4]);
    putchar(HEX_LUT[val & 0x0F]);
  }

  putchar('\n');
}

sig_atomic_t should_exit = 0;
void signal_handler(/* int sig */) { should_exit = 1; }

int main(int argc, char *argv[]) {
  const char *device = (argc > 1) ? argv[1] : "plug:dsnoop";
  snd_pcm_t *handle = alsa_init(device);

  FreqBand bands[BANDS];
  generate_12tet_bands(bands, 36, BANDS, 0.0);

  double window[2] = {1.0, 0.5};

  VQsDFT dft_instance;
  vqsdft_init(&dft_instance, bands, BANDS, window, 2,
              100.0, // temporal smoothing window in ms
              SAMPLE_RATE);

  int16_t alsa_samples[BLOCK_SIZE];
  int32_t q16_samples[BLOCK_SIZE];

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  while (!should_exit) {
    ssize_t len;
    if ((len = alsa_read(handle, alsa_samples, BLOCK_SIZE)) != BLOCK_SIZE) {
      if (len == 0)
        break;
      continue;
    }

    for (int i = 0; i < BLOCK_SIZE; i++)
      q16_samples[i] = alsa_samples[i];

    vqsdft_analyze_block(&dft_instance, q16_samples, BLOCK_SIZE);

    dump_spectrum(&dft_instance, 4);
  }

  snd_pcm_close(handle);

  return 0;
}
