#include "board_pins.h"
#include "board_clock.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "lvgl.h"
#include "driver/i2c_master.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

enum { LCD_BOUNCE_LINES = 20, LVGL_DRAW_LINES = 16 };

static lv_display_t *disp; static lv_indev_t *touch; static void *draw_buf; static i2c_master_dev_handle_t touch_dev; static esp_lcd_panel_handle_t panel;
static void lv_tick(void *arg){(void)arg;lv_tick_inc(2);}
static bool flush_ready(esp_lcd_panel_handle_t p, const esp_lcd_rgb_panel_event_data_t *edata, void *ctx) { (void)p;(void)edata; lv_display_flush_ready((lv_display_t*)ctx); return false; }
static void flush_cb(lv_display_t *d,const lv_area_t *a,uint8_t *px) { esp_lcd_panel_draw_bitmap(panel,a->x1,a->y1,a->x2+1,a->y2+1,px); }
/* GT911 reports a ready bit followed by one 8-byte record per touch.
 * Each record starts with a tracking ID, then little-endian X and Y.
 */
static void touch_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    static lv_point_t point;
    static bool pressed;
    static bool logged_touch;
    const uint8_t status_reg[] = {0x81, 0x4E};
    const uint8_t first_point_reg[] = {0x81, 0x4F};
    const uint8_t clear_status[] = {0x81, 0x4E, 0};
    uint8_t status;

    if (i2c_master_transmit_receive(touch_dev, status_reg, sizeof(status_reg),
                                    &status, 1, 20) != ESP_OK) {
        pressed = false;
    } else if (status & 0x80) {
        uint8_t touch_count = status & 0x0F;
        uint8_t record[8];
        pressed = touch_count > 0 && touch_count <= 5 &&
                  i2c_master_transmit_receive(touch_dev, first_point_reg,
                                              sizeof(first_point_reg), record,
                                              sizeof(record), 20) == ESP_OK;
        if (pressed) {
            uint16_t x = record[1] | ((uint16_t)record[2] << 8);
            uint16_t y = record[3] | ((uint16_t)record[4] << 8);
            point.x = x < LCD_H_RES ? x : LCD_H_RES - 1;
            point.y = y < LCD_V_RES ? y : LCD_V_RES - 1;
            if (!logged_touch) {
                ESP_LOGI("touch", "First touch report received");
                logged_touch = true;
            }
        }
        i2c_master_transmit(touch_dev, clear_status, sizeof(clear_status), 20);
    }

    data->point = point;
    data->state = pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}

static void init_touch(i2c_master_bus_handle_t bus)
{
    const uint8_t addresses[] = {TOUCH_I2C_ADDR, 0x14};
    const uint8_t product_reg[] = {0x81, 0x40};
    for (size_t i = 0; i < sizeof(addresses); i++) {
        if (i2c_master_probe(bus, addresses[i], 20) != ESP_OK) continue;

        const i2c_device_config_t config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addresses[i],
            .scl_speed_hz = 100000,
        };
        if (i2c_master_bus_add_device(bus, &config, &touch_dev) != ESP_OK) continue;

        uint8_t id[4] = {0};
        if (i2c_master_transmit_receive(touch_dev, product_reg, sizeof(product_reg),
                                        id, sizeof(id), 20) == ESP_OK &&
            id[0] == '9' && id[1] == '1' && id[2] == '1') {
            touch = lv_indev_create();
            if (!touch) break;
            lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
            lv_indev_set_display(touch, disp);
            lv_indev_set_read_cb(touch, touch_read);
            ESP_LOGI("touch", "GT911 detected at 0x%02x", (unsigned)addresses[i]);
            return;
        }
        i2c_master_bus_rm_device(touch_dev);
        touch_dev = NULL;
    }
    ESP_LOGW("touch", "Unable to initialize GT911 on SDA %d / SCL %d",
             TOUCH_I2C_SDA, TOUCH_I2C_SCL);
}

void board_display_init(void) {
    const esp_lcd_rgb_panel_config_t c={.clk_src=LCD_CLK_SRC_DEFAULT,.timings={.pclk_hz=16000000,.h_res=LCD_H_RES,.v_res=LCD_V_RES,.hsync_pulse_width=4,.hsync_back_porch=8,.hsync_front_porch=8,.vsync_pulse_width=4,.vsync_back_porch=8,.vsync_front_porch=8,.flags.pclk_active_neg=1},.data_width=16,.bits_per_pixel=16,.num_fbs=2,.bounce_buffer_size_px=LCD_H_RES * LCD_BOUNCE_LINES,.dma_burst_size=64,.hsync_gpio_num=LCD_PIN_HSYNC,.vsync_gpio_num=LCD_PIN_VSYNC,.de_gpio_num=LCD_PIN_DE,.pclk_gpio_num=LCD_PIN_PCLK,.disp_gpio_num=LCD_PIN_DISP,.data_gpio_nums={LCD_PIN_B0,LCD_PIN_B1,LCD_PIN_B2,LCD_PIN_B3,LCD_PIN_B4,LCD_PIN_G0,LCD_PIN_G1,LCD_PIN_G2,LCD_PIN_G3,LCD_PIN_G4,LCD_PIN_G5,LCD_PIN_R0,LCD_PIN_R1,LCD_PIN_R2,LCD_PIN_R3,LCD_PIN_R4},.flags.fb_in_psram=true};
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&c,&panel)); ESP_ERROR_CHECK(esp_lcd_panel_reset(panel)); ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    if (c.disp_gpio_num >= 0) ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
    lv_init(); const esp_timer_create_args_t tick_args={.callback=lv_tick,.name="lv_tick"}; esp_timer_handle_t tick_timer; ESP_ERROR_CHECK(esp_timer_create(&tick_args,&tick_timer)); ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer,2000)); disp=lv_display_create(LCD_H_RES,LCD_V_RES); lv_display_set_color_format(disp,LV_COLOR_FORMAT_RGB565); lv_display_set_flush_cb(disp,flush_cb);
    // Keep partial rendering in internal RAM to reduce contention with RGB scanout.
    const size_t draw_size = LCD_H_RES * LVGL_DRAW_LINES * sizeof(uint16_t);
    draw_buf = heap_caps_malloc(draw_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!draw_buf) {
        ESP_LOGW("display", "Internal draw buffer unavailable, using PSRAM");
        draw_buf = heap_caps_malloc(draw_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    ESP_ERROR_CHECK(draw_buf ? ESP_OK : ESP_ERR_NO_MEM);
    lv_display_set_buffers(disp, draw_buf, NULL, draw_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    const esp_lcd_rgb_panel_event_callbacks_t cb = {.on_color_trans_done = flush_ready};
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel, &cb, disp));
    i2c_master_bus_handle_t bus; i2c_master_bus_config_t bc={.i2c_port=I2C_NUM_0,.sda_io_num=TOUCH_I2C_SDA,.scl_io_num=TOUCH_I2C_SCL,.clk_source=I2C_CLK_SRC_DEFAULT,.glitch_ignore_cnt=7,.flags.enable_internal_pullup=true}; ESP_ERROR_CHECK(i2c_new_master_bus(&bc,&bus));
    init_touch(bus);
    board_clock_attach(bus);
}
