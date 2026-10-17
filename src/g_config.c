#include "g_config.h"

video_mode_t mode_640x480_60Hz = {
    .sys_freq = 252000,
    .pixel_freq = 25200000.0, // 25175000.0,
    .h_visible_area = 640,
    .v_visible_area = 480,
    .whole_line = 800,
    .whole_frame = 525,
    .h_front_porch = 16,
    .h_sync_pulse = 96,
    .h_back_porch = 48,
    .v_front_porch = 10,
    .v_sync_pulse = 2,
    .v_back_porch = 33,
    .sync_polarity = 0b11000000, // negative
    .div = 2,
    .shX = 68,
    .shY = 34,
};

// 480p at the Pentagon frame rate: 25 MHz / 800 = 31.25 kHz line rate,
// 31250 / 640 = 48.828125 Hz (one input line = two output lines, as in
// 720x576@48.8). The pixel clock is 25.0 MHz instead of 25.2 MHz so that a
// whole line takes exactly 32 us: sys_clk 250 MHz (VCO 1500 / 6 / 1), one
// TMDS bit per sys_clk. Horizontal timing, sync widths and back porches
// match 640x480@60; the 115 extra lines go to the vertical front porch, so
// the picture position is unchanged. DVI and VGA (VGA: PIO clkdiv
// 250 x 2 / 25 = 20, line 800 / 2 = 400 bytes = 100 words, no fractions).
// BUFFERING is X3 at the Pentagon rate (buffering_x3).
video_mode_t mode_640x480_48Hz = {
    .sys_freq = 250000,
    .pixel_freq = 25000000.0,
    .h_visible_area = 640,
    .v_visible_area = 480,
    .whole_line = 800,
    .whole_frame = 640,
    .h_front_porch = 16,
    .h_sync_pulse = 96,
    .h_back_porch = 48,
    .v_front_porch = 125,
    .v_sync_pulse = 2,
    .v_back_porch = 33,
    .sync_polarity = 0b11000000, // negative
    .div = 2,
    .shX = 68, // as 640x480@60: same visible area and div
    .shY = 34,
};

video_mode_t mode_720x576_50Hz = {
    .sys_freq = 270000,
    .pixel_freq = 27000000.0,
    .h_visible_area = 720,
    .v_visible_area = 576,
    .whole_line = 864,
    .whole_frame = 625,
    .h_front_porch = 12,
    .h_sync_pulse = 64,
    .h_back_porch = 68,
    .v_front_porch = 5,
    .v_sync_pulse = 5,
    .v_back_porch = 39,
    .sync_polarity = 0b11000000, // negative
    .div = 2,
    .shX = 48,
    .shY = 12,
};

// 576p at the Pentagon frame rate: 31250 / 640 = 48.828125 Hz (320 input
// lines x 64 us, one input line = two output lines). Free-running with
// constant timing; the residual crystal offset costs one repeated/skipped
// frame every few minutes. The 15 extra lines go to the front porch, so the
// picture position is unchanged. BUFFERING is X3 at the Pentagon rate
// (buffering_x3): with nearly matched rates the buffer phase is quasi-static,
// so in X1 a tearing line would sit at one screen position.
// Available on DVI and VGA. See docs/VGA_TIMINGS.md.
video_mode_t mode_720x576_48Hz = {
    .sys_freq = 270000,
    .pixel_freq = 27000000.0,
    .h_visible_area = 720,
    .v_visible_area = 576,
    .whole_line = 864,
    .whole_frame = 640,
    .h_front_porch = 12,
    .h_sync_pulse = 64,
    .h_back_porch = 68,
    .v_front_porch = 20,
    .v_sync_pulse = 5,
    .v_back_porch = 39,
    .sync_polarity = 0b11000000, // negative
    .div = 2,
    .shX = 48, // as 720x576@50: same visible area and div
    .shY = 12,
};

video_mode_t mode_800x600_60Hz = {
    .sys_freq = 240000,
    .pixel_freq = 40000000.0,
    .h_visible_area = 800,
    .v_visible_area = 600,
    .whole_line = 1056,
    .whole_frame = 628,
    .h_front_porch = 40,
    .h_sync_pulse = 128,
    .h_back_porch = 88,
    .v_front_porch = 1,
    .v_sync_pulse = 4,
    .v_back_porch = 23,
    .sync_polarity = 0b00000000, // positive
    .div = 2,
    .shX = 46,
    .shY = 4,
};

