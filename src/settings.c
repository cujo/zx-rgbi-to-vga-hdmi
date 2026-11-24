#include <stddef.h>

#include "hardware/flash.h"
#include "hardware/sync.h"

#include "g_config.h"
#include "settings.h"

extern volatile bool stop_core1;
extern volatile bool core1_inactive;
extern volatile bool restart_capture;

extern settings_t settings;

settings_t default_settings = {
    .video_out_type = VIDEO_OUT_TYPE_DEF,
    .video_out_mode = VIDEO_OUT_MODE_DEF,
    .scanlines_mode = SCANLINES_OFF,
    .buffering_mode = false,
    .cap_sync_mode = CAP_SYNC_MODE_DEF,
    .crt_options = CRT_OPTIONS(BEAM_WIDTH_TV, NORMAL_LEVEL_205),
    .grille_mode = GRILLE_OFF,
    .frequency = FREQUENCY_DEF,
    .ext_clk_divider = EXT_CLK_DIVIDER_DEF,
    .delay = DELAY_DEF,
    .shX = shX_DEF,
    .shY = shY_DEF,

#if defined(BOARD_LEO_V2) || defined(BOARD_LEO_V3) || defined(BOARD_LEO_V3_2040BT)
    .video_sync_mode = true,
    .pin_inversion_mask = (1 << CAP_VS) | PIN_INVERSION_MASK_DEF,
#else
    .video_sync_mode = false,
    .pin_inversion_mask = PIN_INVERSION_MASK_DEF,
#endif

#ifdef OSD_FF_ENABLE
    .ff_osd_config = {

#if defined(BOARD_LEO_V2) || defined(BOARD_LEO_V3) || defined(BOARD_LEO_V3_2040BT)
        .enabled = true,
#else
        .enabled = false,
#endif

        .i2c_protocol = true,
        .cols = FF_OSD_COLUMNS_MAX,
        .rows = 3,
        .h_position = 3,
        .v_position = false,
    },
#endif

#if defined(BOARD_LEO_V3) || defined(BOARD_LEO_V3_2040BT)
    .hw_config = {
        .rom_bank = HW_ROM_BANK_DEF,
        .ram_size = HW_RAM_SIZE_DEF,
        .gotek_drive = HW_GOTEK_DRIVE_DEF,
    },
#endif

    .crc = 0,
};

void check_settings(settings_t *settings)
{
  if (settings->video_out_type > VIDEO_OUT_TYPE_MAX ||
      settings->video_out_type < VIDEO_OUT_TYPE_MIN)
    settings->video_out_type = VIDEO_OUT_TYPE_DEF;

  if (video_mode_index(settings->video_out_type, settings->video_out_mode) < 0) // not a mode of this output
    settings->video_out_mode = VIDEO_OUT_MODE_DEF;

  if (settings->scanlines_mode > SCANLINES_LEVEL_MAX)
    settings->scanlines_mode = SCANLINES_OFF;

  settings->crt_options &= 0x0f; // beam width and normal level (both always valid)

  if (GRILLE_LEVEL(settings->grille_mode) >= GRILLE_COUNT || (settings->grille_mode & 0xc0))
    settings->grille_mode = GRILLE_OFF;


  if (settings->cap_sync_mode > CAP_SYNC_MODE_MAX ||
      settings->cap_sync_mode < CAP_SYNC_MODE_MIN)
    settings->cap_sync_mode = CAP_SYNC_MODE_DEF;

  if (settings->frequency > FREQUENCY_MAX ||
      settings->frequency < FREQUENCY_MIN)
    settings->frequency = FREQUENCY_DEF;

  if (settings->ext_clk_divider > EXT_CLK_DIVIDER_MAX ||
      settings->ext_clk_divider < EXT_CLK_DIVIDER_MIN)
    settings->ext_clk_divider = EXT_CLK_DIVIDER_DEF;

  if (settings->delay > DELAY_MAX ||
      settings->delay < DELAY_MIN)
    settings->delay = DELAY_DEF;

  if (settings->shX > shX_MAX ||
      settings->shX < shX_MIN)
    settings->shX = shX_DEF;

  if (settings->shY > shY_MAX ||
      settings->shY < shY_MIN)
    settings->shY = shY_DEF;

  for (int m = 0; m <= VIDEO_MODE_STD_MAX; m++)
  {
    if (settings->mode_shX[m] > shX_MAX)
      settings->mode_shX[m] = video_modes[m]->shX;

    if (settings->mode_shY[m] > shY_MAX)
      settings->mode_shY[m] = video_modes[m]->shY;
  }

  settings->options &= OPTIONS_MASK;

  // SLIM EXT KBD includes MODE SWITCH
  if (settings->options & OPTION_SLIM_EXT_KBD)
    settings->options |= OPTION_UPDOWN_SWITCH_MODE;

  // the current shift belongs to the current mode
  uint8_t std_mode = video_mode_standard(settings->video_out_mode);
  settings->mode_shX[std_mode] = settings->shX;
  settings->mode_shY[std_mode] = settings->shY;

  if (settings->pin_inversion_mask & ~PIN_INVERSION_MASK)
  {
    settings->pin_inversion_mask = PIN_INVERSION_MASK_DEF;
    settings->scanlines_mode = SCANLINES_OFF;
    settings->buffering_mode = false;
    settings->video_sync_mode = false;
  }

#ifdef OSD_FF_ENABLE
  if (settings->ff_osd_config.cols < FF_OSD_COLUMNS_MIN)
    settings->ff_osd_config.cols = FF_OSD_COLUMNS_MIN;

  if (settings->ff_osd_config.cols > FF_OSD_COLUMNS_MAX)
    settings->ff_osd_config.cols = FF_OSD_COLUMNS_MAX;

  if (settings->ff_osd_config.rows < FF_OSD_ROWS_MIN)
    settings->ff_osd_config.rows = FF_OSD_ROWS_MIN;

  if (settings->ff_osd_config.rows > FF_OSD_ROWS_MAX)
    settings->ff_osd_config.rows = FF_OSD_ROWS_MAX;

  if (settings->ff_osd_config.h_position < 1)
    settings->ff_osd_config.h_position = 1;

  if (settings->ff_osd_config.h_position > 5)
    settings->ff_osd_config.h_position = 5;
#endif

#if defined(BOARD_LEO_V3) || defined(BOARD_LEO_V3_2040BT)
  if (settings->hw_config.rom_bank < HW_ROM_BANK_MIN ||
      settings->hw_config.rom_bank > HW_ROM_BANK_MAX)
    settings->hw_config.rom_bank = HW_ROM_BANK_DEF;

  if (settings->hw_config.gotek_drive > HW_GOTEK_DRIVE_MAX)
    settings->hw_config.gotek_drive = HW_GOTEK_DRIVE_DEF;
#endif

  settings->crc = calculate_settings_crc(settings);
  settings->mode_crc = calculate_mode_shift_crc(settings);
}

