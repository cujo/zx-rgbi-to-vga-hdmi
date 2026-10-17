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
};

// 576p at the Pentagon frame rate: 31250 / 640 = 48.828125 Hz (320 input
// lines x 64 us, one input line = two output lines). Free-running with
// constant timing; the residual crystal offset costs one repeated/skipped
// frame every few minutes. The 15 extra lines go to the front porch, so the
// picture position is unchanged. BUFFERING X3 is recommended (not enforced):
// with nearly matched rates the buffer phase is quasi-static, so in X1 a
// tearing line sits at one screen position instead of flickering past.
// Available on DVI and (experimental) VGA. See docs/VGA_TIMINGS.md.
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
};

// ── EXPERIMENTAL: VGA mode at the Pentagon frame rate ───────────────────
// whole_line x whole_frame = pixel_freq x 20.48 ms exactly, so the output
// frame rate is 48.828125 Hz = the Pentagon frame rate (320 lines x 64 us):
// no repeated/dropped frames beyond crystal tolerance. Pixel clock, sync
// widths and back porches match the @60 variant (picture position kept);
// the extra blanking goes into the FRONT porches. The display's VGA input
// must accept a 48.8 Hz vertical rate — many LCD scalers refuse < 50-56 Hz
// on VGA, hence experimental. BUFFERING X3 recommended (see @48.8 DVI mode).

// 108 MHz, 1728 x 1280 total, h 62.5 kHz, v 48.828125 Hz.
// On a native 1280x1024 panel this gives the Pentagon frame 1:1 in time
// AND integer 3x vertical scaling (320 x 3 = 960 lines) on the native grid.
video_mode_t mode_1280x1024_48Hz_d3 = {
    .sys_freq = 252000,
    .pixel_freq = 108000000.0,
    .h_visible_area = 1278, // 1280
    .v_visible_area = 1024,
    .whole_line = 1728,
    .whole_frame = 1280,
    .h_front_porch = 84,
    .h_sync_pulse = 111, // 112
    .h_back_porch = 255,
    .v_front_porch = 215,
    .v_sync_pulse = 3,
    .v_back_porch = 38,
    .sync_polarity = 0b00000000, // positive
    .div = 3,
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
};

uint8_t g_v_buf[V_BUF_SZ * 3];
