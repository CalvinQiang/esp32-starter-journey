#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lcd_st7789.h"

#define SW3_GPIO                     39
#define FRAME_INTERVAL_MS             85
#define BUTTON_DEBOUNCE_MS            220
#define FRAME_BYTES                   (LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t))

#define BLACK         0x0000
#define NAVY          0x0822
#define PANEL         0x1084
#define GRID          0x18E6
#define WHITE         0xFFFF
#define CYAN          0x07FF
#define BLUE          0x03BF
#define PURPLE        0x781F
#define MAGENTA       0xF81F
#define PINK          0xFB96
#define GREEN         0x07E0
#define LIME          0xAFE5
#define YELLOW        0xFFE0
#define ORANGE        0xFD20

static const char *TAG = "watch_ui";

typedef struct {
    char character;
    uint8_t row[7];
} glyph_t;

/* Labels use a compact neutral 5x7 font; the primary time face uses geometric
 * seven-segment numerals, so the UI does not rely on a pixel-art look. */
static const glyph_t GLYPHS[] = {
    {' ', {0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
    {':', {0x00,0x04,0x04,0x00,0x04,0x04,0x00}},
    {'0', {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}},
    {'1', {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}},
    {'2', {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}},
    {'3', {0x1E,0x01,0x02,0x0E,0x01,0x01,0x1E}},
    {'4', {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}},
    {'5', {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}},
    {'6', {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}},
    {'7', {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}},
    {'8', {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}},
    {'9', {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}},
    {'A', {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}},
    {'B', {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}},
    {'C', {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}},
    {'D', {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}},
    {'E', {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}},
    {'F', {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}},
    {'G', {0x0E,0x11,0x10,0x17,0x11,0x11,0x0E}},
    {'H', {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}},
    {'I', {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}},
    {'J', {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}},
    {'K', {0x11,0x12,0x14,0x18,0x14,0x12,0x11}},
    {'L', {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}},
    {'M', {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}},
    {'N', {0x11,0x19,0x15,0x13,0x11,0x11,0x11}},
    {'O', {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}},
    {'P', {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}},
    {'Q', {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}},
    {'R', {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}},
    {'S', {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}},
    {'T', {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}},
    {'U', {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}},
    {'V', {0x11,0x11,0x11,0x11,0x0A,0x0A,0x04}},
    {'W', {0x11,0x11,0x11,0x15,0x15,0x1B,0x11}},
    {'X', {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}},
    {'Y', {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}},
    {'Z', {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}},
};

static const int16_t SIN32[32] = {
       0, 195, 383, 556, 707, 831, 924, 981,
    1000, 981, 924, 831, 707, 556, 383, 195,
       0,-195,-383,-556,-707,-831,-924,-981,
   -1000,-981,-924,-831,-707,-556,-383,-195,
};

static const uint8_t *glyph_for(char character)
{
    for (size_t index = 0; index < sizeof(GLYPHS) / sizeof(GLYPHS[0]); ++index) {
        if (GLYPHS[index].character == character) {
            return GLYPHS[index].row;
        }
    }
    return GLYPHS[0].row;
}

static void fill_rect(lcd_st7789_t *lcd, int x, int y, int width, int height, uint16_t color)
{
    lcd_st7789_fill_rect(lcd, x, y, width, height, color);
}

static void pixel(lcd_st7789_t *lcd, int x, int y, uint16_t color)
{
    fill_rect(lcd, x, y, 1, 1, color);
}

static void draw_text(lcd_st7789_t *lcd, int x, int y, const char *text, int scale, uint16_t color)
{
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        const uint8_t *glyph = glyph_for(*cursor);
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if (glyph[row] & (1U << (4 - column))) {
                    fill_rect(lcd, x + column * scale, y + row * scale, scale, scale, color);
                }
            }
        }
        x += *cursor == ' ' ? 3 * scale : 6 * scale;
    }
}

static int label_width(const char *text, int scale)
{
    int width = 0;
    for (const char *cursor = text; *cursor != '\0'; ++cursor) {
        width += *cursor == ' ' ? 3 * scale : 6 * scale;
    }
    return width;
}

static void draw_text_centered(lcd_st7789_t *lcd, int y, const char *text, int scale, uint16_t color)
{
    draw_text(lcd, (LCD_WIDTH - label_width(text, scale)) / 2, y, text, scale, color);
}

