// CRT emulation math shared by the DVI and VGA outputs: scanline dimming, BEAM gaps and the phosphor mask. Levels
// are 8-bit gamma encoded channel values (0, normal, 255 for the source colors).
#include "crt.h"

static const uint8_t normal_values[NORMAL_LEVEL_COUNT] = {[NORMAL_LEVEL_170] = 170, [NORMAL_LEVEL_192] = 192, [NORMAL_LEVEL_205] = 205, [NORMAL_LEVEL_216] = 216};

// mask strength (grille_mode_t) -> brightness of the darkened pixel in percent, 0 = off
static const uint8_t grille_pct[GRILLE_COUNT] = {[GRILLE_OFF] = 0, [GRILLE_5] = 95, [GRILLE_10] = 90, [GRILLE_15] = 85, [GRILLE_30] = 70, [GRILLE_45] = 55};

// BEAM: channel level in the gap between a line of level a and a line of level b (0 = 0, 1 = normal, 2 = 255).
// crt-geom beam profile sampled half a line from each center, normalized so a line center keeps its own level;
// gamma 2.2 in and out, so the line colors do not shift.
// Computed offline: w(d) = exp(-((d / s) / sqrt(wid / 2))^wid), each line contributes L * w(0.5) / w(0) of linear
// light (L = linear level), the sum is clamped to 1.
// crt-geom widens the beam with brightness as wid = 2 + 2 * L^4, which closes the gap on BRIGHT colors (255) almost
// fully while leaving it deep on normal ones (170) and makes the contrast between them too high; here
// wid = 2 + 0.5 * L^4 keeps a little of that bloom. One table per beam_width_t, differing in the distance scale s
// (crt-geom uses 0.3; larger = wider beam). Tables per normal_level_t, then per beam width.
// Gap / line in the comments: normal on normal, 255 on 255.
static const uint8_t beam_gap_tables[NORMAL_LEVEL_COUNT][BEAM_WIDTH_COUNT][3][3] = {
    [NORMAL_LEVEL_170] = {
        [BEAM_WIDTH_TV] = {{0, 84, 140}, {84, 115, 159}, {140, 159, 192}},    // s 0.40: 67%, 75%
        [BEAM_WIDTH_SOFT] = {{0, 90, 150}, {90, 123, 170}, {150, 170, 205}},  // s 0.42: 72%, 80%
        [BEAM_WIDTH_PVM] = {{0, 60, 96}, {60, 82, 111}, {96, 111, 132}},      // s 0.33: 48%, 51%
        [BEAM_WIDTH_SHARP] = {{0, 78, 129}, {78, 106, 147}, {129, 147, 177}}, // s 0.38: 62%, 69%
    },
    [NORMAL_LEVEL_192] = {
        [BEAM_WIDTH_TV] = {{0, 95, 140}, {95, 130, 164}, {140, 164, 192}},     // s 0.40: 67%, 75%
        [BEAM_WIDTH_SOFT] = {{0, 102, 150}, {102, 139, 176}, {150, 176, 205}}, // s 0.42: 72%, 80%
        [BEAM_WIDTH_PVM] = {{0, 68, 96}, {68, 93, 115}, {96, 115, 132}},       // s 0.33: 48%, 51%
        [BEAM_WIDTH_SHARP] = {{0, 88, 129}, {88, 121, 152}, {129, 152, 177}},  // s 0.38: 63%, 69%
    },
    [NORMAL_LEVEL_205] = {
        [BEAM_WIDTH_TV] = {{0, 102, 140}, {102, 140, 168}, {140, 168, 192}},   // s 0.40: 68%, 75%
        [BEAM_WIDTH_SOFT] = {{0, 109, 150}, {109, 150, 180}, {150, 180, 205}}, // s 0.42: 73%, 80%
        [BEAM_WIDTH_PVM] = {{0, 73, 96}, {73, 100, 117}, {96, 117, 132}},      // s 0.33: 48%, 51%
        [BEAM_WIDTH_SHARP] = {{0, 95, 129}, {95, 130, 155}, {129, 155, 177}},  // s 0.38: 63%, 69%
    },
    [NORMAL_LEVEL_216] = {
        [BEAM_WIDTH_TV] = {{0, 109, 140}, {109, 149, 172}, {140, 172, 192}},   // s 0.40: 68%, 75%
        [BEAM_WIDTH_SOFT] = {{0, 116, 150}, {116, 159, 184}, {150, 184, 205}}, // s 0.42: 73%, 80%
        [BEAM_WIDTH_PVM] = {{0, 77, 96}, {77, 106, 120}, {96, 120, 132}},      // s 0.33: 49%, 51%
        [BEAM_WIDTH_SHARP] = {{0, 100, 129}, {100, 138, 159}, {129, 159, 177}}, // s 0.38: 63%, 69%
    },
};