video_mode_t mode_1024x768_60Hz_d3 = {
    .sys_freq = 260000,
    .pixel_freq = 65000000.0,
    .h_visible_area = 1020, // 1024
    .v_visible_area = 768,
    .whole_line = 1344,
    .whole_frame = 806,
    .h_front_porch = 24,
    .h_sync_pulse = 135, // 136
    .h_back_porch = 165, // 160
    .v_front_porch = 3,
    .v_sync_pulse = 6,
    .v_back_porch = 29,
    .sync_polarity = 0b11000000, // negative
    .div = 3,
    .shX = 58,
    .shY = 27,
};

video_mode_t mode_1024x768_60Hz_d4 = {
    .sys_freq = 260000,
    .pixel_freq = 65000000.0,
    .h_visible_area = 1024,
    .v_visible_area = 768,
    .whole_line = 1344,
    .whole_frame = 806,
    .h_front_porch = 24,
    .h_sync_pulse = 136,
    .h_back_porch = 160,
    .v_front_porch = 3,
    .v_sync_pulse = 6,
    .v_back_porch = 29,
    .sync_polarity = 0b11000000, // negative
    .div = 4,
    .shX = 99,
    .shY = 58,
};

video_mode_t mode_1280x1024_60Hz_d3 = {
    .sys_freq = 252000,
    .pixel_freq = 108000000.0,
    .h_visible_area = 1278, // 1280
    .v_visible_area = 1024,
    .whole_line = 1692, // 1688
    .whole_frame = 1066,
    .h_front_porch = 48,
    .h_sync_pulse = 111, // 112
    .h_back_porch = 255, // 248
    .v_front_porch = 1,
    .v_sync_pulse = 3,
    .v_back_porch = 38,
    .sync_polarity = 0b00000000, // positive
    .div = 3,
    .shX = 46,
    .shY = 2,
};

video_mode_t mode_1280x1024_60Hz_d4 = {
    .sys_freq = 243000, // 270000, // switch to a higher system clock if the image is unstable
    .pixel_freq = 108000000.0,
    .h_visible_area = 1280,
    .v_visible_area = 1024,
    .whole_line = 1680, // 1688
    .whole_frame = 1066,
    .h_front_porch = 48,
    .h_sync_pulse = 112,
    .h_back_porch = 240, // 248
    .v_front_porch = 1,
    .v_sync_pulse = 3,
    .v_back_porch = 38,
    .sync_polarity = 0b00000000, // positive
    .div = 4,
    .shX = 68,
    .shY = 27,
};

// ── VGA 1280x1024 at the Pentagon frame rate (48.828125 Hz) ────────────
// The display's VGA input must accept a 48.8 Hz vertical rate (many LCD
// scalers refuse < 50-56 Hz). BUFFERING is X3, see 720x576@48.8.

// 89.25 MHz, 1680 x 1088 total, h 53.125 kHz, v 48.828125 Hz exactly.
// The first version kept the 108 MHz pixel clock with a 1728 x 1280 total (62.5 kHz); monitors that take
// 640x480 and 720x576 at 48.8 Hz showed "Out of range" for it. This one has the VESA 1280x1024 shape instead:
// the total line of the @60 d4 mode, sync widths and back porches of @60 (v front porch 23 instead of 1), at a
// lower pixel clock. sys_clk 238 MHz (VCO 1428 / 6 / 1), PIO clkdiv 238 x 3 / 89.25 = 8 (no fractions).
// On a native 1280x1024 panel: the Pentagon frame 1:1 in time and integer 3x vertical scaling (960 lines).
video_mode_t mode_1280x1024_48Hz_d3 = {
    .sys_freq = 238000,
    .pixel_freq = 89250000.0,
    .h_visible_area = 1278, // 1280
    .v_visible_area = 1024,
    .whole_line = 1680,
    .whole_frame = 1088,
    .h_front_porch = 48,
    .h_sync_pulse = 111, // 112
    .h_back_porch = 243, // 248
    .v_front_porch = 23,
    .v_sync_pulse = 3,
    .v_back_porch = 38,
    .sync_polarity = 0b00000000, // positive
    .div = 3,
    .shX = 46, // as 1280x1024@60 d3: same visible area and div
    .shY = 2,
};

