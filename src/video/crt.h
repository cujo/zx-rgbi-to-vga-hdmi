#pragma once

#include "g_config.h"

// CRT effect parameters of one output (crt_from_settings) and the math on them (crt.c), shared by DVI and VGA.
// Levels are 8-bit gamma encoded channel values; a source color's channel is 0, normal or 255.
// Channel arrays are B, G, R (index 0-2).
typedef struct crt_t
{
  uint8_t normal;               // normal (non-BRIGHT) channel level
  const uint8_t (*beam_gap)[3]; // BEAM gap level between lines of level index a and b (0, normal, 255)
  uint8_t grille;               // mask: brightness of the darkened pixel, percent (0 = no mask)
  uint8_t mask;                 // mask type, grille_mask_t flags
} crt_t;

// parameters from the settings for scanline level scanlines_level: the normal level and the mask only with
// scanlines on (off: normal 170, no mask)
void crt_from_settings(crt_t *crt, const settings_t *s, uint8_t scanlines_level);

// levels of color c (IRGB, 0-15)
void crt_color(const crt_t *crt, int c, uint8_t *ch);

// levels ch as shown on a scanline of level `level` (scanlines_level_t), in place
void crt_dim(const crt_t *crt, uint8_t level, uint8_t *ch);

// BEAM: the gap between a line of levels `above` and one of levels `below`
void crt_beam(const crt_t *crt, const uint8_t *above, const uint8_t *below, uint8_t *out);

// the phosphor mask on the two output pixels of a source pixel, in place: MONO darkens the second pixel, MG
// darkens G on the first and R, B on the second (magenta | green), SLOT swaps them on scanlines (gap_row)
void crt_mask(const crt_t *crt, uint8_t *first, uint8_t *second, bool gap_row);
