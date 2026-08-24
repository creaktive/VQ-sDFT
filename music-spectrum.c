#include <signal.h>
#include <stdio.h>
#include <time.h>

#include "ws2811/ws2811.h"

#include "12tet.h"
#include "input.h"
#include "vqsdft.h"

#define SAMPLE_RATE 24000
#define BLOCK_SIZE 240
#define BANDS MAX_BANDS

#define THRESHOLD 0.015f
#define START_MIDI_NOTE 35

#define GPIO_PIN 12
#define LED_COUNT (BANDS * 2) // 64 bands × 2 = 128 LEDs
#define BRIGHTNESS 255
#define DMA_CHANNEL 10

static const ws2811_led_t PALETTE[] = {
    0x00FF0000, // red
    0x0080FF00, // orange
    0x00FFFF00, // yellow
    0x00FF8000, // lime
    0x0000FF00, // green
    0x0080FF00, // spring
    0x0000FFFF, // cyan
    0x008080FF, // sky
    0x000000FF, // blue
    0x00FF00FF, // purple
    0x00FF00FF, // magenta
    0x008000FF, // pink
};
#define PALETTE_LEN (int)(sizeof(PALETTE) / sizeof(PALETTE[0]))

static uint32_t palette_color_level(ws2811_led_t color, float level) {
  if (level <= THRESHOLD)
    return 0;
  if (level >= 1.0f)
    return color;
  int r = (color >> 16) & 0xFF;
  int g = (color >> 8) & 0xFF;
  int b = color & 0xFF;
  r = (int)((float)r * level + 0.5f);
  g = (int)((float)g * level + 0.5f);
  b = (int)((float)b * level + 0.5f);
  if (r > 255)
    r = 255;
  if (g > 255)
    g = 255;
  if (b > 255)
    b = 255;
  return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void render_leds(const VQsDFT *v, ws2811_t *ws2811) {
  for (int i = 0; i < v->num_coeffs; i++) {
    float mag = 2.0f * v->spectrum_data[i];
    ws2811_led_t color = palette_color_level(PALETTE[i % PALETTE_LEN], mag);
    ws2811->channel[0].leds[i * 2] = color;
    ws2811->channel[0].leds[i * 2 + 1] = color;
  }
}

static volatile sig_atomic_t running = 1;

static void signal_handler(int signum) {
  (void)signum;
  running = 0;
}

int main(int argc, char *argv[]) {
  ws2811_return_t ret;
  ws2811_t ledstring = {
      .freq = WS2811_TARGET_FREQ,
      .dmanum = DMA_CHANNEL,
      .channel =
          {
              [0] =
                  {
                      .gpionum = GPIO_PIN,
                      .invert = 0,
                      .count = LED_COUNT,
                      .strip_type = WS2811_STRIP_GRB,
                      .brightness = BRIGHTNESS,
                  },
          },
  };

  if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS) {
    fprintf(stderr, "ws2811_init failed: %s\n", ws2811_get_return_t_str(ret));
    return 1;
  }

  struct sigaction sa = {.sa_handler = signal_handler};
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  input_open(argc == 2 ? argv[1] : NULL, SAMPLE_RATE);

  FreqBand bands[BANDS];
  generate_12tet_bands(bands, START_MIDI_NOTE, BANDS, 0.0);

  float window[2] = {1.0, 0.5};

  VQsDFT dft_instance;
  vqsdft_init(&dft_instance, bands, BANDS, window, 2,
              0.1f, // temporal smoothing window in seconds
              SAMPLE_RATE);

  float samples[BLOCK_SIZE];

  while (running) {
    ssize_t len;
    if ((len = input_read(samples, BLOCK_SIZE)) != BLOCK_SIZE) {
      if (len == 0)
        break;
      continue;
    }

    vqsdft_analyze_block(&dft_instance, samples, BLOCK_SIZE);

    render_leds(&dft_instance, &ledstring);

    if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS) {
      fprintf(stderr, "ws2811_render failed: %s\n",
              ws2811_get_return_t_str(ret));
      break;
    }
  }

  // Clear LEDs on exit
  for (int i = 0; i < LED_COUNT; i++)
    ledstring.channel[0].leds[i] = 0;
  ws2811_render(&ledstring);
  ws2811_fini(&ledstring);

  input_close();
  return 0;
}
