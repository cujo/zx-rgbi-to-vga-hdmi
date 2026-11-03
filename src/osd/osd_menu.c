#include "hardware/timer.h"

#include "g_config.h"
#include "osd_menu.h"
#include "font.h"
#include "osd.h"
#include "rgb_capture.h"
#include "settings.h"
#include "video_output.h"

#ifdef OSD_FF_ENABLE
#include "ff_osd.h"
#endif

#ifdef KBD_ENABLE
#include "osd_kbd.h"
#endif

#ifdef HW_CONFIG_ENABLE
#include "hw_config.h"
#endif

// Pin inversion mask bit positions for menu items
static const uint8_t mask_bit_positions[] = {
    CAP_F,
    CAP_HS,
    CAP_VS,
    CAP_I,
    CAP_R,
    CAP_G,
    CAP_B,
};

// Main menu item indices (FF OSD and HARDWARE entries are conditionally included)
#define MAIN_ITEM_OUTPUT 0
#define MAIN_ITEM_CAPTURE 1
#define MAIN_ITEM_IMAGE_ADJ 2
#define MAIN_ITEM_CRT 3

// Compute subsequent indices based on which optional items are enabled
#ifdef OSD_FF_ENABLE
#define MAIN_ITEM_FF_OSD 4
#define _NEXT_AFTER_FF 5
#else
#define _NEXT_AFTER_FF 4
#endif

#ifdef HW_CONFIG_ENABLE
#define MAIN_ITEM_HARDWARE _NEXT_AFTER_FF
#define _NEXT_AFTER_HW (_NEXT_AFTER_FF + 1)
#else
#define _NEXT_AFTER_HW _NEXT_AFTER_FF
#endif

#define MAIN_ITEM_ABOUT _NEXT_AFTER_HW
#define MAIN_ITEM_SAVE (_NEXT_AFTER_HW + 1)
#define MAIN_ITEM_EXIT (_NEXT_AFTER_HW + 2)
#define MAIN_ITEM_COUNT (_NEXT_AFTER_HW + 3)

extern settings_t settings;
extern video_out_type_t active_video_output;
extern volatile bool restart_capture;

osd_menu_state_t osd_menu_state = {
    .selected_item = 0,
    .tuning_mode = false};

osd_menu_nav_t osd_menu = {0};

static void osd_menu_hide(void)
{
    // Hide menu and block button input until all buttons are released.
    osd_hide();
    osd_state.menu_active = false;
    osd_block_buttons_until_release();
#ifdef OSD_FF_ENABLE
    if (settings.ff_osd_config.enabled && settings.ff_osd_config.i2c_protocol)
        ff_osd_set_buttons(0);
#endif
}

// Navigate back to the parent menu, restoring the previous selection.
// Returns true if navigation occurred (sets menu_changed in callers).
static bool osd_menu_go_back(void)
{
    if (osd_menu.menu_depth == 0)
        return false;

    osd_menu.menu_depth--;
    osd_menu.current_menu = osd_menu.menu_stack[osd_menu.menu_depth];
    osd_menu_state.selected_item = osd_menu.item_stack[osd_menu.menu_depth];
    osd_menu_state.tuning_mode = false;
    osd_state.needs_redraw = true;
    return true;
}

// Enter a child submenu, saving current position on the stack.
// Returns true (always sets menu_changed in callers).
static bool osd_menu_enter_submenu(uint8_t menu_type)
{
    if (osd_menu.menu_depth >= 4)
        return false;

    osd_menu.menu_stack[osd_menu.menu_depth] = osd_menu.current_menu;
    osd_menu.item_stack[osd_menu.menu_depth] = osd_menu_state.selected_item;
    osd_menu.menu_depth++;
    osd_menu.current_menu = menu_type;
    osd_menu_state.selected_item = 0;
    osd_menu_state.tuning_mode = false;
    osd_state.needs_redraw = true;
    return true;
}

// Flush pending render operations.
static void osd_flush_render(void)
{
    if (osd_state.needs_redraw)
    {
        osd_update_text_buffer();
        osd_state.needs_redraw = false;
        osd_state.text_updated = true;
    }

    if (osd_state.text_updated)
    {
        osd_render_text_to_buffer();
        osd_state.text_updated = false;
    }
}

void osd_menu_init()
{
    memset(&osd_menu_state, 0, sizeof(osd_menu_state));
    memset(&osd_menu, 0, sizeof(osd_menu));
    osd_menu.current_menu = MENU_TYPE_MAIN;
}

// settings.video_out_mode changed from old_mode: restart the output in it (when this output type is running)
static void osd_apply_video_mode(uint8_t old_mode)
{
    if (active_video_output != settings.video_out_type || old_mode == settings.video_out_mode)
        return;

    stop_video_output();
    // keep the old mode's shift, restore the new mode's (its preset until adjusted)
    switch_mode_shift(old_mode);
    start_video_output(active_video_output);
    // Adjust capture frequency for new system clock
    set_capture_frequency(settings.frequency);
}

// the same resolution at the other frame rate (standard / Pentagon); applied on SEL (tuning mode)
static void osd_toggle_video_rate()
{
    int index = video_mode_index(settings.video_out_type, settings.video_out_mode);
    settings.video_out_mode = video_mode_select(settings.video_out_type, index, !video_mode_pentagon(settings.video_out_mode));
}

// the mode name shown for a while after a mode change with the buttons outside the menu (0 = not shown); long
// enough to outlast the monitor's resync, and off center, clear of the monitor's own mode message
#define MODE_TOAST_US 5000000u
static uint64_t mode_toast_until = 0;

bool osd_menu_toast_active()
{
    return mode_toast_until != 0;
}

static void osd_show_mode_toast()
{
    const char *name = video_mode_name(settings.video_out_mode);

    osd_font = osd_font_style_1;
    osd_mode.x = 6; // near the left
    osd_mode.y = 3; // near the top
    osd_mode.columns = strlen(name) + 4;
    osd_mode.rows = 3;
    osd_mode.border_enabled = true;
    osd_mode.full_width = false;
    osd_mode.width = osd_mode.columns * OSD_FONT_WIDTH;
    osd_mode.height = osd_mode.rows * OSD_FONT_HEIGHT;
    osd_mode.buffer_size = osd_mode.width * osd_mode.height / 2;
    osd_set_position();
    osd_clear_text_buffer();
    osd_show();
    osd_text_print_centered(1, name, OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND, 0);
    osd_render_text_to_buffer();
    mode_toast_until = time_us_64() + MODE_TOAST_US;
}

// FF OSD (I2C) shares the buttons with the Gotek: short taps go to the Gotek, long holds are ours
static bool ff_osd_owns_buttons()
{
#ifdef OSD_FF_ENABLE
    return settings.ff_osd_config.enabled && settings.ff_osd_config.i2c_protocol;
#else
    return false;
#endif
}

