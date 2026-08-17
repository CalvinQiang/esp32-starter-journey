#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#define LCD_WIDTH   280
#define LCD_HEIGHT  240

typedef struct {
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_handle_t panel;
    uint16_t *framebuffer;
    uint16_t *transfer_buffer;
} lcd_st7789_t;

/* The public API uses only logical landscape coordinates:
 * (0, 0) is top-left and (LCD_WIDTH - 1, LCD_HEIGHT - 1) is bottom-right. */
esp_err_t lcd_st7789_init(lcd_st7789_t *lcd);
void lcd_st7789_clear(lcd_st7789_t *lcd, uint16_t color);
void lcd_st7789_fill_rect(lcd_st7789_t *lcd, int x, int y, int width, int height, uint16_t color);
esp_err_t lcd_st7789_flush(lcd_st7789_t *lcd);
