# Video Timings

## Standard VGA Timing Parameters

| Mode                                  | 640 x 480     | 800 x 600     | 1024 x 768    | 1280 x 1024   |
|---------------------------------------|--------------:|--------------:|--------------:|--------------:|
| **Screen refresh rate**               | 60 Hz         | 60 Hz         | 60 Hz         | 60 Hz         |
| **Vertical refresh**                  | 31.469 kHz    | 37.879 kHz    | 48.363 kHz    | 63.981 kHz    |
| **Pixel frequency**                   | 25.175 MHz    | 40.0 MHz      | 65.0 MHz      | 108.0 MHz     |
|                                       |               |               |               |               |
| **Horizontal timing (line)**          |               |               |               |               |
| **Polarity of horizontal sync pulse** | negative      | positive      | negative      | positive      |
| **Scanline part**                     | **Pixels**    | **Pixels**    | **Pixels**    | **Pixels**    |
| **Visible area**                      | 640           | 800           | 1024          | 1280          |
| **Front porch**                       | 16            | 40            | 24            | 48            |
| **Sync pulse**                        | 96            | 128           | 136           | 112           |
| **Back porch**                        | 48            | 88            | 160           | 248           |
| **Whole line**                        | 800           | 1056          | 1344          | 1688          |
|                                       |               |               |               |               |
| **Vertical timing (frame)**           |               |               |               |               |
| **Polarity of vertical sync pulse**   | negative      | positive      | negative      | positive      |
| **Frame part**                        | **Lines**     | **Lines**     | **Lines**     | **Lines**     |
| **Visible area**                      | 480           | 600           | 768           | 1024          |
| **Front porch**                       | 10            | 1             | 3             | 1             |
| **Sync pulse**                        | 2             | 4             | 6             | 3             |
| **Back porch**                        | 33            | 23            | 29            | 38            |
| **Whole frame**                       | 525           | 628           | 806           | 1066          |

## Extended Timing Parameters for Supported DVI Video Modes

| Mode                                  | 640 x 480     | 720 x 576 @50 | 720 x 576 @48.8 | 640 x 480 @48.8   |
|---------------------------------------|--------------:|--------------:|----------------:|------------------:|
| **Screen refresh rate**               | 60 Hz         | 50 Hz         | 48.828125 Hz    | 48.828125 Hz      |
| **Vertical refresh**                  | 31.469 kHz    | 31.25 kHz     | 31.25 kHz       | 31.25 kHz         |
| **Pixel frequency**                   | 25.175 MHz    | 27.0 MHz      | 27.0 MHz        | 25.0 MHz          |
|                                       |               |               |                 |                   |
| **Horizontal timing (line)**          |               |               |                 |                   |
| **Polarity of horizontal sync pulse** | negative      | negative      | negative        | negative          |
| **Scanline part**                     | **Pixels**    | **Pixels**    | **Pixels**      | **Pixels**        |
| **Visible area**                      | 640           | 720           | 720             | 640               |
| **Front porch**                       | 16            | 12            | 12              | 16                |
| **Sync pulse**                        | 96            | 64            | 64              | 96                |
| **Back porch**                        | 48            | 68            | 68              | 48                |
| **Whole line**                        | 800           | 864           | 864             | 800               |
|                                       |               |               |                 |                   |
| **Vertical timing (frame)**           |               |               |                 |                   |
| **Polarity of vertical sync pulse**   | negative      | negative      | negative        | negative          |
| **Frame part**                        | **Lines**     | **Lines**     | **Lines**       | **Lines**         |
| **Visible area**                      | 480           | 576           | 576             | 480               |
| **Front porch**                       | 10            | 5             | 20              | 125               |
| **Sync pulse**                        | 2             | 5             | 5               | 2                 |
| **Back porch**                        | 33            | 39            | 39              | 33                |
| **Whole frame**                       | 525           | 625           | 640             | 640               |

### Notes on 720 x 576 @48.8 (Pentagon mode)

This mode targets the Pentagon ZX Spectrum clone, whose frame is
320 lines x 64 us = 20.48 ms (48.828125 Hz). The standard 576p50 output
never matches that rate, so about once a second a captured frame is
repeated or dropped — visible as a periodic stutter on smooth scrolling.

The @48.8 mode keeps the 576p50 pixel clock and line timing but extends
the frame to 640 lines: 31250 / 640 = 48.828125 Hz, exactly the Pentagon
frame rate (one input line = two output lines). The 15 extra lines go
into the vertical front porch, so the picture position is identical to
576p50. The output is free-running with constant timing; the residual
crystal tolerance (tens of ppm) costs one repeated or skipped frame every
few minutes.

- The display (or HDMI-to-VGA converter) must accept a 48.8 Hz vertical
  rate. If there is no picture in this mode, fall back to 720 x 576 @50
  or 640 x 480 @60.
- **BUFFERING is X3** in the @48.8 modes, whatever the setting says (the
  setting is kept for the standard rate). With nearly matched input and
  output rates the buffer read/write phase is quasi-static: in X1 a tearing
  line, when it occurs, stays at one screen position and creeps slowly
  instead of flickering past. X3 removes it at the cost of up to two frames
  of latency.

### Notes on 640 x 480 @48.8 (Pentagon mode)

