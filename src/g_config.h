#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "pico.h"
#include "pico/time.h"

#define FW_VER "v1.8.4"

#if PICO_RP2350
#define FW_VERSION FW_VER " (PICO 2)"
#else
#define FW_VERSION FW_VER
#endif

#define GIT_REPO_URL_1 "https://github.com/"
#define GIT_REPO_URL_2 "osemenyuk-114/"
#define GIT_REPO_URL_3 "zx-rgbi-to-vga-hdmi"

#if defined(BOARD_36LJU22) || defined(BOARD_RP2040_ZERO) || defined(BOARD_LEO_V2)

#define VIDEO_OUTPUT_AUTO_DETECT

#ifdef BOARD_36LJU22

#define HW_VERSION "36LJU22"
#define I2C_PIN_SDA 16
#define I2C_PIN_SCL 17
#define I2C_INST i2c0

#else

#ifdef BOARD_RP2040_ZERO
#define HW_VERSION "RP2040 Zero"
#else
#define HW_VERSION "LEO v2"
#endif

#define WS2812_LED_ENABLE
#define I2C_PIN_SDA 18
#define I2C_PIN_SCL 19
#define I2C_INST i2c1

#endif

#define DVI_PIN_D0 8
#define DVI_PIN_CLK0 (DVI_PIN_D0 + 6)
#define VGA_PIN_D0 DVI_PIN_D0
#define CAP_PIN_D0 0
#define OSD_BTN_UP 26
#define OSD_BTN_DOWN 27
#define OSD_BTN_SEL 28

#elif defined(BOARD_38LJE24) || defined(BOARD_38LJU24)

#ifdef BOARD_38LJE24
#define HW_VERSION "38LJE24"
#else
#define HW_VERSION "38LJU24"
#endif

#define VIDEO_OUTPUT_AUTO_DETECT
#define DVI_PINS_REVERSED // DVI pins are in reverse order (D0 is the last pin, D5 is the first)
#define DVI_PIN_D0 14
#define DVI_PIN_CLK0 12
#define VGA_PINS_SWAPPED // VGA R and B pins are swapped
#define VGA_PIN_D0 DVI_PIN_CLK0
#define CAP_PIN_D0 5
#define OSD_BTN_UP 26
#define OSD_BTN_DOWN 27
#define OSD_BTN_SEL 28
#define I2C_PIN_SDA 20
#define I2C_PIN_SCL 21
#define I2C_INST i2c0
#define PS2_PIN_DATA 1
#define PS2_PIN_CLK 0
#define KBD_PIN_DATA 2
#define KBD_PIN_CLK 3
#define KBD_PIN_STB 4

#elif defined(BOARD_11XGA24_1) || defined(BOARD_11XGA24_2)

#define HW_VERSION "11XGA24"
#define DVI_PINS_REVERSED // DVI pins are in reverse order (D0 is the last pin, D5 is the first)
#define VGA_PINS_SWAPPED  // VGA R and B pins are swapped

#ifdef BOARD_11XGA24_1

#define DVI_PIN_D0 2
#define DVI_PIN_CLK0 0
#define VGA_PIN_D0 8

#else

#define DVI_PIN_D0 10
#define DVI_PIN_CLK0 8
#define VGA_PIN_D0 0

#endif

#define CAP_PIN_D0 16
#define OSD_BTN_UP 26
#define OSD_BTN_DOWN 27
#define OSD_BTN_SEL 28

#elif defined(BOARD_LEO_V3) || defined(BOARD_LEO_V3_2040BT)

#define VIDEO_OUTPUT_AUTO_DETECT
#define SPI_KB_ENABLE
#define HW_CONFIG_ENABLE

#ifdef BOARD_LEO_V3

#define HW_VERSION "LEO v3.0"
#define WS2812_LED_ENABLE
#define I2C_PIN_SDA 18
#define I2C_PIN_SCL 19
#define I2C_INST i2c1

#else

#define HW_VERSION "LEO v3.0.2040BT"
#define I2C_PIN_SDA 16
#define I2C_PIN_SCL 17
#define I2C_INST i2c0
// Not used in this board variant, but defined and pulled up to avoid Gotek menu navigation issues
#define HW_GOTEK_BTN_L 18
#define HW_GOTEK_BTN_R 19

#endif

#define DVI_PIN_D0 8
#define DVI_PIN_CLK0 (DVI_PIN_D0 + 6)
#define VGA_PIN_D0 DVI_PIN_D0
#define CAP_PIN_D0 0

