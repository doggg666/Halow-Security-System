#include "app_state.h"
#include "app_config.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

enum {
    BACKGROUND = 0xF0F4F9,
    SURFACE = 0xFFFFFF,
    PANEL = 0xE8EFF8,
    OUTLINE = 0xDADCE0,
    INK = 0x202124,
    SECONDARY = 0x454B54,
    BLUE = 0x0B57D0,
    BLUE_TINT = 0xD3E3FD,
    BLUE_INK = 0x041E49,
    PRESSED = 0xC2D7F8,
};

typedef enum { BUTTON_QUIET, BUTTON_SELECTED, BUTTON_PRIMARY } button_style_t;

static const char *const camera_names[] = {CAMERA_NAME_0, CAMERA_NAME_1, CAMERA_NAME_2};
static lv_obj_t *camera_buttons[CAMERA_COUNT], *network_buttons[3];
static lv_obj_t *resolution_buttons[CAMERA_RESOLUTION_COUNT];
static lv_obj_t *camera_title, *camera_hint;
static lv_obj_t *fullscreen, *fullscreen_frame, *fullscreen_status;
static lv_obj_t *scrim, *ssid_input, *password_input, *keyboard, *settings_error;
static lv_obj_t *settings_status, *halow_quality, *settings_ip;
static lv_obj_t *scan_overlay, *scan_message, *scan_list;
static wifi_scan_results_t scan_results;
static bool scan_is_halow;
static lv_obj_t *dashboard_clock, *fullscreen_clock, *settings_clock;
static lv_obj_t *wifi_dot, *halow_dot;
static const char *const network_modes[] = {"Auto", "Wi-Fi", "HaLow"};
static void show_scan_results(void);

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color, int radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_radius(obj, radius, 0);
    return obj;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y,
                       uint32_t color, const lv_font_t *font)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, text);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_pos(obj, x, y);
    return obj;
}

static void update_halow_quality(void)
{
    if (!scrim) return;

    const char *quality;
    char reading[48];
    int rssi_dbm;
    if (!halow_adapter_available()) {
        quality = "HaLow link quality: Driver unavailable";
    } else if (halow_adapter_state() == LINK_CONNECTING) {
        quality = "HaLow link quality: Connecting";
    } else if (halow_adapter_state() != LINK_UP) {
        quality = "HaLow link quality: Disconnected";
    } else if (halow_adapter_get_rssi(&rssi_dbm)) {
        snprintf(reading, sizeof(reading), "HaLow link quality: %d dBm RSSI", rssi_dbm);
        quality = reading;
    } else {
        quality = "HaLow link quality: Waiting for signal reading";
    }

    if (strcmp(lv_label_get_text(halow_quality), quality) != 0) {
        lv_label_set_text(halow_quality, quality);
    }
}

static void update_settings_ip(void)
{
    if (!scrim) return;
    char address[16];
    char text[48];
    if (network_get_ipv4(address)) {
        snprintf(text, sizeof(text), "IP address: %s", address);
    } else {
        snprintf(text, sizeof(text), "IP address: Not assigned");
    }
    if (strcmp(lv_label_get_text(settings_ip), text) != 0) {
        lv_label_set_text(settings_ip, text);
    }
}