void crt_from_settings(crt_t *crt, const settings_t *s, uint8_t scanlines_level)
{
  // the normal level and the mask apply with scanlines on; fields are range-checked by check_settings
  bool on = scanlines_level != SCANLINES_OFF;
  uint8_t normal_level = on ? CRTOPT_NORMAL(s->crt_options) : NORMAL_LEVEL_170;
  uint8_t grille = GRILLE_LEVEL(s->grille_mode);

  crt->normal = normal_values[normal_level];
  crt->beam_gap = beam_gap_tables[normal_level][CRTOPT_WIDTH(s->crt_options)];
  crt->grille = on && grille < GRILLE_COUNT ? grille_pct[grille] : 0;
  crt->mask = GRILLE_MASK(s->grille_mode);
}

// channel level (0, normal or 255) as an index into the beam gap table
static int level_idx(const crt_t *crt, uint8_t v)
{
  return v == 255 ? 2 : v == crt->normal ? 1 : 0;
}

void crt_color(const crt_t *crt, int c, uint8_t *ch)
{
  for (int i = 0; i < 3; i++)
    ch[i] = ((c >> i) & 1) ? ((c & 8) ? 255 : crt->normal) : 0;
}

void crt_dim(const crt_t *crt, uint8_t level, uint8_t *ch)
{
  for (int i = 0; i < 3; i++)
  {
    uint8_t v = ch[i];

    switch (level)
    {
    case SCANLINES_BEAM: // both neighbor lines of this color (a flat area)
      v = crt->beam_gap[level_idx(crt, v)][level_idx(crt, v)];
      break;
    case SCANLINES_ADAPTIVE: // a bright beam spot spreads and almost fills the gap, a dim one does not
      v = v == 255 ? 200 : v * 10 / 17; // normal: 100 for 170
      break;
    case SCANLINES_25:
      v = v * 3 / 4;
      break;
    case SCANLINES_50:
      v = v / 2;
      break;
    case SCANLINES_75:
      v = v / 4;
      break;
    default: // SCANLINES_100 (and OFF, where the result is unused)
      v = 0;
      break;
    }

    ch[i] = v;
  }
}

void crt_beam(const crt_t *crt, const uint8_t *above, const uint8_t *below, uint8_t *out)
{
  for (int i = 0; i < 3; i++)
    out[i] = crt->beam_gap[level_idx(crt, above[i])][level_idx(crt, below[i])];
}

void crt_mask(const crt_t *crt, uint8_t *first, uint8_t *second, bool gap_row)
{
  if (!crt->grille)
    return;

  bool swap = (crt->mask & GRILLE_MASK_SLOT) && gap_row;
  bool g_first = (crt->mask & GRILLE_MASK_MG) ? !swap : swap;

  for (int i = 0; i < 3; i++)
  {
    uint8_t *v = (i == 1 ? g_first : swap) ? &first[i] : &second[i];
    *v = *v * crt->grille / 100;
  }
}