// Buttons are not connected to GPIOs on LEO v3.0, so use the same GPIOs as the ROM bank pins for OSD buttons
// Inputs are internally overriden and are read as HIGH to avoid OSD menu navigation issues
#define NO_OSD_BUTTONS
#define OSD_BTN_UP 20
#define OSD_BTN_DOWN 21
#define OSD_BTN_SEL 22

#define PS2_PIN_DATA 29
#define PS2_PIN_CLK 28
#define KBD_PIN_DATA 7
#define KBD_PIN_CLK 26
#define KBD_PIN_STB 27

#define HW_PIN_ROM_BANK_D0 20
#define HW_PIN_ROM_BANK_D1 21
#define HW_PIN_ROM_BANK_D2 22
#define HW_PIN_GOTEK_DRIVE_D0 23
#define HW_PIN_GOTEK_DRIVE_D1 24
#define HW_PIN_RAM_SIZE 25

#elif defined(BOARD_09LJV23)

#define HW_VERSION "09LJV23"
#define VIDEO_OUTPUT_AUTO_DETECT
#define DVI_PIN_D0 7
#define DVI_PIN_CLK0 (DVI_PIN_D0 + 6)
#define VGA_PIN_D0 DVI_PIN_D0
#define CAP_PIN_D0 0
#define OSD_BTN_UP 26
#define OSD_BTN_DOWN 27
#define OSD_BTN_SEL 28
#define I2C_PIN_SDA 16
#define I2C_PIN_SCL 17
#define I2C_INST i2c0

#else

#error "Unsupported board variant. Please define the board variant in CMakeLists.txt."

#endif

// capture pins bit positions
#define CAP_B 0
#define CAP_G 1
#define CAP_R 2
#define CAP_I 3
#define CAP_HS 4
#define CAP_VS 5
#define CAP_F 6

// capture GPIO pin numbers
#define CAP_HS_PIN (CAP_PIN_D0 + CAP_HS)
#define CAP_VS_PIN (CAP_PIN_D0 + CAP_VS)
#define CAP_F_PIN (CAP_PIN_D0 + CAP_F)

// PIO and SM for DVI
#define PIO_DVI pio0
#define DREQ_PIO_DVI DREQ_PIO0_TX0
#define SM_DVI 0
#define SM_DVI_CONV (SM_DVI + 1)

// PIO and SM for VGA
#define PIO_VGA pio0
#define DREQ_PIO_VGA DREQ_PIO0_TX0
#define SM_VGA 0

// capture PIO and SM
#define PIO_CAP pio1
#define DREQ_PIO_CAP DREQ_PIO1_RX0
#define SM_CAP 0

#define PIO_PS2 pio1
// Use IRQ index 1 (PIO1_IRQ_1) to avoid conflicts with capture SM
#define PIO_PS2_IRQ PIO1_IRQ_1
#define SM_PS2 1

#define PIO_CH446Q pio1
#define SM_CH446Q 2

#define PIO_SPI pio1
#define SM_SPI 2

// PIO and SM for WS2812B RGB LED (on PIO0 to avoid PIO1 memory pressure)
#define PIO_WS2812 pio0
#define SM_WS2812 2

typedef enum video_out_type_t
{
  OUTPUT_TYPE_MIN,
  DVI = OUTPUT_TYPE_MIN,
  VGA,
  OUTPUT_TYPE_MAX = VGA,
} video_out_type_t;

typedef enum video_out_mode_t
{
  VIDEO_MODE_MIN,
  MODE_640x480_60Hz = VIDEO_MODE_MIN,
  MODE_720x576_50Hz,
  MODE_800x600_60Hz,
  MODE_1024x768_60Hz_d3,
  MODE_1024x768_60Hz_d4,
  MODE_1280x1024_60Hz_d3,
  MODE_1280x1024_60Hz_d4,
  // modes at the Pentagon frame rate (48.828125 Hz), appended so saved mode indices stay valid
  MODE_720x576_48Hz,      // DVI and VGA
  MODE_1280x1024_48Hz_d3, // VGA
  MODE_640x480_48Hz,      // DVI and VGA
  MODE_1280x1024_48Hz_d4, // VGA
  MODE_800x600_48Hz,      // VGA
  MODE_1024x768_48Hz_d3,  // VGA
  MODE_1024x768_48Hz_d4,  // VGA
  VIDEO_MODE_MAX = MODE_1024x768_48Hz_d4,
} video_out_mode_t;