static void update_header(lv_timer_t *timer)
{
    (void)timer;
    wifi_scan_results_t completed;
    if (network_wifi_scan_take(&completed) && scan_list && !scan_is_halow) {
        scan_results = completed;
        show_scan_results();
    }
    if (halow_adapter_scan_take(&completed) && scan_list && scan_is_halow) {
        scan_results = completed;
        show_scan_results();
    }
    const uint32_t connected = 0x137333;
    const uint32_t inactive = 0x747B86;
    const uint32_t unavailable = 0xA35B00;
    lv_color_t wifi_color = lv_color_hex(network_wifi_link_up() ? connected : inactive);
    if (!lv_color_eq(lv_obj_get_style_bg_color(wifi_dot, LV_PART_MAIN), wifi_color)) {
        lv_obj_set_style_bg_color(wifi_dot, wifi_color, 0);
    }
    uint32_t halow_color = inactive;
    if (!halow_adapter_available() || halow_adapter_state() == LINK_ERROR) {
        halow_color = unavailable;
    } else if (halow_adapter_state() == LINK_UP) {
        halow_color = connected;
    } else if (halow_adapter_state() == LINK_CONNECTING) {
        halow_color = BLUE;
    }
    lv_color_t next_halow_color = lv_color_hex(halow_color);
    if (!lv_color_eq(lv_obj_get_style_bg_color(halow_dot, LV_PART_MAIN), next_halow_color)) {
        lv_obj_set_style_bg_color(halow_dot, next_halow_color, 0);
    }
    update_halow_quality();
    update_settings_ip();

    char display_time[32] = "--:--";
    time_t now = time(NULL);
    struct tm local_time;
    if (localtime_r(&now, &local_time) && local_time.tm_year >= 120) {
        int hour = local_time.tm_hour % 12;
        snprintf(display_time, sizeof(display_time), "%d:%02d %s",
                 hour ? hour : 12, local_time.tm_min, local_time.tm_hour < 12 ? "AM" : "PM");
    }
    if (strcmp(display_time, lv_label_get_text(dashboard_clock)) == 0) return;

    lv_label_set_text(dashboard_clock, display_time);
    lv_label_set_text(fullscreen_clock, display_time);
    if (scrim) lv_label_set_text(settings_clock, display_time);
}

static void style_button(lv_obj_t *button, button_style_t style)
{
    uint32_t background = SURFACE;
    uint32_t foreground = BLUE;
    if (style == BUTTON_PRIMARY) {
        background = BLUE;
        foreground = SURFACE;
    } else if (style == BUTTON_SELECTED) {
        background = BLUE_TINT;
        foreground = BLUE_INK;
    }

    lv_obj_set_style_bg_color(button, lv_color_hex(background), 0);
    lv_obj_set_style_bg_color(button,
                             lv_color_hex(style == BUTTON_PRIMARY ? 0x0842A0 : PRESSED),
                             LV_STATE_PRESSED);
    lv_obj_set_style_border_width(button, style == BUTTON_QUIET ? 1 : 0, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(OUTLINE), 0);
    lv_obj_set_style_text_color(lv_obj_get_child(button, 0), lv_color_hex(foreground), 0);
}

static lv_obj_t *button(lv_obj_t *parent, const char *text, int x, int y, int w,
                        button_style_t style, lv_event_cb_t callback, int value)
{
    lv_obj_t *obj = lv_button_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, 46);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_t *caption = label(obj, text, 0, 0, BLUE, &lv_font_montserrat_16);
    lv_obj_center(caption);
    style_button(obj, style);
    lv_obj_add_event_cb(obj, callback, LV_EVENT_CLICKED, (void *)(intptr_t)value);
    return obj;
}

static void update_camera(void)
{
    lv_label_set_text(camera_title, camera_names[g_app.active_camera]);
    for (int i = 0; i < CAMERA_COUNT; i++) {
        style_button(camera_buttons[i], i == g_app.active_camera ? BUTTON_SELECTED : BUTTON_QUIET);
    }
    for (int i = 0; i < CAMERA_RESOLUTION_COUNT; i++) {
        style_button(resolution_buttons[i],
                     i == (int)g_app.camera_resolution ? BUTTON_SELECTED : BUTTON_QUIET);
    }
    lv_label_set_text(camera_hint, camera_stream_url()[0] ?
                      "Connect to the camera network" : "Stream URL missing for this resolution");
}