// outside the menu with UP / DOWN SWITCH MODE on, they step through the resolutions of the output at the
// current rate (a short press, a 1 s hold when the FF OSD shares the buttons); returns true when it did
static bool osd_mode_hotkeys(uint64_t current_time)
{
    int8_t direction = 0;

    if (!UPDOWN_SWITCH_MODE)
        return false;

#if defined(OSD_FF_ENABLE) && defined(KBD_ENABLE)
    if (ff_osd_kbd_active)
        return false; // the keyboard arrows control the Gotek (F10)
#endif

    if (ff_osd_owns_buttons())
    {
        if (osd_button_hold_duration_us(0, current_time) > OSD_HOLD_US)
            direction = 1;
        else if (osd_button_hold_duration_us(1, current_time) > OSD_HOLD_US)
            direction = -1;
    }
    else if (osd_button_pressed(0))
        direction = 1;
    else if (osd_button_pressed(1))
        direction = -1;

    if (!direction)
        return false;

    // one step per press or hold; the release is not passed to the Gotek
    osd_block_buttons_until_release();
#ifdef OSD_FF_ENABLE
    if (ff_osd_owns_buttons())
        ff_osd_set_buttons(0);
#endif

    uint8_t old_mode = settings.video_out_mode;
    osd_adjust_video_mode(direction);
    osd_apply_video_mode(old_mode);
    osd_show_mode_toast();
    return true;
}

