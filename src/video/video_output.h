#pragma once

// CRT effect settings, in the order of the OSD CRT EFFECTS menu and the serial keys "swgmn"
typedef enum crt_setting_t
{
  CRT_SCANLINES,  // settings.scanlines_mode
  CRT_BEAM_WIDTH, // CRTOPT_WIDTH(settings.crt_options) (DVI, BEAM scanlines)
  CRT_GRILLE,     // GRILLE_LEVEL(settings.grille_mode): mask strength (DVI, scanlines on)
  CRT_MASK,       // GRILLE_MASK(settings.grille_mode): mask type (DVI, scanlines and a mask strength on)
  CRT_NORMAL,     // CRTOPT_NORMAL(settings.crt_options) (DVI, scanlines on)
  CRT_SETTING_COUNT
} crt_setting_t;

video_out_type_t detect_video_output_type();
void start_video_output(video_out_type_t);
bool buffering_x3();         // the buffering in effect: X3 at the Pentagon rate, else settings.buffering_mode
void apply_buffering_mode(); // sets it
void stop_video_output();
void set_scanlines_mode();
// whether a CRT effect setting applies with the current output and settings (inactive ones do not change)
bool crt_setting_active(crt_setting_t);
// cycles a CRT effect setting to its next value and applies it to the running output
void next_crt_setting(crt_setting_t);
// the scanline level the output shows (VGA: OFF, 25, 50, 75 or 100%; ADAPT and BEAM show as 25%)
uint8_t crt_scanlines_level();
void draw_welcome_screen(video_mode_t);
void draw_welcome_screen_h(video_mode_t);
void draw_no_signal(video_mode_t);