static void update_network(void)
{
    if (!scrim) return;

    const char *status = g_app.status[0] ? g_app.status : "Waiting for network";

    lv_label_set_text(settings_status, status);
    update_halow_quality();
    update_settings_ip();
    for (int i = NET_AUTO; i <= NET_HALOW; i++) {
        style_button(network_buttons[i],
                     i == (int)g_app.selected_mode ? BUTTON_SELECTED : BUTTON_QUIET);
    }
}

static void select_camera(lv_event_t *event)
{
    camera_stream_select((int)(intptr_t)lv_event_get_user_data(event));
    update_camera();
}

static void close_fullscreen(lv_event_t *event)
{
    (void)event;
    lv_obj_add_flag(fullscreen, LV_OBJ_FLAG_HIDDEN);
}

static void open_fullscreen(lv_event_t *event)
{
    (void)event;
    const bool vga = g_app.camera_resolution == CAMERA_VGA;
    lv_obj_set_size(fullscreen_frame, vga ? 640 : 320, vga ? 480 : 240);
    lv_obj_center(fullscreen_frame);
    lv_label_set_text(fullscreen_status, camera_stream_url()[0] ?
                      "No video" : "Stream URL missing");
    lv_obj_remove_flag(fullscreen, LV_OBJ_FLAG_HIDDEN);
}

static void select_network(lv_event_t *event)
{
    network_select((network_mode_t)(intptr_t)lv_event_get_user_data(event));
    update_network();
}

static void select_resolution(lv_event_t *event)
{
    camera_stream_set_resolution((camera_resolution_t)(intptr_t)lv_event_get_user_data(event));
    update_camera();
}

static void reconnect(lv_event_t *event)
{
    (void)event;
    network_retry();
    update_network();
}

