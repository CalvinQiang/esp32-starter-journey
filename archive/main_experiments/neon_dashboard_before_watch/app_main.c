#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lcd_st7789.h"

#define USER_BUTTON_GPIO 39
#define FRAME_INTERVAL_MS 160
#define BUTTON_DEBOUNCE_MS 220

#define COLOR_BLACK      0x0000
#define COLOR_INK        0x0861
#define COLOR_PANEL      0x10A2
#define COLOR_GRID       0x18E3
#define COLOR_WHITE      0xFFFF
#define COLOR_CYAN       0x07FF
#define COLOR_BLUE       0x001F
#define COLOR_MAGENTA    0xF81F
#define COLOR_PINK       0xFBB5
#define COLOR_GREEN      0x07E0
#define COLOR_LIME       0xAFE5
#define COLOR_YELLOW     0xFFE0
#define COLOR_ORANGE     0xFD20

static const char *TAG = "neon_ui";

/* All glyphs are standard 5x7 bitmaps. They are rendered directly in the
 * verified logical coordinate system: left-to-right and top-to-bottom. */
typedef struct {
    char character;
    uint8_t rows[7];
} glyph_t;

static const glyph_t GLYPHS[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {':', {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00}},
    {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}},
    {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}},
    {'3', {0x1E, 0x01, 0x02, 0x0E, 0x01, 0x01, 0x1E}},
    {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}},
    {'5', {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}},
    {'6', {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}},
    {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}},
    {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}},
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
    {'D', {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {'G', {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E}},
    {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
    {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'V', {0x11, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04}},
    {'Y', {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}},
};

static const uint8_t *glyph_for(char character)
{
    for (size_t index = 0; index < sizeof(GLYPHS) / sizeof(GLYPHS[0]); ++index) {
        if (GLYPHS[index].character == character) {
            return GLYPHS[index].rows;
        }
    }
    return GLYPHS[0].rows;
}

static int text_width(const char *text, int scale)
{
    int width = 0;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        width += *cursor == ' ' ? 3 * scale : 6 * scale;
    }
    return width;
}

static void draw_text(lcd_st7789_t *lcd, int x, int y, const char *text, int scale, uint16_t color)
{
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        const uint8_t *glyph = glyph_for(*cursor);
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if ((glyph[row] & (1U << (4 - column))) != 0) {
                    lcd_st7789_fill_rect(lcd, x + column * scale, y + row * scale, scale, scale, color);
                }
            }
        }
        x += *cursor == ' ' ? 3 * scale : 6 * scale;
    }
}

static void draw_text_centered(lcd_st7789_t *lcd, int y, const char *text, int scale, uint16_t color)
{
    draw_text(lcd, (LCD_WIDTH - text_width(text, scale)) / 2, y, text, scale, color);
}

static void put_pixel(lcd_st7789_t *lcd, int x, int y, uint16_t color)
{
    lcd_st7789_fill_rect(lcd, x, y, 1, 1, color);
}

