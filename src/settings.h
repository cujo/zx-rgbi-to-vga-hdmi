#pragma once

uint32_t calculate_settings_crc(const settings_t *);
void reset_settings_to_defaults(settings_t *);
void check_settings(settings_t *);
void load_settings(settings_t *);
void save_settings(settings_t *);
uint32_t calculate_mode_shift_crc(const settings_t *);
// on a video mode change (settings.video_out_mode already set): keep the shift of old_mode, restore the new mode's
// (one entry per resolution and div: the standard and Pentagon rate pair share it)
void switch_mode_shift(uint8_t old_mode);