#include "app_state.h"
#include "board_clock.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

static void gui_task(void *arg) { (void)arg; for(;;) { uint32_t wait=lv_timer_handler(); vTaskDelay(pdMS_TO_TICKS(wait<5?5:wait)); } }
app_state_t g_app = {.selected_mode=NET_AUTO,.wifi_state=LINK_DOWN,.halow_state=LINK_DOWN,.active_camera=0};
void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    board_display_init();
    network_init();
    board_clock_start();
    halow_adapter_init();
    network_select(NET_AUTO);
    ui_init();
    xTaskCreatePinnedToCore(gui_task,"lvgl",8192,NULL,5,NULL,1);
}
