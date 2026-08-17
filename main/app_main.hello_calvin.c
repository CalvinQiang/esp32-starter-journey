#include <stddef.h>
#include <stdint.h>

#include "esp_check.h"
#include "esp_log.h"

#include "lcd_st7789.h"

#define COLOR_BLACK  0x0000
#define COLOR_WHITE  0xFFFF
#define COLOR_GREEN  0x07E0
#define COLOR_BLUE   0x001F

static const char *TAG = "lcd_hello";

/* Each row uses the low five bits. Glyphs are drawn directly in normal
 * left-to-right, top-to-bottom logical coordinates: no rotation or mirroring. */
static const uint8_t GLYPH_SPACE[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t GLYPH_H[7]     = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
static const uint8_t GLYPH_C[7]     = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
static const uint8_t GLYPH_a[7]     = {0x00, 0x0E, 0x01, 0x0F, 0x11, 0x0F, 0x00};
static const uint8_t GLYPH_e[7]     = {0x00, 0x0E, 0x11, 0x1F, 0x10, 0x0F, 0x00};
static const uint8_t GLYPH_i[7]     = {0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x0E};
static const uint8_t GLYPH_l[7]     = {0x0C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
static const uint8_t GLYPH_n[7]     = {0x00, 0x16, 0x19, 0x11, 0x11, 0x11, 0x11};
static const uint8_t GLYPH_o[7]     = {0x00, 0x0E, 0x11, 0x11, 0x11, 0x11, 0x0E};
static const uint8_t GLYPH_v[7]     = {0x00, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04};

static const uint8_t *glyph_for(char character)
{
    switch (character) {
    case 'H': return GLYPH_H;
    case 'C': return GLYPH_C;
    case 'a': return GLYPH_a;
    case 'e': return GLYPH_e;
    case 'i': return GLYPH_i;
    case 'l': return GLYPH_l;
    case 'n': return GLYPH_n;
    case 'o': return GLYPH_o;
    case 'v': return GLYPH_v;
    case ' ': return GLYPH_SPACE;
    default:  return GLYPH_SPACE;
    }
}

static int glyph_advance(char character, int scale)
{
    return character == ' ' ? (2 * scale) : (5 * scale + scale);
}

static void draw_glyph(lcd_st7789_t *lcd, int x, int y, char character, int scale, uint16_t color)
{
    const uint8_t *glyph = glyph_for(character);
    for (int row = 0; row < 7; ++row) {
        for (int column = 0; column < 5; ++column) {
            if ((glyph[row] & (1U << (4 - column))) != 0) {
                lcd_st7789_fill_rect(lcd, x + column * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

static void draw_text_centered(lcd_st7789_t *lcd, const char *text, int y, int scale, uint16_t color)
{
    int text_width = 0;
    for (const char *character = text; *character != '\0'; ++character) {
        text_width += glyph_advance(*character, scale);
    }

    int x = (LCD_WIDTH - text_width) / 2;
    for (const char *character = text; *character != '\0'; ++character) {
        draw_glyph(lcd, x, y, *character, scale, color);
        x += glyph_advance(*character, scale);
    }
}

void app_main(void)
{
    lcd_st7789_t lcd;
    ESP_ERROR_CHECK(lcd_st7789_init(&lcd));

    lcd_st7789_clear(&lcd, COLOR_BLACK);
    lcd_st7789_fill_rect(&lcd, 42, 76, LCD_WIDTH - 84, 4, COLOR_GREEN);
    draw_text_centered(&lcd, "Hello Calvin", 106, 4, COLOR_WHITE);
    lcd_st7789_fill_rect(&lcd, 42, 160, LCD_WIDTH - 84, 4, COLOR_BLUE);

    ESP_ERROR_CHECK(lcd_st7789_flush(&lcd));
    ESP_LOGI(TAG, "Hello Calvin drawn with normal 5x7 glyph coordinates");
}