void osd_menu_update()
{
    if (!osd_state.enabled)
        return;

    // If menu is not active, only check for activation buttons
    if (!osd_state.menu_active)
    {
#ifdef KBD_ENABLE
        // Check cross-core menu open request (from keyboard)
        bool kbd_open = osd_menu_request;
        if (kbd_open)
            osd_menu_request = false;
#endif
        osd_buttons_update();

        if (osd_buttons_apply_release_block())
        {
#ifdef OSD_FF_ENABLE
            if (settings.ff_osd_config.enabled && settings.ff_osd_config.i2c_protocol)
                ff_osd_set_buttons(0);
#endif
            return;
        }

        uint64_t current_time = time_us_64();

        if (mode_toast_until && current_time > mode_toast_until)
        {
            mode_toast_until = 0;
            osd_hide();
        }

        if (osd_mode_hotkeys(current_time))
            return;

        bool opened_by_long_hold = osd_button_hold_duration_us(2, current_time) > OSD_HOLD_US;

        // any button opens the menu; SEL only when UP / DOWN switch the video mode (osd_mode_hotkeys)
        bool open_menu = osd_button_pressed(2);

        if (!UPDOWN_SWITCH_MODE)
            open_menu |= osd_button_pressed(0) || osd_button_pressed(1);

        if (ff_osd_owns_buttons())
            open_menu = opened_by_long_hold;
#ifdef KBD_ENABLE
        open_menu |= kbd_open;
#endif

        if (open_menu)
        {
            mode_toast_until = 0;
            osd_state.menu_active = true;
            osd_font = osd_font_style_1;
            osd_mode.x = 0;
            osd_mode.y = 0;
            osd_mode.columns = 30;
            osd_mode.rows = 15;
            osd_mode.border_enabled = true;
            osd_mode.full_width = false;
            osd_mode.width = osd_mode.columns * OSD_FONT_WIDTH;
            osd_mode.height = osd_mode.rows * OSD_FONT_HEIGHT;
            osd_mode.buffer_size = osd_mode.width * osd_mode.height / 2;

            osd_set_position();

            osd_show();
            // Always require a full release after opening to avoid immediate
            // repeat navigation or accidental activation in the first frame.
            osd_block_buttons_until_release();
#ifdef OSD_FF_ENABLE
            if (settings.ff_osd_config.enabled && settings.ff_osd_config.i2c_protocol)
                ff_osd_set_buttons(0);
#endif
            // Don't process the button press that opened the menu.
            // Also clear any virtual button events (e.g. BACK/ESC) that were
            // queued before the menu opened — otherwise a stale BACK would
            // fire after the release block and immediately close the menu.
            osd_clear_pressed_buttons();
#ifdef KBD_ENABLE
            osd_virtual_buttons = 0;
#endif
        }
        return;
    }

#ifdef KBD_ENABLE
    // F9 (OSD_HOTKEY_MENU) toggle: close menu if already active
    if (osd_menu_request)
    {
        osd_menu_request = false;
        osd_menu_hide();
        osd_menu_init();
        return;
    }
#endif

    // Menu is active from here
    // Update button states
    osd_buttons_update();

    if (osd_buttons_apply_release_block())
    {
#ifdef OSD_FF_ENABLE
        if (settings.ff_osd_config.enabled && settings.ff_osd_config.i2c_protocol)
            ff_osd_set_buttons(0);
#endif
        // Keep rendering while input is blocked so menu appears immediately.
        osd_flush_render();
        return;
    }

    // Check for menu timeout
    {
        uint64_t current_time = time_us_64();

        if ((current_time - osd_state.last_activity_time) > OSD_MENU_TIMEOUT_US)
        {
            osd_menu_hide();
            return;
        }
    }

    // Get menu item count based on current menu
    {
        uint8_t max_items;
        if (osd_menu.current_menu == MENU_TYPE_MAIN)
            max_items = MAIN_ITEM_COUNT - 1;
        else if (osd_menu.current_menu == MENU_TYPE_OUTPUT)
            max_items = 4; // Output menu: 0-4 (5 items: mode, rate, up/down switch mode, buffering, back)
        else if (osd_menu.current_menu == MENU_TYPE_CRT)
            max_items = CRT_SETTING_COUNT; // CRT effects menu: the settings (crt_setting_t order), back
        else if (osd_menu.current_menu == MENU_TYPE_CAPTURE)
            max_items = 5; // Capture menu: 0-5 (6 items: freq, mode, divider, sync, mask, back) - divider always shown but dimmed for SELF
        else if (osd_menu.current_menu == MENU_TYPE_IMAGE_ADJUST)
            max_items = 4; // Image adjust menu: 0-4 (5 items: H-POS, V-POS, DELAY, RESET, BACK)
        else if (osd_menu.current_menu == MENU_TYPE_MASK)
            max_items = 7; // Mask menu: 0-7 (8 items: F, SSI, KSI, I, B, G, R, BACK)
        else if (osd_menu.current_menu == MENU_TYPE_ABOUT)
            max_items = 0; // About menu: 0 (1 item: BACK)
#ifdef MAIN_ITEM_FF_OSD
        else if (osd_menu.current_menu == MENU_TYPE_FF_OSD)
            max_items = 6; // FF OSD menu: 0-6 (7 items: ENABLE, PROTOCOL, ROWS, COLUMNS, H_POS, V_POS, BACK)
#endif
#ifdef MAIN_ITEM_HARDWARE
        else if (osd_menu.current_menu == MENU_TYPE_HARDWARE)
            max_items = 3; // Hardware menu: 0-3 (4 items: ROM BANK, RAM SIZE, GOTEK DRIVE, BACK)
#endif
        else
            max_items = 0;

        // Handle navigation buttons
        if (osd_button_pressed(0))
        {                          // UP button
            osd_update_activity(); // Reset timeout on user interaction
            if (osd_menu.current_menu == MENU_TYPE_OUTPUT && osd_menu_state.tuning_mode && osd_menu_state.selected_item < 2)
            { // Video mode / rate adjustment
                if (osd_menu_state.selected_item == 0)
                    osd_adjust_video_mode(1);
                else
                    osd_toggle_video_rate();
                osd_state.needs_redraw = true;
            }
            else if (osd_menu.current_menu == MENU_TYPE_IMAGE_ADJUST && osd_menu_state.tuning_mode && osd_menu_state.selected_item < 3)
            { // Parameter adjustment mode - increase parameter value
                osd_adjust_image_parameter(osd_menu_state.selected_item, 1);
                osd_state.needs_redraw = true;
            }
            else if (osd_menu.current_menu == MENU_TYPE_CAPTURE && osd_menu_state.tuning_mode && osd_menu_state.selected_item != 1 && osd_menu_state.selected_item != 3)
            { // Capture parameter adjustment (exclude item 1 - MODE, item 3 - SYNC)
                osd_adjust_capture_parameter(osd_menu_state.selected_item, 1);
                osd_state.needs_redraw = true;
            }
#ifdef MAIN_ITEM_FF_OSD
            else if (osd_menu.current_menu == MENU_TYPE_FF_OSD && osd_menu_state.tuning_mode && (osd_menu_state.selected_item == 3 || osd_menu_state.selected_item == 4))
            { // FF OSD columns or H_POS cycling up
                osd_adjust_ff_osd_parameter(osd_menu_state.selected_item, 1);
                osd_state.needs_redraw = true;
            }
#endif
#ifdef MAIN_ITEM_HARDWARE
            else if (osd_menu.current_menu == MENU_TYPE_HARDWARE && osd_menu_state.tuning_mode)
            {
                osd_adjust_hardware_parameter(osd_menu_state.selected_item, 1);
                osd_state.needs_redraw = true;
            }
#endif
            else
            { // Menu navigation mode - move selection up
                if (osd_menu_state.selected_item > 0)
                {
                    osd_menu_state.selected_item--;
                    osd_state.needs_redraw = true;
                }
            }

            osd_buttons.up_pressed = false;
        }
        if (osd_button_pressed(1))
        {                          // DOWN button
            osd_update_activity(); // Reset timeout on user interaction
            if (osd_menu.current_menu == MENU_TYPE_OUTPUT && osd_menu_state.tuning_mode && osd_menu_state.selected_item < 2)
            { // Video mode / rate adjustment
                if (osd_menu_state.selected_item == 0)
                    osd_adjust_video_mode(-1);
                else
                    osd_toggle_video_rate();
                osd_state.needs_redraw = true;
            }
            else if (osd_menu.current_menu == MENU_TYPE_IMAGE_ADJUST && osd_menu_state.tuning_mode && osd_menu_state.selected_item < 3)
            { // Parameter adjustment mode - decrease parameter value
                osd_adjust_image_parameter(osd_menu_state.selected_item, -1);
                osd_state.needs_redraw = true;
            }
            else if (osd_menu.current_menu == MENU_TYPE_CAPTURE && osd_menu_state.tuning_mode && osd_menu_state.selected_item != 1 && osd_menu_state.selected_item != 3)
            { // Capture parameter adjustment (exclude item 1 - MODE, item 3 - SYNC)
                osd_adjust_capture_parameter(osd_menu_state.selected_item, -1);
                osd_state.needs_redraw = true;
            }
#ifdef MAIN_ITEM_FF_OSD
            else if (osd_menu.current_menu == MENU_TYPE_FF_OSD && osd_menu_state.tuning_mode && (osd_menu_state.selected_item == 3 || osd_menu_state.selected_item == 4))
            { // FF OSD columns or H_POS cycling down
                osd_adjust_ff_osd_parameter(osd_menu_state.selected_item, -1);
                osd_state.needs_redraw = true;
            }
#endif
#ifdef MAIN_ITEM_HARDWARE
            else if (osd_menu.current_menu == MENU_TYPE_HARDWARE && osd_menu_state.tuning_mode)
            {
                osd_adjust_hardware_parameter(osd_menu_state.selected_item, -1);
                osd_state.needs_redraw = true;
            }
#endif
            else
            { // Menu navigation mode - move selection down
                if (osd_menu_state.selected_item < max_items)
                {
                    osd_menu_state.selected_item++;
                    osd_state.needs_redraw = true;
                }
            }

            osd_buttons.down_pressed = false;
        }

        // Handle selection in different menus
        if (osd_button_pressed(2))
        {                          // SEL button for item selection
            osd_update_activity(); // Reset timeout on user interaction
            bool menu_changed = false;

            if (osd_menu.current_menu == MENU_TYPE_MAIN)
            {
                // Main menu selection
                if (osd_menu_state.selected_item == MAIN_ITEM_OUTPUT)
                { // Output Settings
                    menu_changed = osd_menu_enter_submenu(MENU_TYPE_OUTPUT);
                }
                else if (osd_menu_state.selected_item == MAIN_ITEM_CAPTURE)
                { // Capture Settings
                    menu_changed = osd_menu_enter_submenu(MENU_TYPE_CAPTURE);
                }
                else if (osd_menu_state.selected_item == MAIN_ITEM_IMAGE_ADJ)
                { // Image Adjust
                    menu_changed = osd_menu_enter_submenu(MENU_TYPE_IMAGE_ADJUST);
                }
                else if (osd_menu_state.selected_item == MAIN_ITEM_CRT)
                { // CRT effects
                    menu_changed = osd_menu_enter_submenu(MENU_TYPE_CRT);
                }
#ifdef MAIN_ITEM_FF_OSD
                else if (osd_menu_state.selected_item == MAIN_ITEM_FF_OSD)
                { // FF OSD Config
                    menu_changed = osd_menu_enter_submenu(MENU_TYPE_FF_OSD);
                }
#endif
#ifdef MAIN_ITEM_HARDWARE
                else if (osd_menu_state.selected_item == MAIN_ITEM_HARDWARE)
                { // Hardware Config
                    menu_changed = osd_menu_enter_submenu(MENU_TYPE_HARDWARE);
                }
#endif
                else if (osd_menu_state.selected_item == MAIN_ITEM_ABOUT)
                { // About
                    menu_changed = osd_menu_enter_submenu(MENU_TYPE_ABOUT);
                }
                else if (osd_menu_state.selected_item == MAIN_ITEM_SAVE)
                { // Save
                    stop_video_output();
                    save_settings(&settings);
                    osd_menu_hide();
                    menu_changed = true;
                    start_video_output(active_video_output);
                }
                else if (osd_menu_state.selected_item == MAIN_ITEM_EXIT)
                { // Exit without saving
                    osd_menu_hide();
                    menu_changed = true;
                }
            }
            else if (osd_menu.current_menu == MENU_TYPE_OUTPUT)
            {                                // Output submenu selection
                uint8_t back_item_index = 4; // 5 items (mode, rate, up/down switch mode, buffering, back)

                if (osd_menu_state.selected_item == back_item_index)
                { // Back to Main
                    menu_changed = osd_menu_go_back();
                }
                else if (osd_menu_state.selected_item < 2)
                { // Video mode / rate selection - enter/exit tuning mode
                    if (osd_menu_state.tuning_mode)
                    { // Exit tuning mode and apply video mode change (restarts only if the mode has changed)
                        osd_apply_video_mode(osd_menu_state.original_video_mode);
                        osd_menu_state.tuning_mode = false;
                    }
                    else
                    { // Enter tuning mode - store original mode
                        osd_menu_state.original_video_mode = settings.video_out_mode;
                        osd_menu_state.tuning_mode = true;
                    }
                    osd_state.needs_redraw = true;
                }
                else if (osd_menu_state.selected_item == 2)
                { // UP / DOWN SWITCH MODE: on / off
                    settings.options ^= OPTION_UPDOWN_SWITCH_MODE;
                    osd_state.needs_redraw = true;
                }
                else if (osd_menu_state.selected_item == 3)
                { // Buffering - toggle between X1 and X3 (at the Pentagon rate always X3)
                    if (!video_mode_pentagon(settings.video_out_mode))
                    {
                        settings.buffering_mode = !settings.buffering_mode;
                        apply_buffering_mode();
                        osd_state.needs_redraw = true;
                    }
                }
            }
            else if (osd_menu.current_menu == MENU_TYPE_CRT)
            { // CRT effects submenu selection: a setting (next value) or back
                if (osd_menu_state.selected_item == CRT_SETTING_COUNT)
                    menu_changed = osd_menu_go_back();
                else
                {
                    next_crt_setting(osd_menu_state.selected_item);
                    osd_state.needs_redraw = true;
                }
            }
            else if (osd_menu.current_menu == MENU_TYPE_CAPTURE)
            {                                // Capture submenu selection
                uint8_t back_item_index = 5; // Always 6 items (freq, mode, divider, sync, mask, back)

                if (osd_menu_state.selected_item == back_item_index)
                { // Back to Main
                    menu_changed = osd_menu_go_back();
                }
                else if (osd_menu_state.selected_item == 1)
                { // Capture mode - toggle between SELF and EXT
                    settings.cap_sync_mode = (settings.cap_sync_mode == SELF) ? EXT : SELF;
                    restart_capture = true;
                    osd_state.needs_redraw = true;
                }
                else if (osd_menu_state.selected_item == 2)
                { // Divider
                    // Only allow tuning mode if EXT mode
                    if (settings.cap_sync_mode == EXT)
                    {
                        osd_menu_state.tuning_mode = !osd_menu_state.tuning_mode;
                        osd_state.needs_redraw = true;
                    }
                }
                else if (osd_menu_state.selected_item == 3)
                { // Sync - toggle between COMPOSITE and SEPARATE
                    settings.video_sync_mode = !settings.video_sync_mode;
                    set_video_sync_mode(settings.video_sync_mode);
                    osd_state.needs_redraw = true;
                }
                else if (osd_menu_state.selected_item == 4)
                { // Mask - open mask submenu
                    menu_changed = osd_menu_enter_submenu(MENU_TYPE_MASK);
                }
                else
                { // Other adjustable parameters (0)
                    // Toggle tuning mode for adjustable parameters
                    osd_menu_state.tuning_mode = !osd_menu_state.tuning_mode;
                    osd_state.needs_redraw = true;
                }
            }
            else if (osd_menu.current_menu == MENU_TYPE_IMAGE_ADJUST)
            {                                // Image adjust submenu selection
                uint8_t back_item_index = 4; // 5 items: H-POS, V-POS, DELAY, RESET, BACK

                if (osd_menu_state.selected_item == back_item_index)
                { // Back to Main
                    menu_changed = osd_menu_go_back();
                }
                else if (osd_menu_state.selected_item == 3)
                { // Reset to defaults (the shift: the current mode's preset)
                    set_capture_shX(video_modes[settings.video_out_mode]->shX);
                    set_capture_shY(video_modes[settings.video_out_mode]->shY);
                    set_capture_delay(DELAY_DEF);
                    osd_state.needs_redraw = true;
                    osd_menu_hide(); // Hide menu after resetting to defaults
                    menu_changed = true;
                }
                else if (osd_menu_state.selected_item < 3)
                { // Adjustable parameters (0-2)
                    // Toggle tuning mode for adjustable parameters
                    osd_menu_state.tuning_mode = !osd_menu_state.tuning_mode;
                    osd_state.needs_redraw = true;
                }
            }
            else if (osd_menu.current_menu == MENU_TYPE_MASK)
            {                                // Mask submenu selection
                uint8_t back_item_index = 7; // 8 items: F, SSI, KSI, I, B, G, R, BACK

                if (osd_menu_state.selected_item == back_item_index)
                { // Back to Capture
                    menu_changed = osd_menu_go_back();
                }
                else
                { // Toggle the selected mask bit
                    uint8_t bit_pos = mask_bit_positions[osd_menu_state.selected_item];
                    uint8_t bit_mask = 1 << bit_pos;
                    settings.pin_inversion_mask ^= bit_mask;
                    set_pin_inversion_mask(settings.pin_inversion_mask);
                    osd_state.needs_redraw = true;
                }
            }
#ifdef MAIN_ITEM_FF_OSD
            else if (osd_menu.current_menu == MENU_TYPE_FF_OSD)
            {                                // FF OSD submenu selection
                uint8_t back_item_index = 6; // 7 items: ENABLE, PROTOCOL, ROWS, COLUMNS, H_POS, V_POS, BACK

                if (osd_menu_state.selected_item == back_item_index)
                { // Back to Main
                    menu_changed = osd_menu_go_back();
                }
                else if (osd_menu_state.selected_item == 0)
                { // Enable - toggle
                    osd_adjust_ff_osd_parameter(0, 0);
                    osd_state.needs_redraw = true;
                }
                else if (osd_menu_state.selected_item == 1)
                { // Protocol - toggle
                    osd_adjust_ff_osd_parameter(1, 0);
                    osd_state.needs_redraw = true;
                }
                else if (osd_menu_state.selected_item == 2)
                { // Rows - toggle 2/4 (only when LCD_HD44780)
                    if (!settings.ff_osd_config.i2c_protocol)
                    {
                        osd_adjust_ff_osd_parameter(2, 0);
                        osd_state.needs_redraw = true;
                    }
                }
                else if (osd_menu_state.selected_item == 3)
                { // Columns - enter/exit tuning mode (only when LCD_HD44780)
                    if (!settings.ff_osd_config.i2c_protocol)
                    {
                        osd_menu_state.tuning_mode = !osd_menu_state.tuning_mode;
                        osd_state.needs_redraw = true;
                    }
                }
                else if (osd_menu_state.selected_item == 4)
                { // H_POS - enter/exit tuning mode
                    osd_menu_state.tuning_mode = !osd_menu_state.tuning_mode;
                    osd_state.needs_redraw = true;
                }
                else if (osd_menu_state.selected_item == 5)
                { // V_POS - toggle top/bottom
                    osd_adjust_ff_osd_parameter(5, 0);
                    osd_state.needs_redraw = true;
                }
            }
#endif
#ifdef MAIN_ITEM_HARDWARE
            else if (osd_menu.current_menu == MENU_TYPE_HARDWARE)
            {                                // Hardware submenu selection
                uint8_t back_item_index = 3; // 4 items: ROM BANK, RAM SIZE, GOTEK DRIVE, BACK

                if (osd_menu_state.selected_item == back_item_index)
                { // Back to Main
                    menu_changed = osd_menu_go_back();
                }
                else if (osd_menu_state.selected_item == 1)
                { // RAM SIZE - toggle directly
                    settings.hw_config.ram_size = !settings.hw_config.ram_size;
                    hw_set_ram_size(settings.hw_config.ram_size);
                    osd_state.needs_redraw = true;
                }
                else
                { // ROM BANK / GOTEK DRIVE - enter/exit tuning mode
                    osd_menu_state.tuning_mode = !osd_menu_state.tuning_mode;
                    osd_state.needs_redraw = true;
                }
            }
#endif
            else if (osd_menu.current_menu == MENU_TYPE_ABOUT)
            { // About submenu - only BACK button
                if (osd_menu_state.selected_item == 0)
                { // Back to Main
                    menu_changed = osd_menu_go_back();
                }
            }
            else
            { // If we're in an unknown menu or want to close, toggle off
                osd_menu_toggle();
                menu_changed = true;
            }

            if (menu_changed)
            {
                osd_block_buttons_until_release();
#ifdef KBD_ENABLE
                // Discard any BACK/SEL events that arrived during the action
                // so they don't fire unexpectedly after the block clears.
                osd_virtual_buttons = 0;
#endif
            }

            osd_buttons.sel_pressed = false;
        }
#ifdef KBD_ENABLE
        // Handle BACK (ESC key via virtual buttons)
        uint8_t virt = osd_virtual_buttons;

        if (virt & OSD_VIRT_BACK)
        {
            osd_virtual_buttons &= ~OSD_VIRT_BACK;
            osd_update_activity();

            if (osd_menu_state.tuning_mode)
            {
                osd_menu_state.tuning_mode = false;
                osd_state.needs_redraw = true;
            }
            else if (osd_menu.menu_depth > 0)
            {
                osd_menu_go_back();
                osd_block_buttons_until_release();
            }
            else
                osd_menu_hide();
        }
#endif

#ifdef MAIN_ITEM_FF_OSD
        // Re-render when Gotek reports a new column count asynchronously (Core 1 update)
        if (osd_menu.current_menu == MENU_TYPE_FF_OSD && settings.ff_osd_config.i2c_protocol)
        {
            static uint8_t last_ff_cols = 0;

            if (ff_osd_display.cols != last_ff_cols)
            {
                last_ff_cols = ff_osd_display.cols;
                osd_state.needs_redraw = true;
            }
        }
#endif

        osd_flush_render();
    }
}