#define VIDEO_MODE_DVI_MAX MODE_720x576_50Hz
// the standard rate modes come first: one per resolution and div, each has its Pentagon rate pair after them
#define VIDEO_MODE_STD_MAX MODE_1280x1024_60Hz_d4

typedef enum cap_sync_mode_t
{
  SYNC_MODE_MIN,
  SELF = SYNC_MODE_MIN,
  EXT,
  SYNC_MODE_MAX = EXT,
} cap_sync_mode_t;

#ifdef OSD_FF_ENABLE
typedef struct ff_osd_config_t
{
  bool enabled;
  bool i2c_protocol; // false = LCD_HD44780, true = FlashFloppy
  uint16_t cols;
  uint16_t rows;
  uint8_t h_position; // 1=left, 2=left-center, 3=center, 4=center-right, 5=right
  bool v_position;    // false = at the top, true = at the bottom of the screen
} ff_osd_config_t;

#define FF_OSD_COLUMNS_MIN 16
#define FF_OSD_COLUMNS_MAX 40
#define FF_OSD_ROWS_MIN 2
#define FF_OSD_ROWS_MAX 4

#endif

#ifdef HW_CONFIG_ENABLE
typedef struct hw_config_t
{
  uint8_t rom_bank;    // 1-8
  bool ram_size;       // false = 128KB, true = 1024KB
  uint8_t gotek_drive; // 0 = OFF, 1 = A, 2 = B
} hw_config_t;

#define HW_ROM_BANK_MIN 1
#define HW_GOTEK_DRIVE_MIN 0

#define HW_ROM_BANK_MAX 8
#define HW_GOTEK_DRIVE_MAX 2

#define HW_ROM_BANK_DEF 1
#define HW_RAM_SIZE_DEF true
#define HW_GOTEK_DRIVE_DEF 1

#endif

// scanline levels; DVI uses all of them, VGA only off / on (black lines)
// percentages are the darkening strength: 25% keeps 75% of the line brightness, 100% is black
typedef enum scanlines_level_t
{
  SCANLINES_OFF = 0,
  SCANLINES_ADAPTIVE, // dark colors darkened more than bright ones, like a CRT beam spot widening with brightness
  SCANLINES_25,
  SCANLINES_50,
  SCANLINES_75,
  SCANLINES_100,
  SCANLINES_BEAM, // DVI: gap line lit by the beam profiles of both neighbor lines (crt-geom model)
  SCANLINES_LEVEL_MAX = SCANLINES_BEAM
} scanlines_level_t;

// settings.crt_options: bits 0-1 BEAM beam width (beam_width_t), bits 2-3 normal level (normal_level_t). The byte is
// former struct padding, 0 in settings saved by older firmware: both defaults are 0.
#define CRTOPT_WIDTH(o) ((o) & 0x03)
#define CRTOPT_NORMAL(o) (((o) >> 2) & 0x03)
#define CRT_OPTIONS(width, normal) ((uint8_t)(((normal) << 2) | (width)))

// BEAM scanlines: beam width (distance scale of the beam profile, see the beam gap tables in crt.c). Cycling +1 goes
// TV, SOFT, PVM, SHARP.

typedef enum beam_width_t
{
  BEAM_WIDTH_TV = 0, // 0.40: a good consumer TV at home brightness
  BEAM_WIDTH_SOFT,   // 0.42: small or cheap TV, or high brightness
  BEAM_WIDTH_PVM,    // 0.33: sharp studio monitor
  BEAM_WIDTH_SHARP,  // 0.38: sharp-beam TV (Trinitron-like)
  BEAM_WIDTH_COUNT
} beam_width_t;

// DVI phosphor mask, settings.grille_mode: bits 0-3 strength (grille_mode_t), bits 4-5 type (grille_mask_t).
// One of the two output pixels of each source pixel is darkened by the strength, drawing vertical lines; only with
// scanlines on. 0 (OFF, mono grille) is the default: the field is former struct padding, 0 in older saves.
typedef enum grille_mode_t
{
  GRILLE_OFF = 0,
  GRILLE_5,
  GRILLE_10,
  GRILLE_15,
  GRILLE_30,
  GRILLE_45,
  GRILLE_COUNT
} grille_mode_t;

