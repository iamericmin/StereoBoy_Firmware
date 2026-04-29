/*
 * Copyright (c) 2021 Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * 
 */

#ifndef _PICO_ST7789_H_
#define _PICO_ST7789_H_


// #include "hardware/spi.h"
#include "visualizers.h"
#include "lib/sb_util/global_vars.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"

#define BLACK   0x0000
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define WHITE   0xFFFF
#define YELLOW  0xFFE0
#define CYAN    0x07FF
#define MAGENTA 0xF81F

#define HIGHLIGHT_COLOR_PRIMARY 0x049F
#define HIGHLIGHT_COLOR_SECONDARY 0x7bcf

#define BACKLIGHT_PIN 5 
#define MAX_BRIGHTNESS 65535

// void st7789_init(const st7789_t* config, uint16_t width, uint16_t height);
void st7789_write(const void* data, size_t len);
void st7789_put(uint16_t pixel);
void st7789_fill(uint16_t pixel);
void st7789_ramwr(void);
void st7789_set_cursor(uint16_t x, uint16_t y);
void st7789_vertical_scroll(uint16_t row);
void st7789_set_window(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye);
void st7789_draw_string(uint16_t x, uint16_t y, const char *text, uint16_t color);
void set_pixel(uint16_t x, uint16_t y, uint16_t color);
void lcd_draw_char(uint16_t x, uint16_t y, char c, uint16_t color);

void st7789_caset(uint16_t xs, uint16_t xe);
void st7789_raset(uint16_t ys, uint16_t ye);

void st7789_cmd(uint8_t cmd, const uint8_t* data, size_t len);
void st7789_set_brightness(uint16_t brightness);
void st7789_decrease_brightness(float x);
void st7789_increase_brightness(float x);
void st7789_update_brightness(float x);


#endif