// the shift table from the mode presets
static void reset_mode_shifts(settings_t *settings)
{
  for (int m = 0; m <= VIDEO_MODE_STD_MAX; m++)
  {
    settings->mode_shX[m] = video_modes[m]->shX;
    settings->mode_shY[m] = video_modes[m]->shY;
  }
}

void reset_settings_to_defaults(settings_t *settings)
{
  memcpy(settings, &default_settings, sizeof(settings_t));
  reset_mode_shifts(settings);
  settings->shX = video_modes[settings->video_out_mode]->shX;
  settings->shY = video_modes[settings->video_out_mode]->shY;
  settings->crc = calculate_settings_crc(settings);
  settings->mode_crc = calculate_mode_shift_crc(settings);
}

static uint32_t crc32(const uint8_t *data, size_t len)
{
  uint32_t crc = 0xFFFFFFFF;

  for (size_t i = 0; i < len; i++)
  {
    crc ^= data[i];
    for (int bit = 0; bit < 8; bit++)
      crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
  }

  return ~crc;
}

uint32_t calculate_settings_crc(const settings_t *settings)
{
  // CRC covers all fields before the crc field
  return crc32((const uint8_t *)settings, offsetof(settings_t, crc));
}

uint32_t calculate_mode_shift_crc(const settings_t *settings)
{
  return crc32(settings->mode_shX, offsetof(settings_t, mode_crc) - offsetof(settings_t, mode_shX));
}

void switch_mode_shift(uint8_t old_mode)
{
  uint8_t old_std = video_mode_standard(old_mode);
  uint8_t new_std = video_mode_standard(settings.video_out_mode);

  settings.mode_shX[old_std] = settings.shX;
  settings.mode_shY[old_std] = settings.shY;
  settings.shX = settings.mode_shX[new_std];
  settings.shY = settings.mode_shY[new_std];
}

void load_settings(settings_t *settings)
{
  const int *saved_settings = (const int *)(XIP_BASE + (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE));

  memcpy(settings, saved_settings, sizeof(settings_t));

  if (settings->crc != calculate_settings_crc(settings))
  {
    reset_settings_to_defaults(settings);
    return;
  }

  if (settings->mode_crc != calculate_mode_shift_crc(settings))
    reset_mode_shifts(settings); // saved without the table (check_settings keeps the current mode's shift)

  check_settings(settings);
}

// save_settings programs one flash page
_Static_assert(sizeof(settings_t) <= FLASH_PAGE_SIZE, "settings_t must fit in a flash page");

void save_settings(settings_t *settings)
{
  check_settings(settings);

  stop_core1 = true;

  __dmb();

  while (!core1_inactive)
    __dmb();

  uint32_t ints = save_and_disable_interrupts();

  flash_range_erase((PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE), FLASH_SECTOR_SIZE);
  flash_range_program((PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE), (uint8_t *)settings, FLASH_PAGE_SIZE);

  restore_interrupts_from_disabled(ints);

  // Force core 1 to re-enter the capture initialization path after flash save.
  restart_capture = true;

  __dmb();

  stop_core1 = false;
  core1_inactive = false;
}

uint8_t buttons_mode()
{
  if (SLIM_EXT_KBD)
    return BUTTONS_SLIM_EXT_KBD;

  return UPDOWN_SWITCH_MODE ? BUTTONS_MODE_SWITCH : BUTTONS_DEFAULT;
}

void set_buttons_mode(uint8_t mode)
{
  settings.options &= ~(OPTION_UPDOWN_SWITCH_MODE | OPTION_SLIM_EXT_KBD);

  if (mode == BUTTONS_SLIM_EXT_KBD)
    settings.options |= OPTION_UPDOWN_SWITCH_MODE | OPTION_SLIM_EXT_KBD;
  else if (mode == BUTTONS_MODE_SWITCH)
    settings.options |= OPTION_UPDOWN_SWITCH_MODE;
}

const char *buttons_mode_name(uint8_t mode)
{
  static const char *names[BUTTONS_MODE_COUNT] = {"DEFAULT", "MODE SWITCH", "SLIM EXT KBD"};

  return mode < BUTTONS_MODE_COUNT ? names[mode] : "?";
}
