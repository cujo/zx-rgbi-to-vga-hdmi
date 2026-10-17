#pragma once

video_out_type_t detect_video_output_type();
void start_video_output(video_out_type_t);
bool buffering_x3();         // the buffering in effect: X3 at the Pentagon rate, else settings.buffering_mode
void apply_buffering_mode(); // sets it
void stop_video_output();
void set_scanlines_mode();
void draw_welcome_screen(video_mode_t);
void draw_welcome_screen_h(video_mode_t);
void draw_no_signal(video_mode_t);