void osd_menu_toggle()
{
    if (osd_state.menu_active)
        osd_menu_hide();
    else
    {
        osd_show();
        osd_state.menu_active = true;
    }
}

// Compute fg/bg colors for a menu row.
// selected: whether this row is the current selection
// tuning:   whether this row is in tuning (adjustment) mode
// color:    the base text color (OSD_COLOR_TEXT or OSD_COLOR_DIMMED)
static inline void menu_item_colors(bool selected, bool tuning, uint8_t color, uint8_t *fg, uint8_t *bg)
{
    if (selected && tuning)
    {
        *fg = OSD_COLOR_BACKGROUND;
        *bg = OSD_COLOR_SELECTED;
    }
    else if (selected)
    {
        *fg = OSD_COLOR_BACKGROUND;
        *bg = color;
    }
    else
    {
        *fg = color;
        *bg = OSD_COLOR_BACKGROUND;
    }
}

// Menu rendering functions
static void render_main_menu()
{
    const char *items[] = {
        "OUTPUT SETTINGS",
        "CAPTURE SETTINGS",
        "IMAGE ADJUST",
        "CRT EFFECTS",
#ifdef MAIN_ITEM_FF_OSD
        "FF OSD CONFIG",
#endif
#ifdef MAIN_ITEM_HARDWARE
        "HARDWARE CONFIG",
#endif
        "ABOUT",
        "SAVE",
        "EXIT"};

    for (int i = 0; i < MAIN_ITEM_COUNT; i++)
    {
        uint8_t row = OSD_MENU_START_ROW + i;
        uint8_t fg_color, bg_color;
        menu_item_colors(i == osd_menu_state.selected_item, false, OSD_COLOR_TEXT, &fg_color, &bg_color);

        if (i < MAIN_ITEM_ABOUT)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-16s >", items[i]);
        else
            osd_text_print(row, 2, items[i], fg_color, bg_color, 0);
    }
}