// mask type flags: MG = color mask (first pixel keeps R and B, second keeps G, like an RGB triad split in two),
// SLOT = the pattern is swapped on scanline (gap) rows, a staggered slot-mask-like layout; needs scanlines on
typedef enum grille_mask_t
{
  GRILLE_MASK_MONO = 0,
  GRILLE_MASK_MG = 1,
  GRILLE_MASK_SLOT = 2,
  GRILLE_MASK_MG_SLOT = 3,
  GRILLE_MASK_COUNT
} grille_mask_t;

#define GRILLE_LEVEL(m) ((m) & 0x0f)
#define GRILLE_MASK(m) (((m) >> 4) & 0x03)
#define GRILLE_MODE(level, mask) ((uint8_t)(((mask) << 4) | (level)))

// DVI level of normal (non-BRIGHT) colors with scanlines on (off: 170, the converter's original level); BRIGHT is
// 255. A higher level makes up for the light the scanlines take. Emulators use about 85% (0xD7 / 0xD8),
// measurements of real machines are lower and differ by model. 0 = 80% is the default: with scanlines it keeps
// about the average light of normal colors without them (67%). Cycling +1 goes 80, 85, 67, 75%.
typedef enum normal_level_t
{
  NORMAL_LEVEL_205 = 0, // 80%
  NORMAL_LEVEL_216,     // 85%
  NORMAL_LEVEL_170,     // 67%
  NORMAL_LEVEL_192,     // 75%
  NORMAL_LEVEL_COUNT
} normal_level_t;

typedef struct settings_t
{
  video_out_type_t video_out_type;
  video_out_mode_t video_out_mode;
  uint8_t scanlines_mode; // scanlines_level_t; was bool, so a saved 1 ("on") reads as SCANLINES_ADAPTIVE
  bool buffering_mode;
  bool video_sync_mode;
  cap_sync_mode_t cap_sync_mode;
  uint8_t crt_options; // CRTOPT_WIDTH, CRTOPT_NORMAL; former padding before frequency (layout and size unchanged)
  uint8_t grille_mode; // grille_mode_t; the other former padding byte before frequency
  uint32_t frequency;
  int8_t ext_clk_divider;
  int8_t delay;
  int16_t shX;
  int16_t shY;
  uint8_t pin_inversion_mask;
  uint8_t options; // OPTION_* flags; former padding before ff_osd_config / crc, so the layout does not change
#ifdef OSD_FF_ENABLE
  ff_osd_config_t ff_osd_config;
#endif
#ifdef HW_CONFIG_ENABLE
  hw_config_t hw_config;
#endif
  uint32_t crc;
  // shift (shX, shY) per resolution and div, restored on a mode change. Indexed by the standard rate mode: its
  // Pentagon rate pair shares the entry. After crc with its own check value, so settings saved without it keep
  // everything else; an invalid table is filled from the mode presets.
  uint8_t mode_shX[VIDEO_MODE_STD_MAX + 1];
  uint8_t mode_shY[VIDEO_MODE_STD_MAX + 1];
  uint32_t mode_crc;
} settings_t;

// settings.options flags (0 in settings saved by older firmware: all off). The OSD item BUTTONS sets them together:
// DEFAULT (none), MODE SWITCH (OPTION_UPDOWN_SWITCH_MODE), SLIM EXT KBD (both); see buttons_mode().
// OPTION_UPDOWN_SWITCH_MODE. Off: UP / DOWN outside the menu work as before (open the menu; with the FF OSD they go
// to the Gotek at once, with repeat). On: they step through the resolutions (with the FF OSD on a 1 s hold; short
// taps go to the Gotek on release).
#define OPTION_UPDOWN_SWITCH_MODE 0x01
// OPTION_SLIM_EXT_KBD (only with OPTION_UPDOWN_SWITCH_MODE). For the three button lines driven by the Fn + arrow
// keys of the Slim ZX-Ext-Keyboard through its Gotek button connector (Fn + RIGHT pulls UP, Fn + LEFT pulls DOWN,
// Fn + DOWN pulls SEL, Fn + UP pulls UP and DOWN together). Inside the menu the arrows then work as arrows:
// Fn + UP / DOWN move, Fn + RIGHT selects, Fn + LEFT goes back. Outside the menu the lines are read as UP / DOWN /
// SEL.
#define OPTION_SLIM_EXT_KBD 0x02
#define OPTIONS_MASK (OPTION_UPDOWN_SWITCH_MODE | OPTION_SLIM_EXT_KBD)
#define UPDOWN_SWITCH_MODE (settings.options & OPTION_UPDOWN_SWITCH_MODE)
#define SLIM_EXT_KBD (settings.options & OPTION_SLIM_EXT_KBD)