The same idea at 640 x 480 resolution: the frame is extended to 640
lines at a 31.25 kHz line rate, so the output runs at exactly
48.828125 Hz and one input line maps to two output lines.
For a whole line to take exactly 32 us the pixel clock is 25.0 MHz
(sys_clk 250 MHz) instead of 25.2 MHz. Horizontal timing, sync widths and
back porches are the 640 x 480 @60 values; the 115 extra lines go into the
vertical front porch, so the picture position is unchanged. DVI and VGA. The
caveats of the 720 x 576 @48.8 mode apply: the display must accept 48.8 Hz,
and BUFFERING is X3.

## VGA Modes at the Pentagon Frame Rate (48.828125 Hz)

Same idea as the DVI @48.8 mode, applied to the VGA output. Every VGA
resolution has a Pentagon-rate twin; the menus pick the resolution and the
rate (STANDARD / 48.8) separately.

- **640 x 480 @48.8** — the DVI mode with identical timing:
  25.0 MHz pixel clock, 31.25 kHz line rate, 640 lines.
- **720 x 576 @50 and @48.8** — the DVI modes with identical timing (27 MHz,
  sys_clk 270 MHz, PIO divider 270 x 2 / 27 = 20). 576 lines are the 288
  source lines doubled: the whole frame with the border, without scaling.
  For monitors that know 576p; others may take it for 800 x 600 (see below).
- **1280 x 1024 @48.8 d3** — VESA 1280 x 1024 shape (the 1680-pixel line of
  the @60 d4 mode, @60 sync widths and back porches) at a lower pixel clock,
  89.25 MHz, so that `whole_line x whole_frame = pixel_freq x 20.48 ms`
  exactly. The frame has 1088 lines instead of 1066; the 22 extra lines go
  into the vertical front porch (23 lines instead of 1).
  sys_clk is 238 MHz and the PIO divider 238 x 3 / 89.25 = 8, an integer.
  An earlier version kept 108 MHz with a 1728 x 1280 total (62.5 kHz) and
  was reported "Out of range" on two monitors that accept the other @48.8
  modes on VGA.
- **800 x 600 @48.8** — a monitor tells VGA modes apart by line rate and lines
  per frame; the 31.25 kHz / 640-line signal of 640 x 480 and 720 x 576 @48.8
  is closest to 800 x 600 (625-628 lines) in its tables, so some monitors
  (a BenQ 1280 x 1024) show those modes as 800 x 600 with broken geometry
  (720 x 576 @50, 625 lines, likely too).
  This mode is a real 800 x 600 with the VESA 800 x 600 line (1056 pixels,
  positive syncs) at 33 MHz, so the line length the monitor assumes is right:
  1056 x 640 total, sys_clk 264 MHz, PIO divider 8. It shows 400 x 300 source
  pixels: narrow side bars at a 7 MHz capture, nearly the whole border.
- **1280 x 1024 @48.8 d4** — 4x scaling: 320 source pixels fill the width,
  256 of the 320 source lines are shown (as in 1280 x 1024 @60 d4). No DIV4
  timing is exact with an integer PIO divider at a reachable sys_clk, so it
  runs 15 ppm fast (one dropped frame in about 23 minutes, below crystal
  tolerance): 1680 x 1089 total at 89.333 MHz, sys_clk 268 MHz, divider 12.
- **1024 x 768 @48.8 d3 / d4** — the VESA 1024 x 768 line (1344 pixels,
  negative syncs) at 52.5 MHz, 1344 x 800 total (800 lines instead of 806,
  close enough for the monitor's 1024 x 768 guess; vertical back porch 25
  instead of 29, front porch 1), sys_clk 210 MHz, PIO divider 12 (d3) / 16
  (d4), 48.828125 Hz exactly.

| Mode                | Pixel clock | Whole line | Whole frame | H rate      | V rate       |
|---------------------|------------:|-----------:|------------:|------------:|-------------:|
| 640 x 480 @48.8     | 25.0 MHz    | 800        | 640         | 31.25 kHz   | 48.828125 Hz |
| 1280 x 1024 @48.8 d3| 89.25 MHz   | 1680       | 1088        | 53.125 kHz  | 48.828125 Hz |
| 1280 x 1024 @48.8 d4| 89.333 MHz  | 1680       | 1089        | 53.175 kHz  | 48.828837 Hz |
| 800 x 600 @48.8     | 33.0 MHz    | 1056       | 640         | 31.25 kHz   | 48.828125 Hz |
| 1024 x 768 @48.8    | 52.5 MHz    | 1344       | 800         | 39.0625 kHz | 48.828125 Hz |

**Status:** 640 x 480 and 1280 x 1024 d3 @48.8 were reported working on two
LCD monitors over VGA (one takes 640 x 480 @48.8 for 800 x 600, hence the
800 x 600 mode); 800 x 600, 1024 x 768 and 1280 x 1024 d4 @48.8 are
experimental (untested). Many LCD scalers refuse vertical rates below 50-56 Hz on the
analog VGA input even when the same panel accepts 48.8 Hz over HDMI — test
with your display before relying on these modes.

On a native 1280 x 1024 panel the 1280 x 1024 @48.8 d3 mode is the
quality ceiling for this converter: Pentagon-native frame rate AND
integer 3x vertical scaling (320 x 3 = 960 lines) on the native pixel
grid, with no scaler resampling. BUFFERING is X3, as above.
