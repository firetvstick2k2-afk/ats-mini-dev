#pragma once

#include <stddef.h>

// RESONIK R5-style RTTY: one-bit tone filters, dual-polarity framing,
// ITA2/USOS decoding. Input is the isolated two-tone audio stream.
enum RttyPolarity { RTTY_POL_AUTO = 0, RTTY_POL_NORMAL = 1, RTTY_POL_REVERSE = 2 };

void rtty_init();
void rtty_set_tones(float f1, float f2); // any order; 0 unlocks
void rtty_set_baud(float baud);
void rtty_set_polarity(RttyPolarity pol);
float rtty_mark_hz();
float rtty_space_hz();
float rtty_baud();
bool rtty_active();
void rtty_process(const float *x, int n);
size_t rtty_take_text(char *buf, size_t size);
