idf_component_register(SRCS "app_main.c" "ui.c" "network.c" "halow_adapter.c" "camera_stream.c" "board_display.c" "board_clock.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_event esp_netif esp_wifi nvs_flash esp_driver_gpio esp_driver_i2c esp_lcd esp_timer)
