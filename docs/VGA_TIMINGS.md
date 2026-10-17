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

| Mode                                  | 640 x 480     | 720 x 576 @50 | 720 x 576 @48.8 |
|---------------------------------------|--------------:|--------------:|----------------:|
| **Screen refresh rate**               | 60 Hz         | 50 Hz         | 48.828125 Hz    |
| **Vertical refresh**                  | 31.469 kHz    | 31.25 kHz     | 31.25 kHz       |
| **Pixel frequency**                   | 25.175 MHz    | 27.0 MHz      | 27.0 MHz        |
|                                       |               |               |                 |
| **Horizontal timing (line)**          |               |               |                 |
| **Polarity of horizontal sync pulse** | negative      | negative      | negative        |
| **Scanline part**                     | **Pixels**    | **Pixels**    | **Pixels**      |
| **Visible area**                      | 640           | 720           | 720             |
| **Front porch**                       | 16            | 12            | 12              |
| **Sync pulse**                        | 96            | 64            | 64              |
| **Back porch**                        | 48            | 68            | 68              |
| **Whole line**                        | 800           | 864           | 864             |
|                                       |               |               |                 |
| **Vertical timing (frame)**           |               |               |                 |
| **Polarity of vertical sync pulse**   | negative      | negative      | negative        |
| **Frame part**                        | **Lines**     | **Lines**     | **Lines**       |
| **Visible area**                      | 480           | 576           | 576             |
| **Front porch**                       | 10            | 5             | 20              |
| **Sync pulse**                        | 2             | 5             | 5               |
| **Back porch**                        | 33            | 39            | 39              |
| **Whole frame**                       | 525           | 625           | 640             |

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
- **BUFFERING X3 is recommended** in this mode. With nearly matched input
  and output rates the buffer read/write phase is quasi-static: in X1 a
  tearing line, when it occurs, stays at one screen position and creeps
  slowly instead of flickering past. X3 removes it at the cost of up to
  two frames of latency. This is a recommendation, not enforced — X1
  remains selectable.

## Experimental VGA Modes at the Pentagon Frame Rate (48.828125 Hz)

Same idea as the DVI @48.8 mode, applied to the VGA output: pixel clocks
match the @60 variants, blanking is extended so that
`whole_line x whole_frame = pixel_freq x 20.48 ms` exactly. The extra
blanking goes into the front porches; sync widths and back porches match
the @60 variants, so the picture position is unchanged.

| Mode                | Pixel clock | Whole line | Whole frame | H rate      | V rate       |
|---------------------|------------:|-----------:|------------:|------------:|-------------:|
| 800 x 600 @48.8     | 40.0 MHz    | 1024       | 800         | 39.0625 kHz | 48.828125 Hz |
| 1024 x 768 @48.8 d3 | 65.0 MHz    | 1300       | 1024        | 50.0 kHz    | 48.828125 Hz |
| 1280 x 1024 @48.8 d3| 108.0 MHz   | 1728       | 1280        | 62.5 kHz    | 48.828125 Hz |

**Status: experimental / untested on hardware.** Many LCD scalers refuse
vertical rates below 50-56 Hz on the analog VGA input even when the same
panel accepts 48.8 Hz over HDMI — test with your display before relying
on these modes. A 640 x 480 variant is deliberately omitted: it would
require a 28.1 kHz line rate, below the minimum of typical VGA inputs.

On a native 1280 x 1024 panel the 1280 x 1024 @48.8 d3 mode is the
quality ceiling for this converter: Pentagon-native frame rate AND
integer 3x vertical scaling (320 x 3 = 960 lines) on the native pixel
grid, with no scaler resampling. BUFFERING X3 recommended, as above.