// CRT effects submenu items: the settings (crt_setting_t order)
static const char *crt_setting_names[CRT_SETTING_COUNT] = {"SCANLINES", "BEAM", "GRILLE", "MASK", "NORMAL"};

static const char *crt_setting_value(crt_setting_t setting)
{
    static const char *scanlines[] = {"OFF", "ADAPT", "25%", "50%", "75%", "100%", "BEAM"};
    static const char *widths[] = {[BEAM_WIDTH_TV] = "TV", [BEAM_WIDTH_SOFT] = "SOFT", [BEAM_WIDTH_PVM] = "PVM", [BEAM_WIDTH_SHARP] = "SHARP"};
    static const char *grille[] = {[GRILLE_OFF] = "OFF", [GRILLE_5] = "5%", [GRILLE_10] = "10%", [GRILLE_15] = "15%", [GRILLE_30] = "30%", [GRILLE_45] = "45%"};
    static const char *masks[] = {[GRILLE_MASK_MONO] = "MONO", [GRILLE_MASK_MG] = "MG", [GRILLE_MASK_SLOT] = "MONO SLOT", [GRILLE_MASK_MG_SLOT] = "MG SLOT"};
    static const char *normal[] = {[NORMAL_LEVEL_170] = "67%", [NORMAL_LEVEL_192] = "75%", [NORMAL_LEVEL_205] = "80%", [NORMAL_LEVEL_216] = "85%"};

    switch (setting)
    {
    case CRT_SCANLINES:
        return scanlines[crt_scanlines_level()];
    case CRT_BEAM_WIDTH:
        return widths[CRTOPT_WIDTH(settings.crt_options)];
    case CRT_GRILLE:
        return grille[GRILLE_LEVEL(settings.grille_mode)];
    case CRT_MASK:
        return masks[GRILLE_MASK(settings.grille_mode)];
    case CRT_NORMAL:
        return normal[CRTOPT_NORMAL(settings.crt_options)];
    default:
        return "";
    }
}

