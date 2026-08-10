#include <stdio.h>
#include <stdlib.h>

FILE *fp;

void input_open(const char *source, const unsigned int sample_rate) {
  if (source != NULL) {
    fp = fopen(source, "rb");
    if (!fp) {
      printf("Cannot open file %s at sample rate %d\n", source, sample_rate);
      exit(1);
    }
  } else {
    fp = stdin;
  }
}

ssize_t input_read(float *samples, size_t frames) {
  return (ssize_t)fread(samples, sizeof(float), frames, fp);
}

void input_close(void) { fclose(fp); }
