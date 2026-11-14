#include "app_state.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
static const char *TAG="network";
static bool started;
static bool scan_pending; /* Accessed only by the UI task. */
static QueueHandle_t scan_queue;

static void wifi_scan_task(void *arg)
{
    (void)arg;
    wifi_scan_results_t results = {0};
    if (esp_wifi_scan_start(NULL, true) == ESP_OK) {
        wifi_ap_record_t records[WIFI_SCAN_LIMIT];
        uint16_t count = WIFI_SCAN_LIMIT;
        results.ok = esp_wifi_scan_get_ap_records(&count, records) == ESP_OK;
        if (!results.ok) esp_wifi_clear_ap_list();
        for (uint16_t i = 0; results.ok && i < count; i++) {
            if (!records[i].ssid[0]) continue;
            bool duplicate = false;
            for (unsigned j = 0; j < results.count; j++) {
                if (strncmp(results.entries[j].ssid, (const char *)records[i].ssid, 32) == 0)
                    duplicate = true;
            }
            if (duplicate) continue;
            wifi_scan_entry_t *entry = &results.entries[results.count++];
            memcpy(entry->ssid, records[i].ssid, 32);
            entry->rssi = records[i].rssi;
        }
    }
    xQueueOverwrite(scan_queue, &results);
    vTaskDelete(NULL);
}

bool network_wifi_scan_start(void)
{
    if (scan_pending) return true;
    if (!scan_queue) scan_queue = xQueueCreate(1, sizeof(wifi_scan_results_t));
    if (!scan_queue) return false;
    scan_pending = xTaskCreate(wifi_scan_task, "wifi_scan", 4096, NULL, 4, NULL) == pdPASS;
    return scan_pending;
}

bool network_wifi_scan_take(wifi_scan_results_t *results)
{
    if (!scan_pending || xQueueReceive(scan_queue, results, 0) != pdTRUE) return false;
    scan_pending = false;
    return true;
}

bool network_wifi_link_up(void)
{
    wifi_ap_record_t ap;
    return started && g_app.wifi_state == LINK_UP && esp_wifi_sta_get_ap_info(&ap) == ESP_OK;
}

bool network_get_ipv4(char address[16])
{
    const bool halow = g_app.selected_mode == NET_HALOW;
    if (halow ? halow_adapter_state() != LINK_UP : !network_wifi_link_up()) return false;

    // Morse Micro uses the default station key. The built-in Wi-Fi has its own key.
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey(halow ? "WIFI_STA_DEF" : "FIELDVIEW_WIFI");
    esp_netif_ip_info_t info;
    if (!netif || !esp_netif_is_netif_up(netif) ||
        esp_netif_get_ip_info(netif, &info) != ESP_OK || !info.ip.addr) return false;

    snprintf(address, 16, IPSTR, IP2STR(&info.ip));
    return true;
}
static void wifi_event(void *arg, esp_event_base_t base,int32_t id,void *data) {
    if(base==WIFI_EVENT && id==WIFI_EVENT_STA_START) { if(g_app.selected_mode!=NET_HALOW && g_app.ssid[0]) esp_wifi_connect(); }
    if(base==WIFI_EVENT && id==WIFI_EVENT_STA_CONNECTED) { g_app.wifi_state=LINK_UP; strlcpy(g_app.status,"Wi-Fi connected",sizeof(g_app.status)); }
    if(base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED) { g_app.wifi_state=LINK_DOWN; strlcpy(g_app.status,"Wi-Fi disconnected · retry available",sizeof(g_app.status)); }
    if(base==IP_EVENT && id==IP_EVENT_STA_GOT_IP) { g_app.wifi_state=LINK_UP; strlcpy(g_app.status,"Connected · camera network ready",sizeof(g_app.status)); }
}
void network_init(void) { ESP_ERROR_CHECK(esp_netif_init()); ESP_ERROR_CHECK(esp_event_loop_create_default()); esp_netif_inherent_config_t base = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    base.if_key = "FIELDVIEW_WIFI";
    esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_WIFI_STA();
    netif_config.base = &base;
    esp_netif_t *netif = esp_netif_new(&netif_config);
    ESP_ERROR_CHECK(netif ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(esp_netif_attach_wifi_station(netif));
    ESP_ERROR_CHECK(esp_wifi_set_default_wifi_sta_handlers()); wifi_init_config_t c=WIFI_INIT_CONFIG_DEFAULT(); ESP_ERROR_CHECK(esp_wifi_init(&c)); ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event,NULL)); ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_event,NULL)); ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM)); ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA)); ESP_ERROR_CHECK(esp_wifi_start()); started=true; }
bool network_wifi_configure(const char *ssid,const char *password) { if(!ssid||!ssid[0]) return false; if (g_app.selected_mode == NET_HALOW && (!password || !password[0])) return false; if (ssid != g_app.ssid) strlcpy(g_app.ssid,ssid,sizeof(g_app.ssid)); if (password != g_app.password) strlcpy(g_app.password,password?password:"",sizeof(g_app.password)); if (g_app.selected_mode == NET_HALOW) return true; wifi_config_t cfg={0}; strlcpy((char*)cfg.sta.ssid,g_app.ssid,sizeof(cfg.sta.ssid)); strlcpy((char*)cfg.sta.password,g_app.password,sizeof(cfg.sta.password)); cfg.sta.threshold.authmode=g_app.password[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN; cfg.sta.sae_pwe_h2e=WPA3_SAE_PWE_BOTH; if(esp_wifi_set_config(WIFI_IF_STA,&cfg)!=ESP_OK) return false; return true; }
void network_select(network_mode_t mode)
{
    g_app.selected_mode = mode;
    if (mode == NET_HALOW) {
        esp_wifi_disconnect();
        if (!g_app.ssid[0] || !g_app.password[0]) {
            strlcpy(g_app.status, "Enter HaLow SSID and passphrase", sizeof(g_app.status));
        } else if (halow_adapter_connect(g_app.ssid, g_app.password)) {
            strlcpy(g_app.status, "Connecting to HaLow profile", sizeof(g_app.status));
        } else {
            strlcpy(g_app.status, "Unable to start HaLow connection", sizeof(g_app.status));
        }
        return;
    }
    halow_adapter_disconnect();
    if (started && g_app.ssid[0]) {
        if (!network_wifi_configure(g_app.ssid, g_app.password)) {
            strlcpy(g_app.status, "Check Wi-Fi credentials", sizeof(g_app.status));
            return;
        }
        g_app.wifi_state = LINK_CONNECTING;
        strlcpy(g_app.status, mode == NET_AUTO ? "Auto: connecting over Wi-Fi" :
                "Connecting over Wi-Fi", sizeof(g_app.status));
        esp_wifi_connect();
    } else {
        strlcpy(g_app.status, "Add Wi-Fi or HaLow credentials in Settings", sizeof(g_app.status));
    }
}
void network_retry(void)
{
    if (g_app.selected_mode != NET_HALOW) esp_wifi_disconnect();
    network_select(g_app.selected_mode);
}
