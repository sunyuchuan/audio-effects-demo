#ifndef BLACKMAN_H_
#define BLACKMAN_H_

#include "pitch_macro.h"

extern const float blackman_tab[PITCH_BUFFER_SIZE / 2];
extern const float inv_blackman_corr_tab[PITCH_BUFFER_SIZE];
extern const float hamming_tab[PITCH_BUFFER_SIZE / 2];
extern const float inv_hamming_corr_tab[PITCH_BUFFER_SIZE];
extern const float triangular_tab[PITCH_BUFFER_SIZE / 2];
extern const float inv_triangular_corr_tab[PITCH_BUFFER_SIZE];

#endif  // BLACKMAN_H_
