#pragma once
#include "driver/gpio.h"

/* Elecrow CrowPanel Advance 5-inch ESP32-S3 V1.0 pin map.
 * Display RGB pins from Elecrow's V1.0 repository README/schematic.
 * HaLow signals from V1.0 Eagle schematic review; shared mux route required.
 */
#define LCD_H_RES 800
#define LCD_V_RES 480
#define LCD_PIN_B0 GPIO_NUM_21
#define LCD_PIN_B1 GPIO_NUM_47
#define LCD_PIN_B2 GPIO_NUM_48
#define LCD_PIN_B3 GPIO_NUM_45
#define LCD_PIN_B4 GPIO_NUM_38
#define LCD_PIN_G0 GPIO_NUM_9
#define LCD_PIN_G1 GPIO_NUM_10
#define LCD_PIN_G2 GPIO_NUM_11
#define LCD_PIN_G3 GPIO_NUM_12
#define LCD_PIN_G4 GPIO_NUM_13
#define LCD_PIN_G5 GPIO_NUM_14
#define LCD_PIN_R0 GPIO_NUM_7
#define LCD_PIN_R1 GPIO_NUM_17
#define LCD_PIN_R2 GPIO_NUM_18
#define LCD_PIN_R3 GPIO_NUM_3
#define LCD_PIN_R4 GPIO_NUM_46
#define LCD_PIN_HSYNC GPIO_NUM_40
#define LCD_PIN_VSYNC GPIO_NUM_41
#define LCD_PIN_DE GPIO_NUM_42
#define LCD_PIN_PCLK GPIO_NUM_39
#define LCD_PIN_DISP GPIO_NUM_NC
#define LCD_PIN_BL GPIO_NUM_NC /* V1.0 backlight is board managed via expander. */
#define TOUCH_I2C_SDA GPIO_NUM_15
#define TOUCH_I2C_SCL GPIO_NUM_16
#define TOUCH_I2C_ADDR 0x5D

#define HALOW_PIN_MOSI GPIO_NUM_6
#define HALOW_PIN_MISO GPIO_NUM_4
#define HALOW_PIN_SCLK GPIO_NUM_5
/* Leave unset pending board routing/vendor driver validation. */
#define HALOW_PIN_CS GPIO_NUM_NC
#define HALOW_PIN_IRQ GPIO_NUM_NC
#define HALOW_PIN_RESET GPIO_NUM_NC
#define HALOW_PIN_WAKE GPIO_NUM_NC
/* The CrowPanel V1.0 does not route the DAC00106D BUSY pad. */
#define HALOW_PIN_BUSY GPIO_NUM_NC