// BUTTONS (settings.options): DEFAULT, MODE SWITCH, SLIM EXT KBD
enum
{
  BUTTONS_DEFAULT,
  BUTTONS_MODE_SWITCH,
  BUTTONS_SLIM_EXT_KBD,
  BUTTONS_MODE_COUNT
};

typedef struct video_mode_t
{
  uint32_t sys_freq;
  float pixel_freq;
  uint16_t h_visible_area;
  uint16_t v_visible_area;
  uint16_t whole_line;
  uint16_t whole_frame;
  uint8_t h_front_porch;
  uint8_t h_sync_pulse;
  uint8_t h_back_porch;
  uint8_t v_front_porch;
  uint8_t v_sync_pulse;
  uint8_t v_back_porch;
  uint8_t sync_polarity;
  uint8_t div;
  int16_t shX;
  int16_t shY;
} video_mode_t;

extern video_mode_t *video_modes[VIDEO_MODE_MAX + 1];

// The modes of an output are pairs {standard rate, Pentagon rate (48.8 Hz)} in menu order; the menus pick the
// resolution (pair index) and the rate separately. A mode's rate follows from the mode, no setting holds it.
int video_mode_count(video_out_type_t type);                                // pairs of the output
uint8_t video_mode_select(video_out_type_t type, int index, bool pentagon); // the mode of a pair
int video_mode_index(video_out_type_t type, uint8_t mode);                  // its pair, -1 if not of this output
bool video_mode_pentagon(uint8_t mode);                                     // a Pentagon rate mode
uint8_t video_mode_standard(uint8_t mode);                                  // its standard rate pair (or itself)
const char *video_mode_name(uint8_t mode);                                  // OSD name, e.g. "640X480@48.8"

extern uint8_t g_v_buf[];

// settings MIN values
#define VIDEO_OUT_TYPE_MIN OUTPUT_TYPE_MIN
#define VIDEO_OUT_MODE_MIN VIDEO_MODE_MIN
#define CAP_SYNC_MODE_MIN SYNC_MODE_MIN
#define FREQUENCY_MIN 6000000
#define EXT_CLK_DIVIDER_MIN 1
#define DELAY_MIN 0
#define shX_MIN 0
#define shY_MIN 0

// settings MAX values
#define VIDEO_OUT_TYPE_MAX OUTPUT_TYPE_MAX
#define VIDEO_OUT_MODE_MAX VIDEO_MODE_MAX
#define CAP_SYNC_MODE_MAX SYNC_MODE_MAX
#define FREQUENCY_MAX 8000000
#define EXT_CLK_DIVIDER_MAX 5
#define DELAY_MAX 31
#define shX_MAX 100
#define shY_MAX 100
#define PIN_INVERSION_MASK 0x7f

// settings DEFAULT values
#define VIDEO_OUT_TYPE_DEF VGA
#define VIDEO_OUT_MODE_DEF MODE_640x480_60Hz
#define CAP_SYNC_MODE_DEF SELF
#define FREQUENCY_DEF 7000000
#define EXT_CLK_DIVIDER_DEF 2
#define DELAY_DEF 15
#define shX_DEF 68
#define shY_DEF 34
#define PIN_INVERSION_MASK_DEF 0x00

// video timing
// 64 us - duration of a single scanline, 12 us - combined duration of the front porch, horizontal sync pulse, and back porch
#define ACTIVE_VIDEO_TIME (64 - 12)

// video buffer
// width of the video buffer is calculated as max captured line length in pixels
#define V_BUF_W (ACTIVE_VIDEO_TIME * (FREQUENCY_MAX / 1000000))
#define V_BUF_H 304
#define V_BUF_SZ (V_BUF_H * V_BUF_W / 2)

// select scanline thickness for the 1024x768 and 1280x1024 DIV4 video modes
// thin - show scanline once every four lines
// thick - show scanline twice in four lines
#define SCANLINES_USE_THIN

#if defined(OSD_MENU_ENABLE) || defined(OSD_FF_ENABLE)
#define OSD_ENABLE
#endif

// Keyboard subsystem meta-flag: enabled if any keyboard backend is enabled
#if defined(PS2_KBD_ENABLE) || defined(USB_KBD_ENABLE)
#define KBD_ENABLE
#endif