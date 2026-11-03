#include "hardware/gpio.h"

#include "g_config.h"
#include "video_output.h"
#include "dvi.h"
#include "v_buf.h"
#include "vga.h"

#ifdef OSD_ENABLE
#include "osd.h"
#endif

extern settings_t settings;
video_out_type_t active_video_output = VIDEO_OUT_TYPE_DEF;

video_mode_t video_mode;
int16_t h_visible_area;
int16_t h_margin;
int16_t v_visible_area;
int16_t v_margin;

video_out_type_t detect_video_output_type()
{
  // VGA DAC per color channel:
  //
  //   D0 --- 820 Ohm ----+
  //                      |
  //   D1 --- 390 Ohm ----+--- 75 Ohm --- GND (monitor termination)
  //
  // Drive D0 (820 Ohm) LOW, read D1 (390 Ohm) with internal pull-up (~50KOhm).
  //   VGA (no monitor):   D0-D1 path = 390 + 820 = 1210 Ohm    -> reads LOW
  //   VGA (with monitor): D1-GND = 390 + (820||75) = ~459 Ohm  -> reads LOW
  //   HDMI/DVI:           >1MOhm isolation, pull-up wins       -> reads HIGH

  const uint pins_out[] = {
      VGA_PIN_D0,
      VGA_PIN_D0 + 2,
      VGA_PIN_D0 + 4,
  };
  const uint pins_in[] = {
      VGA_PIN_D0 + 1,
      VGA_PIN_D0 + 3,
      VGA_PIN_D0 + 5,
  };

  const int num_pairs = 3;

  for (int i = 0; i < num_pairs; i++)
  {
    gpio_init(pins_out[i]);
    gpio_set_dir(pins_out[i], GPIO_OUT);
    gpio_put(pins_out[i], 0);

    gpio_init(pins_in[i]);
    gpio_set_dir(pins_in[i], GPIO_IN);
    gpio_pull_up(pins_in[i]);
  }

  sleep_ms(2);

  int high_count = 0;
  for (int i = 0; i < num_pairs; i++)
  {
    if (gpio_get(pins_in[i]))
      high_count++;
  }

  // clean up: disable pulls, set pins back to inputs
  for (int i = 0; i < num_pairs; i++)
  {
    gpio_disable_pulls(pins_in[i]);
    gpio_disable_pulls(pins_out[i]);
    gpio_set_dir(pins_out[i], GPIO_IN);
    gpio_deinit(pins_out[i]);
    gpio_deinit(pins_in[i]);
  }

  // majority vote: if at least 2 of 3 pairs read HIGH -> HDMI/DVI
  return (high_count >= 2) ? DVI : VGA;
}

void set_video_mode_params(video_mode_t v_mode)
{
  video_mode = v_mode;

  h_visible_area = (uint16_t)(video_mode.h_visible_area / (video_mode.div * 4)) * 2;
  h_margin = (h_visible_area - (uint16_t)(settings.frequency / 1000000) * (ACTIVE_VIDEO_TIME / 2)) / 2;

  if (h_margin < 0)
    h_margin = 0;

  h_visible_area -= h_margin * 2;

  v_visible_area = V_BUF_H * video_mode.div;
  v_margin = ((int16_t)((video_mode.v_visible_area - v_visible_area) / (video_mode.div * 2) + 0.5)) * video_mode.div;

  if (v_margin < 0)
    v_margin = 0;
}

// the buffering in effect: X3 at the Pentagon rate (the output and capture rates are nearly equal, so with X1 a
// tearing line would sit at one screen position), else the BUFFERING setting, which is kept for the standard rate
bool buffering_x3()
{
  return settings.buffering_mode || video_mode_pentagon(settings.video_out_mode);
}

void apply_buffering_mode()
{
  set_buffering_mode(buffering_x3());
}

void start_video_output(video_out_type_t output_type)
{
  active_video_output = output_type;
  apply_buffering_mode(); // the mode may have changed the rate

  set_video_mode_params(*(video_modes[settings.video_out_mode]));

#ifdef OSD_ENABLE
  osd_set_position();
#endif

  switch (output_type)
  {
  case DVI:
    start_dvi();
    break;

  case VGA:
    start_vga();
    break;

  default:
    break;
  }
}

void stop_video_output()
{
  switch (active_video_output)
  {
  case DVI:
    stop_dvi();
    break;

  case VGA:
    stop_vga();
    break;

  default:
    break;
  }
}