static void draw_circle_outline(lcd_st7789_t *lcd, int center_x, int center_y, int radius, int thickness, uint16_t color)
{
    for (int ring = 0; ring < thickness; ++ring) {
        int x = radius - ring;
        int y = 0;
        int error = 1 - x;
        while (x >= y) {
            pixel(lcd, center_x + x, center_y + y, color);
            pixel(lcd, center_x + y, center_y + x, color);
            pixel(lcd, center_x - y, center_y + x, color);
            pixel(lcd, center_x - x, center_y + y, color);
            pixel(lcd, center_x - x, center_y - y, color);
            pixel(lcd, center_x - y, center_y - x, color);
            pixel(lcd, center_x + y, center_y - x, color);
            pixel(lcd, center_x + x, center_y - y, color);
            ++y;
            if (error < 0) {
                error += 2 * y + 1;
            } else {
                --x;
                error += 2 * (y - x) + 1;
            }
        }
    }
}

static void draw_filled_circle(lcd_st7789_t *lcd, int center_x, int center_y, int radius, uint16_t color)
{
    for (int y = -radius; y <= radius; ++y) {
        const int y2 = y * y;
        int x = 0;
        while ((x + 1) * (x + 1) + y2 <= radius * radius) {
            ++x;
        }
        fill_rect(lcd, center_x - x, center_y + y, 2 * x + 1, 1, color);
    }
}

static void draw_line(lcd_st7789_t *lcd, int x0, int y0, int x1, int y1, uint16_t color)
{
    int dx = x0 < x1 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y0 < y1 ? y1 - y0 : y0 - y1;
    int sy = y0 < y1 ? 1 : -1;
    int error = dx - dy;

    while (true) {
        pixel(lcd, x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            return;
        }
        const int twice_error = 2 * error;
        if (twice_error > -dy) {
            error -= dy;
            x0 += sx;
        }
        if (twice_error < dx) {
            error += dx;
            y0 += sy;
        }
    }
}

static void draw_round_card(lcd_st7789_t *lcd, int x, int y, int width, int height, int radius, uint16_t fill, uint16_t border)
{
    fill_rect(lcd, x + radius, y, width - 2 * radius, height, fill);
    fill_rect(lcd, x, y + radius, width, height - 2 * radius, fill);
    draw_filled_circle(lcd, x + radius, y + radius, radius, fill);
    draw_filled_circle(lcd, x + width - radius - 1, y + radius, radius, fill);
    draw_filled_circle(lcd, x + radius, y + height - radius - 1, radius, fill);
    draw_filled_circle(lcd, x + width - radius - 1, y + height - radius - 1, radius, fill);
    fill_rect(lcd, x + radius, y, width - 2 * radius, 1, border);
    fill_rect(lcd, x + radius, y + height - 1, width - 2 * radius, 1, border);
    fill_rect(lcd, x, y + radius, 1, height - 2 * radius, border);
    fill_rect(lcd, x + width - 1, y + radius, 1, height - 2 * radius, border);
}

static void draw_segment_h(lcd_st7789_t *lcd, int x, int y, int length, int thickness, uint16_t color)
{
    fill_rect(lcd, x + thickness, y, length - 2 * thickness, thickness, color);
    fill_rect(lcd, x, y + 1, thickness, thickness - 1, color);
    fill_rect(lcd, x + length - thickness, y + 1, thickness, thickness - 1, color);
}

static void draw_segment_v(lcd_st7789_t *lcd, int x, int y, int length, int thickness, uint16_t color)
{
    fill_rect(lcd, x, y + thickness, thickness, length - 2 * thickness, color);
    fill_rect(lcd, x + 1, y, thickness - 1, thickness, color);
    fill_rect(lcd, x + 1, y + length - thickness, thickness - 1, thickness, color);
}

static void draw_digit(lcd_st7789_t *lcd, int x, int y, int number, uint16_t color)
{
    const uint8_t masks[] = {0x3F,0x06,0x5B,0x4F,0x66,0x6D,0x7D,0x07,0x7F,0x6F};
    const uint8_t mask = masks[number % 10];
    const int width = 28;
    const int half = 23;
    const int thick = 4;
    if (mask & 0x01) draw_segment_h(lcd, x, y, width, thick, color);
    if (mask & 0x02) draw_segment_v(lcd, x + width - thick, y, half, thick, color);
    if (mask & 0x04) draw_segment_v(lcd, x + width - thick, y + half - thick, half, thick, color);
    if (mask & 0x08) draw_segment_h(lcd, x, y + 2 * half - thick, width, thick, color);
    if (mask & 0x10) draw_segment_v(lcd, x, y + half - thick, half, thick, color);
    if (mask & 0x20) draw_segment_v(lcd, x, y, half, thick, color);
    if (mask & 0x40) draw_segment_h(lcd, x, y + half - (thick / 2), width, thick, color);
}

