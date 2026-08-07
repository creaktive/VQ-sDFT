#include "12tet.h"

void generate_12tet_bands(FreqBand *bands, int start_midi_note, int num_notes,
                          float width) {
  for (int i = 0; i < num_notes; i++) {
    int m = start_midi_note + i;

    float ctr = 440.0f * powf(2.0f, (float)(m - 69) / 12.0f);

    bands[i].ctr = ctr;
    bands[i].lo = ctr * powf(2.0f, -width / 24.0f);
    bands[i].hi = ctr * powf(2.0f, width / 24.0f);
  }
}
