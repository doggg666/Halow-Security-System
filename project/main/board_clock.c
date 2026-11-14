#include "board_clock.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_config.h"
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#define RTC_ADDRESS 0x51
#define RTC_SECONDS 0x02

static const char *TAG = "clock";
static i2c_master_dev_handle_t rtc;
static TaskHandle_t save_task;

void board_clock_attach(i2c_master_bus_handle_t bus)
{
    if (i2c_master_probe(bus, RTC_ADDRESS, 20) != ESP_OK) {
        ESP_LOGW(TAG, "RTC not detected");
        return;
    }
    const i2c_device_config_t config = {
        .device_address = RTC_ADDRESS,
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz = 100000,
    };
    if (i2c_master_bus_add_device(bus, &config, &rtc) != ESP_OK) {
        ESP_LOGW(TAG, "RTC setup failed");
    }
}

static int from_bcd(uint8_t value)
{
    return (value >> 4) * 10 + (value & 0xf);
}

static uint8_t to_bcd(int value)
{
    return ((value / 10) << 4) | (value % 10);
}

static void restore_clock(void)
{
    if (!rtc) return;
    const uint8_t reg = RTC_SECONDS;
    uint8_t data[7];
    if (i2c_master_transmit_receive(rtc, &reg, 1, data, sizeof(data), 20) != ESP_OK ||
        (data[0] & 0x80)) {
        ESP_LOGI(TAG, "RTC time unavailable, waiting for network time");
        return;
    }
    struct tm utc = {
        .tm_sec = from_bcd(data[0] & 0x7f),
        .tm_min = from_bcd(data[1] & 0x7f),
        .tm_hour = from_bcd(data[2] & 0x3f),
        .tm_mday = from_bcd(data[3] & 0x3f),
        .tm_mon = from_bcd(data[5] & 0x1f) - 1,
        .tm_year = 100 + from_bcd(data[6]),
        .tm_isdst = 0,
    };
    if (utc.tm_year < 126 || utc.tm_year > 199 || utc.tm_mon < 0 || utc.tm_mon > 11 ||
        utc.tm_mday < 1 || utc.tm_mday > 31 || utc.tm_hour > 23 ||
        utc.tm_min > 59 || utc.tm_sec > 59) return;

    // Called with TZ set to UTC before the user-facing local zone is applied.
    time_t seconds = mktime(&utc);
    if (seconds < 0) return;
    const struct timeval time = {.tv_sec = seconds};
    if (settimeofday(&time, NULL) == 0) ESP_LOGI(TAG, "Restored time from RTC");
}

static void save_clock(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        time_t seconds = time(NULL);
        struct tm utc;
        if (!gmtime_r(&seconds, &utc) || utc.tm_year < 126 || utc.tm_year > 199) continue;
        const uint8_t data[] = {
            RTC_SECONDS, to_bcd(utc.tm_sec), to_bcd(utc.tm_min), to_bcd(utc.tm_hour),
            to_bcd(utc.tm_mday), to_bcd(utc.tm_wday), to_bcd(utc.tm_mon + 1),
            to_bcd(utc.tm_year - 100),
        };
        esp_err_t err = i2c_master_transmit(rtc, data, sizeof(data), 20);
        if (err != ESP_OK) ESP_LOGW(TAG, "RTC write failed: %s", esp_err_to_name(err));
    }
}

static void time_synced(struct timeval *time)
{
    (void)time;
    ESP_LOGI(TAG, "Network time synchronized");
    if (save_task) xTaskNotifyGive(save_task);
}

static void on_ip(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    (void)id;
    (void)data;
    esp_err_t err = esp_netif_sntp_start();
    if (err != ESP_OK) ESP_LOGW(TAG, "Time sync could not start: %s", esp_err_to_name(err));
}

void board_clock_start(void)
{
    setenv("TZ", "UTC0", 1);
    tzset();
    restore_clock();
    setenv("TZ", CLOCK_TIME_ZONE, 1);
    tzset();

    if (rtc && xTaskCreate(save_clock, "rtc_save", 3072, NULL, 3, &save_task) != pdPASS) {
        ESP_LOGW(TAG, "RTC save task unavailable");
    }

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CLOCK_NTP_SERVER);
    config.start = false;
    config.wait_for_sync = false;
    config.sync_cb = time_synced;
#if CONFIG_LWIP_DHCP_GET_NTP_SRV && CONFIG_LWIP_SNTP_MAX_SERVERS >= 2
    config.server_from_dhcp = true;
    config.renew_servers_after_new_IP = true;
    config.index_of_first_server = 1;
#endif
    esp_err_t err = esp_netif_sntp_init(&config);
    if (err == ESP_OK) {
        err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_ip, NULL);
    }
    if (err != ESP_OK) ESP_LOGW(TAG, "Time sync unavailable: %s", esp_err_to_name(err));
}
