#pragma once
#include <stdbool.h>
#include "esp_event.h"
typedef enum { NET_AUTO=0, NET_WIFI, NET_HALOW } network_mode_t;
typedef enum { LINK_DOWN=0, LINK_CONNECTING, LINK_UP, LINK_ERROR } link_state_t;
typedef enum { CAMERA_QVGA, CAMERA_VGA, CAMERA_RESOLUTION_COUNT } camera_resolution_t;
typedef struct { network_mode_t selected_mode; link_state_t wifi_state, halow_state; int active_camera; camera_resolution_t camera_resolution; char ssid[33]; char password[65]; char status[80]; } app_state_t;
extern app_state_t g_app;
void network_init(void);
void network_select(network_mode_t mode);
void network_retry(void);
bool network_wifi_link_up(void);
/* IPv4 text needs 16 bytes including the terminator. */
bool network_get_ipv4(char address[16]);
#define WIFI_SCAN_LIMIT 12
typedef struct {
    char ssid[33];
    int rssi;
} wifi_scan_entry_t;
typedef struct {
    bool ok;
    unsigned count;
    wifi_scan_entry_t entries[WIFI_SCAN_LIMIT];
} wifi_scan_results_t;
bool network_wifi_scan_start(void);
bool network_wifi_scan_take(wifi_scan_results_t *results);
bool network_wifi_configure(const char *ssid, const char *password);
void halow_adapter_init(void);
bool halow_adapter_available(void);
link_state_t halow_adapter_state(void);
bool halow_adapter_connect(const char *ssid, const char *password);
void halow_adapter_disconnect(void);
bool halow_adapter_scan_start(void);
bool halow_adapter_scan_take(wifi_scan_results_t *results);
/* Read cached RSSI in dBm for the current HaLow association. False means no sample. */
bool halow_adapter_get_rssi(int *rssi_dbm);
void camera_stream_select(int index);
void camera_stream_set_resolution(camera_resolution_t resolution);
const char *camera_stream_url(void);
void camera_stream_stop(void);
void ui_init(void);
void board_display_init(void);