static void focus_input(lv_event_t *event)
{
    lv_keyboard_set_textarea(keyboard, lv_event_get_target(event));
    lv_obj_remove_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(settings_error, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(settings_ip, LV_OBJ_FLAG_HIDDEN);
}

static void close_scan(lv_event_t *event)
{
    (void)event;
    lv_obj_delete(scan_overlay);
    scan_overlay = NULL;
    scan_list = NULL;
}

static void choose_scan_network(lv_event_t *event)
{
    unsigned index = (unsigned)(uintptr_t)lv_event_get_user_data(event);
    const char *ssid = scan_results.entries[index].ssid;
    if (strcmp(lv_textarea_get_text(ssid_input), ssid) != 0) {
        lv_textarea_set_text(password_input, "");
    }
    lv_textarea_set_text(ssid_input, ssid);
    if (scan_is_halow) {
        network_select(NET_HALOW);
        update_network();
    }
    close_scan(event);
    lv_keyboard_set_textarea(keyboard, password_input);
    lv_obj_remove_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void show_scan_results(void)
{
    lv_obj_clean(scan_list);
    if (!scan_results.ok) {
        lv_label_set_text(scan_message, scan_is_halow && !halow_adapter_available() ?
                          "HaLow radio failed to start. Check the module and WM switch." :
                          "Scan failed. Try again after connecting finishes.");
    } else if (!scan_results.count) {
        lv_label_set_text(scan_message, scan_is_halow ? "No HaLow networks found." :
                          "No Wi-Fi networks found.");
    } else {
        lv_label_set_text(scan_message, "Select a network to fill in its name.");
        for (unsigned i = 0; i < scan_results.count; i++) {
            wifi_scan_entry_t *entry = &scan_results.entries[i];
            char text[64];
            snprintf(text, sizeof(text), "%s   %d dBm", entry->ssid, entry->rssi);
            lv_obj_t *row = button(scan_list, text, 4, i * 50, 532,
                                   BUTTON_QUIET, choose_scan_network, (int)i);
            lv_obj_t *caption = lv_obj_get_child(row, 0);
            lv_obj_set_width(caption, 500);
            lv_obj_set_style_text_align(caption, LV_TEXT_ALIGN_CENTER, 0);
            lv_label_set_long_mode(caption, LV_LABEL_LONG_DOT);
        }
    }
}

static void open_scan(lv_event_t *event)
{
    const bool halow = (network_mode_t)(intptr_t)lv_event_get_user_data(event) == NET_HALOW;
    scan_is_halow = halow;
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    scan_overlay = box(scrim, 0, 0, 800, 480, INK, 0);
    lv_obj_set_style_bg_opa(scan_overlay, LV_OPA_40, 0);
    lv_obj_add_flag(scan_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *dialog = box(scan_overlay, 108, 35, 584, 410, BACKGROUND, 28);
    label(dialog, halow ? "Scan HaLow" : "Scan Wi-Fi", 22, 18, INK, &lv_font_montserrat_24);
    scan_message = label(dialog, "", 22, 58, SECONDARY, &lv_font_montserrat_16);
    lv_obj_set_width(scan_message, 540);
    lv_label_set_long_mode(scan_message, LV_LABEL_LONG_WRAP);
    button(dialog, "Back", 444, 348, 118, BUTTON_QUIET, close_scan, 0);
    scan_list = box(dialog, 22, 94, 540, 245, PANEL, 12);
    lv_obj_add_flag(scan_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(scan_list, LV_DIR_VER);
    bool started = halow ? halow_adapter_scan_start() : network_wifi_scan_start();
    lv_label_set_text(scan_message, started ?
                      (halow ? "Scanning for nearby HaLow networks..." : "Scanning for nearby Wi-Fi networks...") :
                      (halow ? "Unable to start HaLow scan. Check the module and restart." :
                               "Unable to start Wi-Fi scan."));
}

static void close_settings(lv_event_t *event)
{
    (void)event;
    lv_obj_delete(scrim);
    scrim = NULL;
}

static void save_settings(lv_event_t *event)
{
    const char *ssid = lv_textarea_get_text(ssid_input);
    if (!network_wifi_configure(ssid, lv_textarea_get_text(password_input))) {
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(settings_ip, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(settings_error, ssid[0] ? "Check your network settings." : "Enter a network name.");
        lv_obj_remove_flag(settings_error, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    close_settings(event);
    network_select(g_app.selected_mode);
    update_network();
}

static lv_obj_t *input(lv_obj_t *parent, const char *hint, int y)
{
    lv_obj_t *obj = lv_textarea_create(parent);
    lv_obj_set_pos(obj, 24, y);
    lv_obj_set_size(obj, 438, 46);
    lv_textarea_set_one_line(obj, true);
    lv_textarea_set_placeholder_text(obj, hint);
    lv_obj_set_style_bg_color(obj, lv_color_hex(SURFACE), 0);
    lv_obj_set_style_radius(obj, 12, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(OUTLINE), 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(BLUE), LV_STATE_FOCUSED);
    lv_obj_set_style_text_color(obj, lv_color_hex(INK), 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(SECONDARY), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_text_font(obj, &lv_font_montserrat_16, 0);
    return obj;
}

static void open_settings(lv_event_t *event)
{
    (void)event;
    if (scrim) return;

    scrim = box(lv_layer_top(), 0, 0, 800, 480, INK, 0);
    lv_obj_set_style_bg_opa(scrim, LV_OPA_40, 0);
    lv_obj_add_flag(scrim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *dialog = box(scrim, 24, 20, 752, 440, BACKGROUND, 28);

    label(dialog, "Network settings", 24, 20, INK, &lv_font_montserrat_24);
    settings_clock = label(dialog, lv_label_get_text(dashboard_clock),
                           380, 24, SECONDARY, &lv_font_montserrat_16);
    label(dialog, "HaLow profile: " HALOW_PROFILE_NAME "  /  WPA3-SAE",
          24, 56, SECONDARY, &lv_font_montserrat_16);

    ssid_input = input(dialog, "Network name (SSID)", 88);
    lv_textarea_set_max_length(ssid_input, sizeof(g_app.ssid) - 1);
    lv_textarea_set_text(ssid_input, g_app.ssid);
    password_input = input(dialog, "WPA2 / WPA3 passphrase", 144);
    lv_textarea_set_password_mode(password_input, true);
    lv_textarea_set_max_length(password_input, sizeof(g_app.password) - 1);
    halow_quality = label(dialog, "", 24, 198, BLUE_INK, &lv_font_montserrat_16);
    settings_ip = label(dialog, "", 24, 222, INK, &lv_font_montserrat_16);

    lv_obj_t *controls = box(dialog, 478, 16, 250, 200, SURFACE, 20);
    label(controls, "Network mode", 12, 10, INK, &lv_font_montserrat_16);
    for (int i = NET_AUTO; i <= NET_HALOW; i++) {
        network_buttons[i] = button(controls, network_modes[i], 12 + i * 77, 36, 72,
                                    BUTTON_QUIET, select_network, i);
    }
    settings_status = label(controls, "", 12, 90, SECONDARY, &lv_font_montserrat_16);
    lv_obj_set_width(settings_status, 226);
    lv_label_set_long_mode(settings_status, LV_LABEL_LONG_WRAP);
    button(controls, LV_SYMBOL_REFRESH "  Reconnect", 12, 142, 226,
           BUTTON_PRIMARY, reconnect, 0);

    keyboard = lv_keyboard_create(dialog);
    lv_obj_align(keyboard, LV_ALIGN_TOP_LEFT, 24, 246);
    lv_obj_set_size(keyboard, 704, 132);
    lv_obj_set_style_bg_color(keyboard, lv_color_hex(PANEL), 0);
    lv_obj_set_style_radius(keyboard, 16, 0);
    lv_obj_set_style_bg_color(keyboard, lv_color_hex(SURFACE), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(keyboard, lv_color_hex(BLUE_TINT), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_set_style_text_color(keyboard, lv_color_hex(INK), LV_PART_ITEMS);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(ssid_input, focus_input, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(password_input, focus_input, LV_EVENT_CLICKED, NULL);

    settings_error = label(dialog, "", 24, 222, 0xB3261E, &lv_font_montserrat_16);
    lv_obj_add_flag(settings_error, LV_OBJ_FLAG_HIDDEN);
    button(dialog, "Scan Wi-Fi", 24, 382, 142, BUTTON_QUIET, open_scan, NET_WIFI);
    button(dialog, "Scan HaLow", 178, 382, 142, BUTTON_QUIET, open_scan, NET_HALOW);
    button(dialog, "Close", 482, 382, 100, BUTTON_QUIET, close_settings, 0);
    button(dialog, "Save", 596, 382, 132, BUTTON_PRIMARY, save_settings, 0);
    update_network();
}

void ui_init(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(BACKGROUND), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    lv_obj_t *icon = box(screen, 20, 16, 44, 44, BLUE_TINT, 16);
    lv_obj_t *mark = label(icon, LV_SYMBOL_VIDEO, 0, 0, BLUE, &lv_font_montserrat_24);
    lv_obj_center(mark);
    label(screen, "Fieldview", 76, 13, INK, &lv_font_montserrat_24);
    label(screen, "HaLow cameras", 76, 42, SECONDARY, &lv_font_montserrat_16);
    dashboard_clock = label(screen, "--:--", 0, 0, INK, &lv_font_montserrat_24);
    lv_obj_align(dashboard_clock, LV_ALIGN_TOP_MID, 0, 23);
    lv_obj_t *wifi = box(screen, 563, 22, 76, 32, SURFACE, 16);
    wifi_dot = box(wifi, 9, 12, 8, 8, OUTLINE, LV_RADIUS_CIRCLE);
    label(wifi, "Wi-Fi", 23, 6, INK, &lv_font_montserrat_16);
    lv_obj_t *halow = box(screen, 645, 22, 83, 32, SURFACE, 16);
    halow_dot = box(halow, 9, 12, 8, 8, OUTLINE, LV_RADIUS_CIRCLE);
    label(halow, "HaLow", 23, 6, INK, &lv_font_montserrat_16);
    lv_obj_t *settings_button = button(screen, LV_SYMBOL_SETTINGS, 734, 15, 46,
                                       BUTTON_QUIET, open_settings, 0);
    lv_obj_set_style_text_font(lv_obj_get_child(settings_button, 0), &lv_font_montserrat_24, 0);

    lv_obj_t *camera_card = box(screen, 20, 76, 760, 384, SURFACE, 28);
    camera_title = label(camera_card, "", 20, 17, INK, &lv_font_montserrat_24);
    resolution_buttons[CAMERA_QVGA] = button(camera_card, "320 x 240", 494, 17, 118,
                                             BUTTON_QUIET, select_resolution, CAMERA_QVGA);
    resolution_buttons[CAMERA_VGA] = button(camera_card, "640 x 480", 622, 17, 118,
                                            BUTTON_QUIET, select_resolution, CAMERA_VGA);

    lv_obj_t *view = box(camera_card, 20, 77, 720, 230, PANEL, 20);
    lv_obj_add_flag(view, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(view, open_fullscreen, LV_EVENT_CLICKED, NULL);
    lv_obj_t *play_badge = box(view, 330, 45, 60, 60, BLUE_TINT, LV_RADIUS_CIRCLE);
    lv_obj_remove_flag(play_badge, LV_OBJ_FLAG_CLICKABLE);
    mark = label(play_badge, LV_SYMBOL_VIDEO, 0, 0, BLUE, &lv_font_montserrat_24);
    lv_obj_center(mark);
    lv_obj_t *message = label(view, "No video", 0, 118, INK, &lv_font_montserrat_24);
    lv_obj_align(message, LV_ALIGN_TOP_MID, 0, 118);
    camera_hint = label(view, "", 0, 154, SECONDARY, &lv_font_montserrat_16);
    lv_obj_set_width(camera_hint, 436);
    lv_obj_set_style_text_align(camera_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(camera_hint, LV_ALIGN_TOP_MID, 0, 154);
    lv_obj_t *fullscreen_hint = label(view, "Tap for full screen", 0, 0, BLUE, &lv_font_montserrat_16);
    lv_obj_align(fullscreen_hint, LV_ALIGN_BOTTOM_MID, 0, -18);

    for (int i = 0; i < CAMERA_COUNT; i++) {
        camera_buttons[i] = button(camera_card, camera_names[i], 20 + i * 244, 322, 232,
                                   BUTTON_QUIET, select_camera, i);
    }
    update_camera();

    fullscreen = box(screen, 0, 0, 800, 480, 0x000000, 0);
    lv_obj_add_flag(fullscreen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(fullscreen, close_fullscreen, LV_EVENT_CLICKED, NULL);
    fullscreen_frame = box(fullscreen, 0, 0, 640, 480, 0x101216, 0);
    lv_obj_remove_flag(fullscreen_frame, LV_OBJ_FLAG_CLICKABLE);
    fullscreen_clock = label(fullscreen, "--:--", 0, 0, SURFACE, &lv_font_montserrat_16);
    lv_obj_set_width(fullscreen_clock, 80);
    lv_label_set_long_mode(fullscreen_clock, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(fullscreen_clock, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(fullscreen_clock, LV_ALIGN_TOP_RIGHT, 0, 12);
    fullscreen_status = label(fullscreen_frame, "", 0, 0, SURFACE, &lv_font_montserrat_24);
    lv_obj_center(fullscreen_status);
    lv_obj_t *back_hint = label(fullscreen_frame, "Tap anywhere to return", 0, 0,
                                SURFACE, &lv_font_montserrat_16);
    lv_obj_align(back_hint, LV_ALIGN_BOTTOM_MID, 0, -24);
    lv_obj_add_flag(fullscreen, LV_OBJ_FLAG_HIDDEN);
    update_header(NULL);
    lv_timer_create(update_header, 1000, NULL);
}