static void draw_line(lcd_st7789_t *lcd, int x0, int y0, int x1, int y1, uint16_t color)
{
    int delta_x = x0 < x1 ? x1 - x0 : x0 - x1;
    int step_x = x0 < x1 ? 1 : -1;
    int delta_y = y0 < y1 ? y1 - y0 : y0 - y1;
    int step_y = y0 < y1 ? 1 : -1;
    int error = delta_x - delta_y;

    while (true) {
        put_pixel(lcd, x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int twice_error = error * 2;
        if (twice_error > -delta_y) {
            error -= delta_y;
            x0 += step_x;
        }
        if (twice_error < delta_x) {
            error += delta_x;
            y0 += step_y;
        }
    }
}

static void draw_rect_outline(lcd_st7789_t *lcd, int x, int y, int width, int height, uint16_t color)
{
    lcd_st7789_fill_rect(lcd, x, y, width, 1, color);
    lcd_st7789_fill_rect(lcd, x, y + height - 1, width, 1, color);
    lcd_st7789_fill_rect(lcd, x, y, 1, height, color);
    lcd_st7789_fill_rect(lcd, x + width - 1, y, 1, height, color);
}

static void draw_circle_outline(lcd_st7789_t *lcd, int center_x, int center_y, int radius, uint16_t color)
{
    int x = radius;
    int y = 0;
    int error = 1 - radius;
    while (x >= y) {
        put_pixel(lcd, center_x + x, center_y + y, color);
        put_pixel(lcd, center_x + y, center_y + x, color);
        put_pixel(lcd, center_x - y, center_y + x, color);
        put_pixel(lcd, center_x - x, center_y + y, color);
        put_pixel(lcd, center_x - x, center_y - y, color);
        put_pixel(lcd, center_x - y, center_y - x, color);
        put_pixel(lcd, center_x + y, center_y - x, color);
        put_pixel(lcd, center_x + x, center_y - y, color);
        ++y;
        if (error < 0) {
            error += 2 * y + 1;
        } else {
            --x;
            error += 2 * (y - x) + 1;
        }
    }
}

static void draw_header(lcd_st7789_t *lcd, const char *page_title, int page)
{
    lcd_st7789_clear(lcd, COLOR_BLACK);
    lcd_st7789_fill_rect(lcd, 0, 0, LCD_WIDTH, 2, COLOR_CYAN);
    lcd_st7789_fill_rect(lcd, 0, 2, LCD_WIDTH, 1, COLOR_BLUE);
    draw_text(lcd, 12, 10, "NEON CORE", 2, COLOR_WHITE);
    draw_text(lcd, 207, 10, "LIVE", 2, COLOR_GREEN);
    draw_text(lcd, 12, 33, page_title, 2, COLOR_CYAN);
    lcd_st7789_fill_rect(lcd, 12, 49, LCD_WIDTH - 24, 1, COLOR_GRID);

    for (int star = 0; star < 17; ++star) {
        const int x = (star * 47 + page * 31) % LCD_WIDTH;
        const int y = 57 + ((star * 29 + page * 11) % 130);
        put_pixel(lcd, x, y, star % 3 == 0 ? COLOR_BLUE : COLOR_INK);
    }
}

static void draw_footer(lcd_st7789_t *lcd, int page)
{
    const uint16_t page_colors[] = {COLOR_CYAN, COLOR_MAGENTA, COLOR_GREEN};
    lcd_st7789_fill_rect(lcd, 0, 213, LCD_WIDTH, 1, COLOR_GRID);
    draw_text(lcd, 12, 221, "SW3", 2, COLOR_WHITE);
    draw_text(lcd, 56, 221, "MODE", 2, COLOR_GRID);

    for (int indicator = 0; indicator < 3; ++indicator) {
        const uint16_t color = indicator == page ? page_colors[indicator] : COLOR_PANEL;
        lcd_st7789_fill_rect(lcd, 196 + indicator * 22, 221, 14, 5, color);
        lcd_st7789_fill_rect(lcd, 196 + indicator * 22, 228, 14, 2, indicator == page ? COLOR_WHITE : COLOR_GRID);
    }
}

static void draw_pulse_page(lcd_st7789_t *lcd, uint32_t frame)
{
    draw_header(lcd, "PULSE", 0);

    const int center_x = 84;
    const int center_y = 131;
    const int pulse = (int)(frame % 20);
    draw_circle_outline(lcd, center_x, center_y, 52, COLOR_PANEL);
    draw_circle_outline(lcd, center_x, center_y, 42, COLOR_BLUE);
    draw_circle_outline(lcd, center_x, center_y, 28 + (pulse < 10 ? pulse / 2 : (19 - pulse) / 2), COLOR_CYAN);
    lcd_st7789_fill_rect(lcd, center_x - 4, center_y - 4, 8, 8, COLOR_WHITE);

    for (int segment = 0; segment < 16; ++segment) {
        const int bar_height = 8 + ((segment * 7 + frame) % 23);
        const uint16_t color = segment % 3 == 0 ? COLOR_MAGENTA : (segment % 3 == 1 ? COLOR_CYAN : COLOR_BLUE);
        lcd_st7789_fill_rect(lcd, 155 + segment * 6, 174 - bar_height, 3, bar_height, color);
    }

    draw_rect_outline(lcd, 155, 74, 108, 72, COLOR_PANEL);
    draw_text(lcd, 165, 84, "PULSE", 2, COLOR_WHITE);
    draw_text(lcd, 165, 108, "078", 4, COLOR_LIME);
    draw_text(lcd, 165, 134, "LIVE", 2, COLOR_CYAN);
    draw_footer(lcd, 0);
}

static void draw_status_bar(lcd_st7789_t *lcd, int y, const char *label, int value, uint16_t color)
{
    draw_text(lcd, 26, y, label, 2, COLOR_WHITE);
    lcd_st7789_fill_rect(lcd, 98, y + 2, 140, 10, COLOR_PANEL);
    lcd_st7789_fill_rect(lcd, 98, y + 2, value, 10, color);
    lcd_st7789_fill_rect(lcd, 242, y + 2, 8, 10, value > 108 ? COLOR_WHITE : COLOR_GRID);
}

static void draw_status_page(lcd_st7789_t *lcd, uint32_t frame)
{
    draw_header(lcd, "SYSTEM", 1);
    draw_rect_outline(lcd, 16, 63, 248, 126, COLOR_PANEL);
    draw_text(lcd, 26, 76, "READY", 2, COLOR_GREEN);
    draw_text(lcd, 186, 76, "01", 3, COLOR_WHITE);

    draw_status_bar(lcd, 105, "CORE", 108 + (frame % 27), COLOR_CYAN);
    draw_status_bar(lcd, 132, "MEM", 74 + ((frame * 3) % 46), COLOR_MAGENTA);
    draw_status_bar(lcd, 159, "SIGNAL", 116 + ((frame * 5) % 20), COLOR_LIME);

    for (int grid = 0; grid < 9; ++grid) {
        const uint16_t color = ((frame / 3 + grid) % 4 == 0) ? COLOR_YELLOW : COLOR_GRID;
        lcd_st7789_fill_rect(lcd, 31 + grid * 24, 195, 12, 3, color);
    }
    draw_footer(lcd, 1);
}

static void draw_scan_page(lcd_st7789_t *lcd, uint32_t frame)
{
    draw_header(lcd, "SCAN", 2);
    draw_rect_outline(lcd, 16, 63, 248, 126, COLOR_PANEL);

    for (int row = 0; row < 7; ++row) {
        for (int column = 0; column < 12; ++column) {
            const int phase = (int)((frame / 2 + row * 3 + column * 5) % 16);
            const uint16_t color = phase < 3 ? COLOR_CYAN : (phase == 3 ? COLOR_MAGENTA : COLOR_INK);
            lcd_st7789_fill_rect(lcd, 28 + column * 19, 76 + row * 15, 12, 8, color);
        }
    }

    const int sweep_x = 26 + (int)((frame * 4) % 224);
    lcd_st7789_fill_rect(lcd, sweep_x, 69, 3, 112, COLOR_LIME);
    draw_line(lcd, 28, 186, 138, 160, COLOR_BLUE);
    draw_line(lcd, 138, 160, 248, 186, COLOR_MAGENTA);
    draw_text(lcd, 28, 194, "SCAN", 2, COLOR_WHITE);
    draw_text(lcd, 185, 194, "READY", 2, COLOR_GREEN);
    draw_footer(lcd, 2);
}

static void draw_page(lcd_st7789_t *lcd, int page, uint32_t frame)
{
    switch (page) {
    case 0: draw_pulse_page(lcd, frame); break;
    case 1: draw_status_page(lcd, frame); break;
    default: draw_scan_page(lcd, frame); break;
    }
}

void app_main(void)
{
    lcd_st7789_t lcd;
    ESP_ERROR_CHECK(lcd_st7789_init(&lcd));

    gpio_config_t button_config = {
        .pin_bit_mask = 1ULL << USER_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&button_config));

    int page = 0;
    uint32_t frame = 0;
    bool was_pressed = false;
    int64_t last_press_us = 0;

    while (true) {
        const bool pressed = gpio_get_level(USER_BUTTON_GPIO) == 0;
        const int64_t now_us = esp_timer_get_time();
        if (pressed && !was_pressed && (now_us - last_press_us) > BUTTON_DEBOUNCE_MS * 1000LL) {
            page = (page + 1) % 3;
            last_press_us = now_us;
            ESP_LOGI(TAG, "SW3 selected page %d", page);
        }
        was_pressed = pressed;

        draw_page(&lcd, page, frame++);
        ESP_ERROR_CHECK(lcd_st7789_flush(&lcd));
        vTaskDelay(pdMS_TO_TICKS(FRAME_INTERVAL_MS));
    }
}
