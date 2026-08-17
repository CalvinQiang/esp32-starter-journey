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
} lcd_st7789_t;

/* All drawing functions use normal landscape coordinates:
 * (0, 0) = physical top-left; (279, 239) = physical bottom-right. */
/* Initializes only the physical panel. LVGL obtains io/panel from this structure. */
esp_err_t lcd_st7789_init_panel(lcd_st7789_t *lcd);
/* Legacy framebuffer helper kept for non-LVGL demos. */
esp_err_t lcd_st7789_init(lcd_st7789_t *lcd);
void lcd_st7789_clear(lcd_st7789_t *lcd, uint16_t color);
void lcd_st7789_fill_rect(lcd_st7789_t *lcd, int x, int y, int width, int height, uint16_t color);
esp_err_t lcd_st7789_flush(lcd_st7789_t *lcd);
esp_err_t lcd_st7789_flush_area(lcd_st7789_t *lcd, int x, int y, int width, int height);
