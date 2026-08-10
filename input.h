#ifndef INPUT_WRAPPER
#define INPUT_WRAPPER

void input_open(const char *source, const unsigned int sample_rate);
ssize_t input_read(float *samples, size_t frames);
void input_close(void);

#endif
