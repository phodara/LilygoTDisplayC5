#pragma once

// LilyGO T-Display C5 basics.
// Source pins follow LilyGO's public T-Display-C5 board_config.h.

#define IIC_SCL_PIN 3
#define IIC_SDA_PIN 2

#define LCD_WIDTH 170
#define LCD_HEIGHT 320

#define BUTTON_PIN 0
#define BUTTON_BOOT 28

#define AXP2602_INT 10

#define LCD_RST 23
#define LCD_SCK 7
#define LCD_CS 26
#define LCD_DC 8
#define LCD_MOSI 9
#define LCD_BLK_POWER 25

#define TOUCH_INT 27
#define TOUCH_RST 24