static void draw_time(lcd_st7789_t *lcd)
{
    draw_digit(lcd, 51, 71, 1, WHITE);
    draw_digit(lcd, 84, 71, 0, WHITE);
    draw_filled_circle(lcd, 120, 85, 3, CYAN);
    draw_filled_circle(lcd, 120, 110, 3, CYAN);
    draw_digit(lcd, 137, 71, 0, WHITE);
    draw_digit(lcd, 170, 71, 8, WHITE);
}

static void restore_from_psram(lcd_st7789_t *lcd, const uint16_t *background, int x, int y, int width, int height)
{
    const int x0 = x < 0 ? 0 : x;
    const int y0 = y < 0 ? 0 : y;
    const int x1 = x + width > LCD_WIDTH ? LCD_WIDTH : x + width;
    const int y1 = y + height > LCD_HEIGHT ? LCD_HEIGHT : y + height;
    if (x0 >= x1 || y0 >= y1) {
        return;
    }
    const size_t row_bytes = (size_t)(x1 - x0) * sizeof(uint16_t);
    for (int row = y0; row < y1; ++row) {
        memcpy(&lcd->framebuffer[row * LCD_WIDTH + x0], &background[row * LCD_WIDTH + x0], row_bytes);
    }
}

static void draw_page_dots(lcd_st7789_t *lcd, int page)
{
    const uint16_t colors[] = {CYAN, MAGENTA, LIME};
    for (int index = 0; index < 3; ++index) {
        const uint16_t color = index == page ? colors[index] : GRID;
        draw_filled_circle(lcd, 126 + index * 14, 221, index == page ? 4 : 2, color);
    }
}

static void draw_face_static(lcd_st7789_t *lcd)
{
    lcd_st7789_clear(lcd, NAVY);
    draw_circle_outline(lcd, 140, 120, 105, 2, GRID);
    draw_circle_outline(lcd, 140, 120, 95, 1, PANEL);
    for (int tick = 0; tick < 12; ++tick) {
        const int cosine = SIN32[(tick * 32 / 12 + 8) & 31];
        const int sine = SIN32[(tick * 32 / 12) & 31];
        const int x = 140 + cosine * 84 / 1000;
        const int y = 120 + sine * 84 / 1000;
        draw_filled_circle(lcd, x, y, tick % 3 == 0 ? 2 : 1, tick % 3 == 0 ? CYAN : GRID);
    }
    draw_text_centered(lcd, 43, "NOVA WATCH", 2, CYAN);
    draw_time(lcd);
    draw_text_centered(lcd, 137, "TUE 12", 2, WHITE);
    draw_round_card(lcd, 62, 157, 156, 28, 10, PANEL, GRID);
    draw_text(lcd, 78, 166, "ACTIVE", 2, LIME);
    draw_text(lcd, 163, 166, "68", 2, WHITE);
    draw_page_dots(lcd, 0);
}

static void draw_health_static(lcd_st7789_t *lcd)
{
    lcd_st7789_clear(lcd, NAVY);
    draw_text_centered(lcd, 30, "HEALTH", 2, CYAN);
    draw_circle_outline(lcd, 140, 111, 69, 6, PANEL);
    draw_circle_outline(lcd, 140, 111, 53, 5, GRID);
    draw_circle_outline(lcd, 140, 111, 37, 4, PANEL);
    draw_text_centered(lcd, 94, "078", 3, WHITE);
    draw_text_centered(lcd, 121, "BPM", 2, PINK);
    draw_round_card(lcd, 31, 170, 218, 30, 10, PANEL, GRID);
    draw_text(lcd, 48, 179, "MOVE", 2, LIME);
    draw_text(lcd, 170, 179, "6420", 2, WHITE);
    draw_page_dots(lcd, 1);
}

static void draw_music_static(lcd_st7789_t *lcd)
{
    lcd_st7789_clear(lcd, NAVY);
    draw_text_centered(lcd, 31, "MUSIC", 2, CYAN);
    draw_round_card(lcd, 43, 60, 194, 128, 18, PANEL, GRID);
    draw_circle_outline(lcd, 140, 109, 47, 4, MAGENTA);
    draw_circle_outline(lcd, 140, 109, 37, 1, PURPLE);
    draw_filled_circle(lcd, 140, 109, 9, WHITE);
    draw_text_centered(lcd, 158, "NOVA MIX", 2, WHITE);
    draw_page_dots(lcd, 2);
}

static void build_page(lcd_st7789_t *lcd, uint16_t *background, int page)
{
    if (page == 0) {
        draw_face_static(lcd);
    } else if (page == 1) {
        draw_health_static(lcd);
    } else {
        draw_music_static(lcd);
    }
    memcpy(background, lcd->framebuffer, FRAME_BYTES);
    ESP_ERROR_CHECK(lcd_st7789_flush(lcd));
}