uint8_t crt_scanlines_level()
{
  uint8_t level = settings.scanlines_mode;

  if (settings.video_out_type == VGA && (level == SCANLINES_ADAPTIVE || level == SCANLINES_BEAM))
    return SCANLINES_25; // VGA: OFF, 25, 50, 75, 100% (on a 2-bit DAC ADAPT looks like 25%)

  return level;
}

bool crt_setting_active(crt_setting_t setting)
{
  // DVI only; the normal level and the mask apply with scanlines on
  bool dvi_on = settings.video_out_type == DVI && settings.scanlines_mode != SCANLINES_OFF;

  switch (setting)
  {
  case CRT_SCANLINES:
    return true;
  case CRT_BEAM_WIDTH:
    return dvi_on && settings.scanlines_mode == SCANLINES_BEAM;
  case CRT_MASK:
    return dvi_on && GRILLE_LEVEL(settings.grille_mode) != GRILLE_OFF;
  default: // CRT_GRILLE, CRT_NORMAL
    return dvi_on;
  }
}

void next_crt_setting(crt_setting_t setting)
{
  uint8_t grille = GRILLE_LEVEL(settings.grille_mode);
  uint8_t mask = GRILLE_MASK(settings.grille_mode);

  if (setting >= CRT_SETTING_COUNT || !crt_setting_active(setting))
    return;

  switch (setting)
  {
  case CRT_SCANLINES:
    if (settings.video_out_type == VGA) // OFF, 25, 50, 75, 100%
      settings.scanlines_mode = crt_scanlines_level() >= SCANLINES_100 ? SCANLINES_OFF : crt_scanlines_level() == SCANLINES_OFF ? SCANLINES_25 : crt_scanlines_level() + 1;
    else
      settings.scanlines_mode = settings.scanlines_mode >= SCANLINES_LEVEL_MAX ? SCANLINES_OFF : settings.scanlines_mode + 1;

    set_scanlines_mode();
    return;
  case CRT_BEAM_WIDTH:
    settings.crt_options = CRT_OPTIONS((CRTOPT_WIDTH(settings.crt_options) + 1) % BEAM_WIDTH_COUNT, CRTOPT_NORMAL(settings.crt_options));
    break;
  case CRT_GRILLE:
    settings.grille_mode = GRILLE_MODE((grille + 1) % GRILLE_COUNT, mask);
    break;
  case CRT_MASK:
    settings.grille_mode = GRILLE_MODE(grille, (mask + 1) % GRILLE_MASK_COUNT);
    break;
  default: // CRT_NORMAL
    settings.crt_options = CRT_OPTIONS(CRTOPT_WIDTH(settings.crt_options), (CRTOPT_NORMAL(settings.crt_options) + 1) % NORMAL_LEVEL_COUNT);
    break;
  }

  set_dvi_crt(); // the settings above are DVI only (crt_setting_active)
}

void set_scanlines_mode()
{
  if (settings.video_out_type == VGA)
    set_vga_scanlines_mode(settings.scanlines_mode);
  else if (settings.video_out_type == DVI)
    set_dvi_scanlines_mode(settings.scanlines_mode);
}

void draw_welcome_screen(video_mode_t video_mode)
{
  int16_t h_visible_area = (uint16_t)(video_mode.h_visible_area / (video_mode.div * 4)) * 4;
  int16_t h_margin = h_visible_area - (uint16_t)(settings.frequency / 1000000) * ACTIVE_VIDEO_TIME;

  if (h_margin < 0)
    h_margin = 0;

  h_visible_area -= h_margin;

  uint8_t *v_buf = (uint8_t *)get_v_buf_out();

  for (int y = 0; y < V_BUF_H; y++)
    for (int x = 0; x < V_BUF_W; x++)
    {
      uint8_t i = 0x0f & ~(uint8_t)((16 * x) / h_visible_area);
      uint8_t R = (i & 4) ? ((i & 1) ? 0b0100 : 0b1100) : 0;
      uint8_t G = (i & 8) ? ((i & 1) ? 0b0010 : 0b1010) : 0;
      uint8_t B = (i & 2) ? ((i & 1) ? 0b0001 : 0b1001) : 0;
      uint8_t c = R | G | B;

      if (x & 1)
        *v_buf++ |= c << 4;
      else
        *v_buf = c & 0x0f;
    }
}

