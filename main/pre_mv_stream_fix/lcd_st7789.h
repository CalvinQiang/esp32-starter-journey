#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"

#define LCD_WIDTH   280
#define LCD_HEIGHT  240

typedef enum {
    LCD_ORIENTATION_LANDSCAPE_0 = 0,
    LCD_ORIENTATION_LANDSCAPE_90,
    LCD_ORIENTATION_LANDSCAPE_180,
    LCD_ORIENTATION_LANDSCAPE_270,
} lcd_orientation_t;

typedef struct {
    esp_lcd_panel_handle_t panel;
    uint16_t *framebuffer;
} lcd_st7789_t;

/* The public API uses only logical landscape coordinates:
 * (0, 0) is top-left and (LCD_WIDTH - 1, LCD_HEIGHT - 1) is bottom-right. */
esp_err_t lcd_st7789_init(lcd_st7789_t *lcd);
esp_err_t lcd_st7789_set_orientation(lcd_st7789_t *lcd, lcd_orientation_t orientation);
void lcd_st7789_clear(lcd_st7789_t *lcd, uint16_t color);
void lcd_st7789_fill_rect(lcd_st7789_t *lcd, int x, int y, int width, int height, uint16_t color);
esp_err_t lcd_st7789_flush(lcd_st7789_t *lcd);