static void animate_face(lcd_st7789_t *lcd, const uint16_t *background, uint32_t frame)
{
    const int old_index = (int)((frame + 31) & 31);
    const int new_index = (int)(frame & 31);
    const int old_x = 140 + SIN32[(old_index + 8) & 31] * 75 / 1000;
    const int old_y = 120 + SIN32[old_index] * 75 / 1000;
    const int new_x = 140 + SIN32[(new_index + 8) & 31] * 75 / 1000;
    const int new_y = 120 + SIN32[new_index] * 75 / 1000;
    restore_from_psram(lcd, background, old_x - 7, old_y - 7, 15, 15);
    restore_from_psram(lcd, background, new_x - 7, new_y - 7, 15, 15);
    draw_filled_circle(lcd, new_x, new_y, 5, CYAN);
    draw_filled_circle(lcd, new_x, new_y, 2, WHITE);
    ESP_ERROR_CHECK(lcd_st7789_flush_area(lcd, old_x - 7, old_y - 7, 15, 15));
    ESP_ERROR_CHECK(lcd_st7789_flush_area(lcd, new_x - 7, new_y - 7, 15, 15));
}

static void animate_health(lcd_st7789_t *lcd, const uint16_t *background, uint32_t frame)
{
    const int old_index = (int)(((frame + 30) / 2) & 31);
    const int new_index = (int)((frame / 2) & 31);
    const int old_x = 140 + SIN32[(old_index + 8) & 31] * 69 / 1000;
    const int old_y = 111 + SIN32[old_index] * 69 / 1000;
    const int new_x = 140 + SIN32[(new_index + 8) & 31] * 69 / 1000;
    const int new_y = 111 + SIN32[new_index] * 69 / 1000;
    restore_from_psram(lcd, background, old_x - 6, old_y - 6, 13, 13);
    restore_from_psram(lcd, background, new_x - 6, new_y - 6, 13, 13);
    draw_filled_circle(lcd, new_x, new_y, 4, PINK);
    ESP_ERROR_CHECK(lcd_st7789_flush_area(lcd, old_x - 6, old_y - 6, 13, 13));
    ESP_ERROR_CHECK(lcd_st7789_flush_area(lcd, new_x - 6, new_y - 6, 13, 13));
}

static void animate_music(lcd_st7789_t *lcd, const uint16_t *background, uint32_t frame)
{
    const int x = 69;
    const int y = 72;
    const int width = 142;
    const int height = 63;
    restore_from_psram(lcd, background, x, y, width, height);
    for (int bar = 0; bar < 9; ++bar) {
        const int amplitude = 11 + (int)((frame * (bar + 3) + bar * 17) % 36);
        const uint16_t color = bar % 3 == 0 ? CYAN : (bar % 3 == 1 ? MAGENTA : LIME);
        fill_rect(lcd, x + 9 + bar * 14, y + 50 - amplitude, 7, amplitude, color);
    }
    ESP_ERROR_CHECK(lcd_st7789_flush_area(lcd, x, y, width, height));
}

void app_main(void)
{
    lcd_st7789_t lcd;
    ESP_ERROR_CHECK(lcd_st7789_init(&lcd));

    uint16_t *background = heap_caps_malloc(FRAME_BYTES, MALLOC_CAP_SPIRAM);
    ESP_ERROR_CHECK(background == NULL ? ESP_ERR_NO_MEM : ESP_OK);
    ESP_LOGI(TAG, "PSRAM online: %u bytes, free external heap: %u bytes",
             (unsigned)esp_psram_get_size(), (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    gpio_config_t button_config = {
        .pin_bit_mask = 1ULL << SW3_GPIO,
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
    build_page(&lcd, background, page);

    while (true) {
        const bool pressed = gpio_get_level(SW3_GPIO) == 0;
        const int64_t now_us = esp_timer_get_time();
        if (pressed && !was_pressed && now_us - last_press_us > BUTTON_DEBOUNCE_MS * 1000LL) {
            page = (page + 1) % 3;
            last_press_us = now_us;
            frame = 0;
            build_page(&lcd, background, page);
            ESP_LOGI(TAG, "SW3 switched to watch page %d", page);
        }
        was_pressed = pressed;

        if (page == 0) {
            animate_face(&lcd, background, frame);
        } else if (page == 1) {
            animate_health(&lcd, background, frame);
        } else {
            animate_music(&lcd, background, frame);
        }
        ++frame;
        vTaskDelay(pdMS_TO_TICKS(FRAME_INTERVAL_MS));
    }
}
