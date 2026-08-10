#include <alsa/asoundlib.h>
#include <signal.h>

snd_pcm_t *handle;

sig_atomic_t should_exit = 0;
void signal_handler() { should_exit = 1; }

void input_open(const char *source, const unsigned int sample_rate) {
  snd_pcm_hw_params_t *params;
  int err;

  if ((err = snd_pcm_open(&handle, source != NULL ? source : "plug:dsnoop",
                          SND_PCM_STREAM_CAPTURE, 0)) < 0) {
    printf("Cannot open audio device %s: %s\n", source, snd_strerror(err));
    exit(1);
  }

  snd_pcm_hw_params_alloca(&params);
  snd_pcm_hw_params_any(handle, params);
  snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_channels(handle, params, 1);
  snd_pcm_hw_params_set_rate(handle, params, sample_rate, 0);

  if ((err = snd_pcm_hw_params(handle, params)) < 0) {
    printf("Cannot set hardware parameters: %s\n", snd_strerror(err));
    exit(2);
  }

  snd_pcm_prepare(handle);

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
}

ssize_t input_read(float *samples, size_t frames) {
  if (should_exit)
    return 0;

  int16_t buffer[1024];
  if (frames > sizeof(buffer) / sizeof(buffer[0])) {
    printf("Buffer too small!\n");
    exit(3);
  }

  int len = (int)snd_pcm_readi(handle, buffer, frames);
  if (len < 0) {
    len = snd_pcm_recover(handle, len, 0);
    if (len < 0) {
      printf("Unrecoverable read error: %s\n", snd_strerror(len));
      exit(4);
    }
  }

  // ALSA uses too much CPU when reading as SND_PCM_FORMAT_FLOAT_LE
  for (size_t i = 0; i < frames; i++)
    samples[i] = buffer[i] / 32768.0f;

  return len;
}

void input_close(void) { snd_pcm_close(handle); }
