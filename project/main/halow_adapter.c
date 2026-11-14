#include "app_state.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "mmhalow.h"
#include <stdatomic.h>
#include <string.h>

typedef enum { RADIO_SCAN, RADIO_CONNECT, RADIO_DISCONNECT } radio_action_t;
typedef struct {
    radio_action_t action;
    char ssid[33];
    char password[65];
} radio_command_t;

static const char *TAG = "halow";
static QueueHandle_t commands, completed_scans;
static bool scan_pending;
static wifi_scan_results_t scan_buffer;
static atomic_bool radio_failed;
static atomic_int state = LINK_DOWN;
static atomic_int last_rssi = INT32_MIN;

static void station_changed(enum mmwlan_sta_state station)
{
    if (station != MMWLAN_STA_CONNECTED) atomic_store(&last_rssi, INT32_MIN);
    atomic_store(&state, station == MMWLAN_STA_CONNECTED ? LINK_UP :
                         station == MMWLAN_STA_CONNECTING ? LINK_CONNECTING : LINK_DOWN);
}

static void scan_received(const struct mmwlan_scan_result *result, void *arg)
{
    (void)arg;
    if (!result->ssid || !result->ssid_len || result->ssid_len > 32 ||
        memchr(result->ssid, 0, result->ssid_len)) return;
    char ssid[33] = {0};
    memcpy(ssid, result->ssid, result->ssid_len);
    for (unsigned i = 0; i < scan_buffer.count; i++) {
        wifi_scan_entry_t *entry = &scan_buffer.entries[i];
        if (strcmp(entry->ssid, ssid) != 0) continue;
        if (result->rssi > entry->rssi) entry->rssi = result->rssi;
        return;
    }
    if (scan_buffer.count == WIFI_SCAN_LIMIT) return;
    wifi_scan_entry_t *entry = &scan_buffer.entries[scan_buffer.count++];
    memcpy(entry->ssid, ssid, sizeof(entry->ssid));
    entry->rssi = result->rssi;
}

static void scan_completed(enum mmwlan_scan_state status, void *arg)
{
    (void)arg;
    scan_buffer.ok = status == MMWLAN_SCAN_SUCCESSFUL;
    ESP_LOGI(TAG, "Scan completed: status %d, %u networks", status, scan_buffer.count);
    xQueueOverwrite(completed_scans, &scan_buffer);
}

static bool radio_start(void)
{
    static bool initialized;
    if (initialized) return true;
    if (atomic_load(&radio_failed)) return false;

    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    struct mmwlan_version version = {0};
    if (mmhalow_init(&config) != ESP_OK ||
        mmwlan_get_version(&version) != MMWLAN_SUCCESS || !version.morse_chip_id) {
        ESP_LOGE(TAG, "MM6108 did not boot. Check the module and wireless switch.");
        atomic_store(&radio_failed, true);
        atomic_store(&state, LINK_ERROR);
        return false;
    }
    ESP_LOGI(TAG, "Radio ready: chip 0x%04lx", (unsigned long)version.morse_chip_id);
    initialized = true;
    return true;
}

static void radio_task(void *arg)
{
    (void)arg;
    radio_command_t command;
    for (;;) {
        if (xQueueReceive(commands, &command, pdMS_TO_TICKS(1000)) != pdTRUE) {
            if (atomic_load(&state) == LINK_UP) atomic_store(&last_rssi, mmwlan_get_rssi());
            continue;
        }
        if (command.action == RADIO_DISCONNECT) {
            if (atomic_load(&state) == LINK_UP || atomic_load(&state) == LINK_CONNECTING) {
                mmhalow_disconnect();
                station_changed(MMWLAN_STA_DISABLED);
            }
            continue;
        }
        bool ready = radio_start();
        if (command.action == RADIO_SCAN) {
            memset(&scan_buffer, 0, sizeof(scan_buffer));
            struct mmhalow_scan_args args = {
                .rx_cb = scan_received, .complete_cb = scan_completed,
            };
            if (!ready || mmhalow_scan(&args) != ESP_OK) {
                wifi_scan_results_t failure = {0};
                xQueueOverwrite(completed_scans, &failure);
            }
            continue;
        }
        if (!ready) continue;

        if (mmwlan_get_sta_state() != MMWLAN_STA_DISABLED) mmhalow_disconnect();
        mmhalow_wifi_config_t config = {.sta = MMWLAN_STA_ARGS_INIT};
        config.sta.ssid_len = strlen(command.ssid);
        config.sta.passphrase_len = strlen(command.password);
        memcpy(config.sta.ssid, command.ssid, config.sta.ssid_len);
        memcpy(config.sta.passphrase, command.password, config.sta.passphrase_len);
        config.sta.security_type = MMWLAN_SAE;
        atomic_store(&state, LINK_CONNECTING);
        if (mmhalow_set_config(WIFI_IF_STA, &config) != ESP_OK ||
            mmhalow_connect(station_changed) != ESP_OK) atomic_store(&state, LINK_ERROR);
    }
}

void halow_adapter_init(void)
{
    commands = xQueueCreate(4, sizeof(radio_command_t));
    completed_scans = xQueueCreate(1, sizeof(wifi_scan_results_t));
    if (!commands || !completed_scans ||
        xTaskCreate(radio_task, "halow", 6144, NULL, 4, NULL) != pdPASS) {
        atomic_store(&radio_failed, true);
    }
}

bool halow_adapter_available(void)
{
    return !atomic_load(&radio_failed);
}

link_state_t halow_adapter_state(void)
{
    return atomic_load(&state);
}

bool halow_adapter_connect(const char *ssid, const char *password)
{
    if (!halow_adapter_available() || scan_pending || !ssid[0] || !password[0]) return false;
    radio_command_t command = {.action = RADIO_CONNECT};
    strlcpy(command.ssid, ssid, sizeof(command.ssid));
    strlcpy(command.password, password, sizeof(command.password));
    return xQueueSend(commands, &command, 0) == pdTRUE;
}

void halow_adapter_disconnect(void)
{
    if (!halow_adapter_available()) return;
    const radio_command_t command = {.action = RADIO_DISCONNECT};
    xQueueSend(commands, &command, 0);
}

bool halow_adapter_scan_start(void)
{
    if (!halow_adapter_available()) return false;
    if (scan_pending) return true;
    const radio_command_t command = {.action = RADIO_SCAN};
    scan_pending = xQueueSend(commands, &command, 0) == pdTRUE;
    return scan_pending;
}

bool halow_adapter_scan_take(wifi_scan_results_t *results)
{
    if (!scan_pending || xQueueReceive(completed_scans, results, 0) != pdTRUE) return false;
    scan_pending = false;
    return true;
}

bool halow_adapter_get_rssi(int *rssi_dbm)
{
    int rssi = atomic_load(&last_rssi);
    if (rssi == INT32_MIN || halow_adapter_state() != LINK_UP) return false;
    *rssi_dbm = rssi;
    return true;
}