// DIV4 variant: 4x scaling, 320 source pixels fill the 1280 width; 1024 / 4 = 256 of the 320 source lines are
// shown (as in 1280x1024@60 d4). No exact DIV4 timing has an integer PIO divider at an exact sys_clk, so this one
// is 15 ppm fast (48.828837 Hz, one dropped frame in ~23 minutes, below crystal tolerance). Otherwise the d3
// timing: 1680 x 1089 total (v front porch 24), 89.333 MHz, h 53.175 kHz, positive syncs, @60 d4 horizontal
// porches. sys_clk 268 MHz (VCO 804 / 3 / 1), PIO clkdiv 268 x 4 / 89.333 = 12.
video_mode_t mode_1280x1024_48Hz_d4 = {
    .sys_freq = 268000,
    .pixel_freq = 89333333.3,
    .h_visible_area = 1280,
    .v_visible_area = 1024,
    .whole_line = 1680,
    .whole_frame = 1089,
    .h_front_porch = 48,
    .h_sync_pulse = 112,
    .h_back_porch = 240,
    .v_front_porch = 24,
    .v_sync_pulse = 3,
    .v_back_porch = 38,
    .sync_polarity = 0b00000000, // positive
    .div = 4,
    .shX = 68, // as 1280x1024@60 d4: same visible area and div
    .shY = 27,
};

// VGA 800x600 at the Pentagon frame rate. A VGA monitor tells modes apart by line rate and lines per frame, and the
// 31.25 kHz / 640-line signal of 640x480@48.8 and 720x576@48.8 is closest to 800x600 in its tables (625-628 lines),
// so some monitors (BenQ 1280x1024) take those modes for 800x600 and sample them wrong (720x576@50, 625 lines, too).
// Monitors that know 576p show 720x576 correctly; this one is for the others. This mode is a real 800x600
// with the VESA 800x600 line (800 + 40 + 128 + 88 = 1056 pixels, positive syncs) at 33 MHz instead of 40 MHz: the
// monitor's assumed line length is then right. 1056 x 640 total, h 31.25 kHz, v 48.828125 Hz exactly; vertical
// porches of 800x600@60, the 12 extra lines in the front porch. sys_clk 264 MHz (VCO 1584 / 6 / 1), PIO divider
// 264 / 33 = 8 (one output byte per monitor pixel). The image is 400 x 300 source pixels: a 7 MHz capture
// (~364 pixels) gets narrow side bars, nearly the whole border is shown vertically.
video_mode_t mode_800x600_48Hz = {
    .sys_freq = 264000,
    .pixel_freq = 33000000.0,
    .h_visible_area = 800,
    .v_visible_area = 600,
    .whole_line = 1056,
    .whole_frame = 640,
    .h_front_porch = 40,
    .h_sync_pulse = 128,
    .h_back_porch = 88,
    .v_front_porch = 13,
    .v_sync_pulse = 4,
    .v_back_porch = 23,
    .sync_polarity = 0b00000000, // positive
    .div = 2,
    .shX = 46, // as 800x600@60: same visible area and div
    .shY = 4,
};

// VGA 1024x768 at the Pentagon frame rate: the VESA 1024x768 line (1344 pixels, negative syncs) at 52.5 MHz,
// 1344 x 800 total, h 39.0625 kHz, v 48.828125 Hz exactly. 800 lines (the @60 mode has 806) keep the monitor's
// guess at 1024x768; the vertical back porch is 25 lines instead of 29, front porch 1. sys_clk 210 MHz (VCO 1260 /
// 6 / 1), PIO divider 210 x 3 / 52.5 = 12 (d3) and 210 x 4 / 52.5 = 16 (d4).
video_mode_t mode_1024x768_48Hz_d3 = {
    .sys_freq = 210000,
    .pixel_freq = 52500000.0,
    .h_visible_area = 1020, // 1024
    .v_visible_area = 768,
    .whole_line = 1344,
    .whole_frame = 800,
    .h_front_porch = 24,
    .h_sync_pulse = 135, // 136
    .h_back_porch = 165, // 160
    .v_front_porch = 1,
    .v_sync_pulse = 6,
    .v_back_porch = 25,
    .sync_polarity = 0b11000000, // negative
    .div = 3,
    .shX = 58, // as 1024x768@60 d3: same visible area and div
    .shY = 27,
};

video_mode_t mode_1024x768_48Hz_d4 = {
    .sys_freq = 210000,
    .pixel_freq = 52500000.0,
    .h_visible_area = 1024,
    .v_visible_area = 768,
    .whole_line = 1344,
    .whole_frame = 800,
    .h_front_porch = 24,
    .h_sync_pulse = 136,
    .h_back_porch = 160,
    .v_front_porch = 1,
    .v_sync_pulse = 6,
    .v_back_porch = 25,
    .sync_polarity = 0b11000000, // negative
    .div = 4,
    .shX = 99, // as 1024x768@60 d4
    .shY = 58,
};