static void render_output_menu()
{
    osd_text_print_centered(OSD_SUBTITLE_ROW, "OUTPUT SETTINGS", OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND, 0);

    for (int i = 0; i < 5; i++)
    {
        uint8_t row = OSD_MENU_START_ROW + i;
        uint8_t color = OSD_COLOR_TEXT;
        uint8_t fg_color, bg_color;

        if (i == 3 && video_mode_pentagon(settings.video_out_mode))
            color = OSD_COLOR_DIMMED; // always X3 at the Pentagon rate

        menu_item_colors(i == osd_menu_state.selected_item, i < 2 && osd_menu_state.tuning_mode, color, &fg_color, &bg_color);

        if (i == 0)
        {
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %s", "MODE", video_mode_name(settings.video_out_mode));
        }
        else if (i == 1)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %s", "RATE", video_mode_pentagon(settings.video_out_mode) ? "PENTAGON" : "STANDARD");
        else if (i == 2)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %s", "UP/DOWN", UPDOWN_SWITCH_MODE ? "SWITCH MODE" : "DEFAULT");
        else if (i == 3)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %s", "BUFFERING", buffering_x3() ? "X3" : "X1");
        else if (i == 4)
            osd_text_print(row, 2, "< BACK TO MAIN", fg_color, bg_color, 0);

        if (i < 2 && i == osd_menu_state.selected_item && osd_menu_state.tuning_mode)
            osd_text_set_char(row, 1, '>', fg_color, bg_color);
    }
}

// CRT effects submenu: the settings (crt_setting_t order), dimmed where they change nothing, then back
static void render_crt_menu()
{
    osd_text_print_centered(OSD_SUBTITLE_ROW, "CRT EFFECTS", OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND, 0);

    for (int i = 0; i <= CRT_SETTING_COUNT; i++)
    {
        uint8_t row = OSD_MENU_START_ROW + i;
        uint8_t fg_color, bg_color;
        uint8_t color = i < CRT_SETTING_COUNT && !crt_setting_active(i) ? OSD_COLOR_DIMMED : OSD_COLOR_TEXT;

        menu_item_colors(i == osd_menu_state.selected_item, false, color, &fg_color, &bg_color);

        if (i < CRT_SETTING_COUNT)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %s", crt_setting_names[i], crt_setting_value(i));
        else
            osd_text_print(row, 2, "< BACK TO MAIN", fg_color, bg_color, 0);
    }
}

static void render_capture_menu()
{
    osd_text_print_centered(OSD_SUBTITLE_ROW, "CAPTURE SETTINGS", OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND, 0);

    for (int i = 0; i < 6; i++)
    {
        uint8_t row = OSD_MENU_START_ROW + i;
        uint8_t color = OSD_COLOR_TEXT;
        uint8_t fg_color, bg_color;

        if (i == 2 && settings.cap_sync_mode != EXT)
            color = OSD_COLOR_DIMMED;

        menu_item_colors(i == osd_menu_state.selected_item, i < 5 && i != 1 && i != 3 && osd_menu_state.tuning_mode, color, &fg_color, &bg_color);

        if (i == 0)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %lu", "FREQ", settings.frequency);
        else if (i == 1)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %s", "MODE", settings.cap_sync_mode == SELF ? "SELF-SYNC" : "EXTERNAL");
        else if (i == 2)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %d", "DIVIDER", settings.ext_clk_divider);
        else if (i == 3)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %s", "SYNC", settings.video_sync_mode ? "SEPARATE" : "COMPOSITE");
        else if (i == 4)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %s", "MASK", ">");
        else if (i == 5)
            osd_text_print(row, 2, "< BACK TO MAIN", fg_color, bg_color, 0);

        if (i < 5 && i != 1 && i != 3 && i == osd_menu_state.selected_item && osd_menu_state.tuning_mode)
            osd_text_set_char(row, 1, '>', fg_color, bg_color);
    }
}

static void render_image_adjust_menu()
{
    osd_text_print_centered(OSD_SUBTITLE_ROW, "IMAGE ADJUST", OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND, 0);

    for (int i = 0; i < 5; i++)
    {
        uint8_t row = OSD_MENU_START_ROW + i;
        uint8_t color = OSD_COLOR_TEXT;
        uint8_t fg_color, bg_color;

        menu_item_colors(i == osd_menu_state.selected_item, i < 3 && osd_menu_state.tuning_mode, color, &fg_color, &bg_color);

        if (i == 0)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %d", "H_POS", shX_MAX - settings.shX);
        else if (i == 1)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %d", "V_POS", settings.shY);
        else if (i == 2)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %d", "DELAY", settings.delay);
        else if (i == 3)
            osd_text_print(row, 2, "RESET TO DEFAULTS", fg_color, bg_color, 0);
        else if (i == 4)
            osd_text_print(row, 2, "< BACK TO MAIN", fg_color, bg_color, 0);

        if (i < 3 && i == osd_menu_state.selected_item && osd_menu_state.tuning_mode)
            osd_text_set_char(row, 1, '>', fg_color, bg_color);
    }
}

static void render_mask_menu()
{
    osd_text_print_centered(OSD_SUBTITLE_ROW, "PIN INVERSION MASK", OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND, 0);

    const char *mask_items[] = {
        "F   (FREQ)",
        "SSI (HSYNC)",
        "KSI (VSYNC)",
        "I   (BRIGHT)",
        "R   (RED)",
        "G   (GREEN)",
        "B   (BLUE)",
    };

    for (int i = 0; i < 8; i++)
    {
        uint8_t row = OSD_MENU_START_ROW + i;
        uint8_t fg_color, bg_color;
        menu_item_colors(i == osd_menu_state.selected_item, false, OSD_COLOR_TEXT, &fg_color, &bg_color);

        if (i < 7)
        {
            uint8_t bit_pos = mask_bit_positions[i];
            bool bit_value = (settings.pin_inversion_mask >> bit_pos) & 1;
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-12s %s", mask_items[i], bit_value ? "ON" : "OFF");
        }
        else
            osd_text_print(row, 2, "< BACK TO CAPTURE", fg_color, bg_color, 0);
    }
}

#ifdef MAIN_ITEM_FF_OSD
static void render_ff_osd_menu()
{
    osd_text_print_centered(OSD_SUBTITLE_ROW, "FF OSD CONFIG", OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND, 0);

    const char *h_pos_names[] = {
        "LEFT",
        "LEFT-CENTER",
        "CENTER",
        "CENTER-RIGHT",
        "RIGHT",
    };

    for (int i = 0; i < 7; i++)
    {
        uint8_t row = OSD_MENU_START_ROW + i;
        uint8_t color = OSD_COLOR_TEXT;
        uint8_t fg_color, bg_color;

        // Dim ROWS and COLUMNS when protocol is FlashFloppy (not LCD_HD44780)
        if ((i == 2 || i == 3) && settings.ff_osd_config.i2c_protocol)
            color = OSD_COLOR_DIMMED;

        menu_item_colors(i == osd_menu_state.selected_item,
                         osd_menu_state.tuning_mode && ((i == 3 && !settings.ff_osd_config.i2c_protocol) || i == 4),
                         color, &fg_color, &bg_color);

        if (i == 0)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %s", "ENABLE", settings.ff_osd_config.enabled ? "ON" : "OFF");
        else if (i == 1)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %s", "PROTOCOL", settings.ff_osd_config.i2c_protocol ? "FLASHFLOPPY" : "LCD HD44780");
        else if (i == 2)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %d", "ROWS", settings.ff_osd_config.i2c_protocol ? ff_osd_display.rows : settings.ff_osd_config.rows);
        else if (i == 3)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %d", "COLUMNS", settings.ff_osd_config.i2c_protocol ? ff_osd_display.cols : settings.ff_osd_config.cols);
        else if (i == 4)
        {
            uint8_t hp = settings.ff_osd_config.h_position;
            const char *hp_name = (hp >= 1 && hp <= 5) ? h_pos_names[hp - 1] : "CENTER";
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %s", "H_POS", hp_name);
        }
        else if (i == 5)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-9s %s", "V_POS", settings.ff_osd_config.v_position ? "BOTTOM" : "TOP");
        else if (i == 6)
            osd_text_print(row, 2, "< BACK TO MAIN", fg_color, bg_color, 0);

        if ((i == 3 && !settings.ff_osd_config.i2c_protocol && i == osd_menu_state.selected_item && osd_menu_state.tuning_mode) ||
            (i == 4 && i == osd_menu_state.selected_item && osd_menu_state.tuning_mode))
            osd_text_set_char(row, 1, '>', fg_color, bg_color);
    }
}
#endif

