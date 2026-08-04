#include "12tet.h"

void generate_12tet_bands(FreqBand *bands, int start_midi_note, int num_notes,
                          double width) {
  for (int i = 0; i < num_notes; i++) {
    int m = start_midi_note + i;

    double ctr = 440.0 * pow(2.0, (m - 69.0) / 12.0);

    bands[i].ctr = ctr;
    bands[i].lo = ctr * pow(2.0, -width / 24.0);
    bands[i].hi = ctr * pow(2.0, width / 24.0);
  }
}