video_mode_t *video_modes[VIDEO_MODE_MAX + 1] = {
    [MODE_640x480_60Hz] = &mode_640x480_60Hz,
    [MODE_720x576_50Hz] = &mode_720x576_50Hz,
    [MODE_800x600_60Hz] = &mode_800x600_60Hz,
    [MODE_1024x768_60Hz_d3] = &mode_1024x768_60Hz_d3,
    [MODE_1024x768_60Hz_d4] = &mode_1024x768_60Hz_d4,
    [MODE_1280x1024_60Hz_d3] = &mode_1280x1024_60Hz_d3,
    [MODE_1280x1024_60Hz_d4] = &mode_1280x1024_60Hz_d4,
    [MODE_720x576_48Hz] = &mode_720x576_48Hz,
    [MODE_1280x1024_48Hz_d3] = &mode_1280x1024_48Hz_d3,
    [MODE_640x480_48Hz] = &mode_640x480_48Hz,
    [MODE_1280x1024_48Hz_d4] = &mode_1280x1024_48Hz_d4,
    [MODE_800x600_48Hz] = &mode_800x600_48Hz,
    [MODE_1024x768_48Hz_d3] = &mode_1024x768_48Hz_d3,
    [MODE_1024x768_48Hz_d4] = &mode_1024x768_48Hz_d4,
};

// {standard rate, Pentagon rate} per resolution, in menu order
static const uint8_t modes_dvi[][2] = {
    {MODE_640x480_60Hz, MODE_640x480_48Hz},
    {MODE_720x576_50Hz, MODE_720x576_48Hz},
};

static const uint8_t modes_vga[][2] = {
    {MODE_640x480_60Hz, MODE_640x480_48Hz},
    {MODE_720x576_50Hz, MODE_720x576_48Hz},
    {MODE_800x600_60Hz, MODE_800x600_48Hz},
    {MODE_1024x768_60Hz_d3, MODE_1024x768_48Hz_d3},
    {MODE_1024x768_60Hz_d4, MODE_1024x768_48Hz_d4},
    {MODE_1280x1024_60Hz_d3, MODE_1280x1024_48Hz_d3},
    {MODE_1280x1024_60Hz_d4, MODE_1280x1024_48Hz_d4},
};

static const char *mode_names[VIDEO_MODE_MAX + 1] = {
    [MODE_640x480_60Hz] = "640X480@60",
    [MODE_720x576_50Hz] = "720X576@50",
    [MODE_800x600_60Hz] = "800X600@60",
    [MODE_1024x768_60Hz_d3] = "1024X768@60 D3",
    [MODE_1024x768_60Hz_d4] = "1024X768@60 D4",
    [MODE_1280x1024_60Hz_d3] = "1280X1024@60 D3",
    [MODE_1280x1024_60Hz_d4] = "1280X1024@60 D4",
    [MODE_720x576_48Hz] = "720X576@48.8",
    [MODE_1280x1024_48Hz_d3] = "1280X1024@48.8 D3",
    [MODE_640x480_48Hz] = "640X480@48.8",
    [MODE_1280x1024_48Hz_d4] = "1280X1024@48.8 D4",
    [MODE_800x600_48Hz] = "800X600@48.8",
    [MODE_1024x768_48Hz_d3] = "1024X768@48.8 D3",
    [MODE_1024x768_48Hz_d4] = "1024X768@48.8 D4",
};

static const uint8_t (*mode_pairs(video_out_type_t type, int *count))[2]
{
  if (type == DVI)
  {
    *count = count_of(modes_dvi);
    return modes_dvi;
  }

  *count = count_of(modes_vga);
  return modes_vga;
}

int video_mode_count(video_out_type_t type)
{
  int count;
  mode_pairs(type, &count);
  return count;
}

uint8_t video_mode_select(video_out_type_t type, int index, bool pentagon)
{
  int count;
  const uint8_t (*pairs)[2] = mode_pairs(type, &count);
  return pairs[index >= 0 && index < count ? index : 0][pentagon];
}

int video_mode_index(video_out_type_t type, uint8_t mode)
{
  int count;
  const uint8_t (*pairs)[2] = mode_pairs(type, &count);

  for (int i = 0; i < count; i++)
    if (pairs[i][0] == mode || pairs[i][1] == mode)
      return i;

  return -1;
}

bool video_mode_pentagon(uint8_t mode)
{
  for (int i = 0; i < (int)count_of(modes_dvi); i++)
    if (modes_dvi[i][1] == mode)
      return true;

  for (int i = 0; i < (int)count_of(modes_vga); i++)
    if (modes_vga[i][1] == mode)
      return true;

  return false;
}

const char *video_mode_name(uint8_t mode)
{
  return mode <= VIDEO_MODE_MAX && mode_names[mode] ? mode_names[mode] : "UNKNOWN";
}

uint8_t g_v_buf[V_BUF_SZ * 3];