#ifdef MAIN_ITEM_HARDWARE
static void render_hardware_menu()
{
    const char *drive_names[] = {
        "OFF",
        "A",
        "B",
    };

    osd_text_print_centered(OSD_SUBTITLE_ROW, "HARDWARE CONFIG", OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND, 0);

    for (int i = 0; i < 4; i++)
    {
        uint8_t row = OSD_MENU_START_ROW + i;
        uint8_t color = OSD_COLOR_TEXT;
        uint8_t fg_color, bg_color;

        // item 1 (RAM SIZE) is a direct toggle — never enters tuning mode
        bool item_in_tuning = (i != 1) && osd_menu_state.tuning_mode;
        menu_item_colors(i == osd_menu_state.selected_item, item_in_tuning, color, &fg_color, &bg_color);

        if (i == 0)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-12s %d", "ROM BANK", settings.hw_config.rom_bank);
        else if (i == 1)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-12s %d", "RAM (KB)", settings.hw_config.ram_size ? 1024 : 128);
        else if (i == 2)
            osd_text_printf(row, 2, fg_color, bg_color, 0, "%-12s %s", "GOTEK DRIVE", drive_names[settings.hw_config.gotek_drive]);
        else if (i == 3)
            osd_text_print(row, 2, "< BACK TO MAIN", fg_color, bg_color, 0);

        if (item_in_tuning && i == osd_menu_state.selected_item)
            osd_text_set_char(row, 1, '>', fg_color, bg_color);
    }
}
#endif

static void render_about_menu()
{
    osd_text_print_centered(OSD_SUBTITLE_ROW, "ABOUT", OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND, 0);

    osd_text_printf(OSD_MENU_START_ROW, 2, OSD_COLOR_TEXT, OSD_COLOR_BACKGROUND, 0, "VERSION   %s", FW_VERSION);
    osd_text_printf(OSD_MENU_START_ROW + 1, 2, OSD_COLOR_TEXT, OSD_COLOR_BACKGROUND, 0, "BOARD     %s", HW_VERSION);

    osd_text_print(OSD_MENU_START_ROW + 3, 2, GIT_REPO_URL_1, OSD_COLOR_TEXT, OSD_COLOR_BACKGROUND, 0);
    osd_text_print(OSD_MENU_START_ROW + 4, 2, GIT_REPO_URL_2, OSD_COLOR_TEXT, OSD_COLOR_BACKGROUND, 0);
    osd_text_print(OSD_MENU_START_ROW + 5, 2, GIT_REPO_URL_3, OSD_COLOR_TEXT, OSD_COLOR_BACKGROUND, 0);
#ifdef GIT_REPO_URL_4
    osd_text_print(OSD_MENU_START_ROW + 6, 2, GIT_REPO_URL_4, OSD_COLOR_TEXT, OSD_COLOR_BACKGROUND, 0);
#endif

    uint8_t fg_color, bg_color;
    menu_item_colors(osd_menu_state.selected_item == 0, false, OSD_COLOR_TEXT, &fg_color, &bg_color);

    osd_text_print(OSD_MENU_START_ROW + 7, 2, "< BACK TO MAIN", fg_color, bg_color, 0);
}

void osd_update_text_buffer()
{ // Clear text buffer
    osd_clear_text_buffer();

    // Draw header
#if defined(BOARD_LEO_V2) || defined(BOARD_LEO_V3) || defined(BOARD_LEO_V3_2040BT)
    const char *title = "ZX PENTAGON LEO 1024K";
#else
    const char *title = settings.video_out_type == VGA ? "ZX RGBI TO VGA CONVERTER" : "ZX RGBI TO HDMI CONVERTER";
#endif

    osd_text_print_centered(OSD_TITLE_ROW, title, OSD_COLOR_TEXT, OSD_COLOR_BACKGROUND, 0);
    osd_text_print_centered(OSD_SUBTITLE_ROW, "SETUP MENU", OSD_COLOR_SELECTED, OSD_COLOR_BACKGROUND, 0);

    // Render menu based on current menu type
    switch (osd_menu.current_menu)
    {
    case MENU_TYPE_MAIN:
        render_main_menu();
        break;

    case MENU_TYPE_OUTPUT:
        render_output_menu();
        break;

    case MENU_TYPE_CAPTURE:
        render_capture_menu();
        break;

    case MENU_TYPE_IMAGE_ADJUST:
        render_image_adjust_menu();
        break;

    case MENU_TYPE_MASK:
        render_mask_menu();
        break;

    case MENU_TYPE_CRT:
        render_crt_menu();
        break;

    case MENU_TYPE_ABOUT:
        render_about_menu();
        break;

#ifdef MAIN_ITEM_FF_OSD
    case MENU_TYPE_FF_OSD:
        render_ff_osd_menu();
        break;
#endif

#ifdef MAIN_ITEM_HARDWARE
    case MENU_TYPE_HARDWARE:
        render_hardware_menu();
        break;
#endif
    }
}

void osd_adjust_image_parameter(uint8_t param_index, int8_t direction)
{
    switch (param_index)
    {
    case 0: // Horizontal Position (shX)
        if (direction > 0 && settings.shX > shX_MIN)
            set_capture_shX(settings.shX - 1);
        else if (direction < 0 && settings.shX < shX_MAX)
            set_capture_shX(settings.shX + 1);

        break;

    case 1: // Vertical Position (shY)
        if (direction > 0 && settings.shY < shY_MAX)
            set_capture_shY(settings.shY + 1);
        else if (direction < 0 && settings.shY > shY_MIN)
            set_capture_shY(settings.shY - 1);

        break;

    case 2: // Delay
        if (direction > 0 && settings.delay < DELAY_MAX)
        {
            settings.delay++;
            set_capture_delay(settings.delay);
        }
        else if (direction < 0 && settings.delay > DELAY_MIN)
        {
            settings.delay--;
            set_capture_delay(settings.delay);
        }

        break;
    }
}