void draw_welcome_screen_h(video_mode_t video_mode)
{
  int16_t v_margin = ((int16_t)((video_mode.v_visible_area - V_BUF_H * video_mode.div) / (video_mode.div * 2) + 0.5)) * video_mode.div * 2;

  if (v_margin < 0)
    v_margin = 0;

  uint16_t v_visible_area = video_mode.v_visible_area - v_margin;

  uint8_t *v_buf = (uint8_t *)get_v_buf_out();

  for (int y = 0; y < V_BUF_H; y++)
  {
    uint8_t i = (16 * y * video_mode.div) / v_visible_area;
    uint8_t R = (i & 4) ? ((i & 1) ? 0b0100 : 0b1100) : 0;
    uint8_t G = (i & 8) ? ((i & 1) ? 0b0010 : 0b1010) : 0;
    uint8_t B = (i & 2) ? ((i & 1) ? 0b0001 : 0b1001) : 0;
    uint8_t c = R | G | B;

    for (int x = 0; x < V_BUF_W / 2; x++)
    {
      c |= c << 4;
      *v_buf++ = c;
    }
  }
}

const char nosignal[14][115] = {
    "xx      xx      xxxxxx                  xxxxxx      xxxxxx      xxxxxx      xx      xx        xx        xx",
    "xx      xx     xxxxxxxx                xxxxxxxx     xxxxxx     xxxxxxxx     xx      xx       xxxx       xx",
    "xxx     xx    xxx    xxx              xxx    xxx      xx      xxx    xxx    xxx     xx      xxxxxx      xx",
    "xxx     xx    xx      xx              xx      xx      xx      xx      xx    xxx     xx     xxx  xxx     xx",
    "xxxx    xx    xx      xx              xx              xx      xx            xxxx    xx    xxx    xxx    xx",
    "xxxxx   xx    xx      xx              xxx             xx      xx            xxxxx   xx    xx      xx    xx",
    "xx xxx  xx    xx      xx               xxxxxxx        xx      xx            xx xxx  xx    xx      xx    xx",
    "xx  xxx xx    xx      xx                xxxxxxx       xx      xx    xxxx    xx  xxx xx    xx      xx    xx",
    "xx   xxxxx    xx      xx                     xxx      xx      xx    xxxx    xx   xxxxx    xxxxxxxxxx    xx",
    "xx    xxxx    xx      xx                      xx      xx      xx      xx    xx    xxxx    xxxxxxxxxx    xx",
    "xx     xxx    xx      xx              xx      xx      xx      xx      xx    xx     xxx    xx      xx    xx",
    "xx     xxx    xxx    xxx              xxx    xxx      xx      xxx    xxx    xx     xxx    xx      xx    xx",
    "xx      xx     xxxxxxxx                xxxxxxxx     xxxxxx     xxxxxxxx     xx      xx    xx      xx    xxxxxxxxxx",
    "xx      xx      xxxxxx                  xxxxxx      xxxxxx      xxxxxx      xx      xx    xx      xx    xxxxxxxxxx",
};

void draw_no_signal(video_mode_t video_mode)
{
  uint8_t c;
  uint8_t c2;

  int16_t h_visible_area = (uint16_t)(video_mode.h_visible_area / (video_mode.div * 4)) * 4;
  int16_t h_margin = h_visible_area - (uint16_t)(settings.frequency / 1000000) * ACTIVE_VIDEO_TIME;

  if (h_margin < 0)
    h_margin = 0;

  int16_t v_margin = ((int16_t)((video_mode.v_visible_area - V_BUF_H * video_mode.div) / (video_mode.div * 2) + 0.5)) * video_mode.div * 2;

  if (v_margin < 0)
    v_margin = 0;

  uint16_t y = (video_mode.v_visible_area - v_margin) / (video_mode.div * 2);
  uint16_t x = (h_visible_area - h_margin - 114) / 4;

  uint8_t *v_buf = (uint8_t *)get_v_buf_out();

  memset(v_buf, 0, V_BUF_H * V_BUF_W / 2);

  for (int row = 0; row < 14; ++row)
    for (int col = 0; col < 114; ++col)
    {
      c = (nosignal[row][col] == 'x') ? 0b0111 : 0b0000;

      if (col & 1)
        v_buf[(y + row) * V_BUF_W / 2 + x + col / 2] = c2 | (c << 4);
      else
        c2 = c;
    }
}