void osd_adjust_capture_parameter(uint8_t param_index, int8_t direction)
{
    static const uint32_t freq_presets[] = {
        7000000, // ZX Spectrum 16K/48K
        7093800, // ZX Spectrum 128/+2/+2A/+3
    };
    static const uint8_t freq_presets_count = count_of(freq_presets);

    switch (param_index)
    {
    case 0: // Frequency
    {
        uint32_t freq_step = 100; // Base step: 100Hz
        uint64_t current_time = time_us_64();

        // Determine which button is being used (UP or DOWN)
        uint8_t button_index = (direction > 0) ? 0 : 1; // 0=UP, 1=DOWN

        // Calculate step size based on key hold duration
        if (osd_buttons.key_held[button_index])
        {
            uint64_t hold_duration = current_time - osd_buttons.key_hold_start[button_index];

            // Progressive step increase based on hold time and current frequency alignment
            if (hold_duration > 5000000 && (settings.frequency % 100000 == 0))
                freq_step = 100000; // After 5 seconds and 100kHz alignment: 100kHz steps
            else if (hold_duration > 2000000 && (settings.frequency % 10000 == 0))
                freq_step = 10000; // After 2 seconds and 10kHz alignment: 10kHz steps
            else if (hold_duration > 1000000 && (settings.frequency % 1000 == 0))
                freq_step = 1000; // After 1 second and 1kHz alignment: 1kHz steps
            else
                freq_step = 100; // First second: 100Hz steps
        }

        uint32_t new_freq = settings.frequency;

        if (direction > 0 && new_freq <= FREQUENCY_MAX - freq_step)
            new_freq += freq_step;
        else if (direction < 0 && new_freq >= FREQUENCY_MIN + freq_step)
            new_freq -= freq_step;

        // Snap to the nearest preset in the direction of travel.
        // When counting up, pick the lowest preset above current freq;
        // when counting down, pick the highest preset below current freq.
        {
            uint32_t snap = 0;
            bool found = false;
            for (uint8_t i = 0; i < freq_presets_count; i++)
            {
                uint32_t preset = freq_presets[i];
                if (settings.frequency == preset)
                {
                    found = false; // Already at preset, keep counting past it
                    break;
                }
                if (direction > 0 && settings.frequency < preset && new_freq >= preset - freq_step)
                {
                    // Counting up: take the first (lowest) qualifying preset
                    snap = preset;
                    found = true;
                    break;
                }
                if (direction < 0 && settings.frequency > preset && new_freq <= preset + freq_step)
                {
                    // Counting down: keep updating to find the highest qualifying preset
                    snap = preset;
                    found = true;
                }
            }
            if (found)
            {
                new_freq = snap;
                osd_block_buttons_until_release();
            }
        }

        settings.frequency = new_freq;
        // Apply frequency change immediately
        set_capture_frequency(settings.frequency);
    }

    break;

    case 2: // External clock divider (only when cap_sync_mode == EXT)
        if (settings.cap_sync_mode == EXT)
        {
            if (direction > 0 && settings.ext_clk_divider < EXT_CLK_DIVIDER_MAX)
                settings.ext_clk_divider++;
            else if (direction < 0 && settings.ext_clk_divider > EXT_CLK_DIVIDER_MIN)
                settings.ext_clk_divider--;
            // Set the new divider immediately
            set_ext_clk_divider(settings.ext_clk_divider);
        }

        break;
    }
}

// the next / previous resolution of the output (in a loop) at the current rate; applied on SEL (tuning mode)
void osd_adjust_video_mode(int8_t direction)
{
    int count = video_mode_count(settings.video_out_type);
    int index = video_mode_index(settings.video_out_type, settings.video_out_mode);
    index = ((index < 0 ? 0 : index) + direction + count) % count;
    settings.video_out_mode = video_mode_select(settings.video_out_type, index, video_mode_pentagon(settings.video_out_mode));
}

#ifdef MAIN_ITEM_FF_OSD
void osd_adjust_ff_osd_parameter(uint8_t param_index, int8_t direction)
{
    switch (param_index)
    {
    case 0: // Enable toggle
        settings.ff_osd_config.enabled = !settings.ff_osd_config.enabled;

        if (settings.ff_osd_config.enabled)
            ff_osd_needs_i2c_init = true;

        break;

    case 1: // Protocol toggle
        settings.ff_osd_config.i2c_protocol = !settings.ff_osd_config.i2c_protocol;

        if (settings.ff_osd_config.i2c_protocol)
        {
            settings.ff_osd_config.cols = ff_osd_display.cols;
            settings.ff_osd_config.rows = ff_osd_display.rows;
        }
        else
            settings.ff_osd_config.rows = 2;

        ff_osd_needs_i2c_init = true;
        break;

    case 2: // Rows - toggle 2/4
        settings.ff_osd_config.rows = (settings.ff_osd_config.rows == 2) ? 4 : 2;

        if (settings.ff_osd_config.rows * settings.ff_osd_config.cols > 80)
            settings.ff_osd_config.cols = 20; // Adjust columns to max allowed for 4 rows

        if (!settings.ff_osd_config.i2c_protocol)
        {
            ff_osd_display.rows = settings.ff_osd_config.rows;
            ff_osd_display.cols = settings.ff_osd_config.cols;
        }

        break;

    case 3: // Columns - adjust with tuning mode
        if (direction > 0)
        {
            settings.ff_osd_config.cols = ff_osd_set_cols(settings.ff_osd_config.cols + 1);

            if (settings.ff_osd_config.rows * settings.ff_osd_config.cols > 80)
                settings.ff_osd_config.rows = 2;
        }
        else
            settings.ff_osd_config.cols = ff_osd_set_cols(settings.ff_osd_config.cols - 1);

        if (!settings.ff_osd_config.i2c_protocol)
        {
            ff_osd_display.rows = settings.ff_osd_config.rows;
            ff_osd_display.cols = settings.ff_osd_config.cols;
        }

        break;

    case 4: // H_POS - cycle 1-5
        settings.ff_osd_config.h_position = ff_osd_set_h_position(settings.ff_osd_config.h_position + direction);
        break;

    case 5: // V_POS - toggle top/bottom
        settings.ff_osd_config.v_position = !settings.ff_osd_config.v_position;
        break;
    }
}
#endif

#ifdef MAIN_ITEM_HARDWARE
void osd_adjust_hardware_parameter(uint8_t param_index, int8_t direction)
{
    switch (param_index)
    {
    case 0: // ROM BANK - adjust with tuning mode
        if (direction > 0 && settings.hw_config.rom_bank < HW_ROM_BANK_MAX)
            settings.hw_config.rom_bank++;
        else if (direction < 0 && settings.hw_config.rom_bank > HW_ROM_BANK_MIN)
            settings.hw_config.rom_bank--;

        break;

    case 2: // GOTEK DRIVE - adjust with tuning mode
        if (direction > 0 && settings.hw_config.gotek_drive < HW_GOTEK_DRIVE_MAX)
            settings.hw_config.gotek_drive++;
        else if (direction < 0 && settings.hw_config.gotek_drive > HW_GOTEK_DRIVE_MIN)
            settings.hw_config.gotek_drive--;

        hw_set_gotek_drive(settings.hw_config.gotek_drive);
        break;
    }
}
#endif