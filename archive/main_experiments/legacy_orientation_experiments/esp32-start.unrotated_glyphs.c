#include <stdint.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"

#define LCD_HOST              SPI2_HOST
#define LCD_H_RES             280
#define LCD_V_RES             240
#define LCD_PIXEL_CLOCK_HZ    (40 * 1000 * 1000)

#define LCD_PIN_MOSI          19
#define LCD_PIN_SCLK          18
#define LCD_PIN_CS            5
#define LCD_PIN_DC            17
#define LCD_PIN_RST           21
#define LCD_PIN_BACKLIGHT     26

#define COLOR_BLACK           0x0000
#define COLOR_WHITE           0xFFFF
#define COLOR_ACCENT          0x07E0

static const char *TAG = "lcd_hello";

/* 5x7 bitmap glyphs; each byte uses the five least significant bits. */
static const uint8_t glyph_space[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t glyph_H[7]     = {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
static const uint8_t glyph_C[7]     = {0x0E, 0x11, 0x01, 0x01, 0x01, 0x11, 0x0E};
static const uint8_t glyph_a[7]     = {0x00, 0x00, 0x0E, 0x10, 0x1E, 0x11, 0x1E};
static const uint8_t glyph_e[7]     = {0x00, 0x00, 0x0E, 0x11, 0x1F, 0x01, 0x1E};
static const uint8_t glyph_i[7]     = {0x04, 0x00, 0x06, 0x04, 0x04, 0x04, 0x0E};
static const uint8_t glyph_l[7]     = {0x06, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
static const uint8_t glyph_n[7]     = {0x00, 0x00, 0x1B, 0x15, 0x11, 0x11, 0x11};
static const uint8_t glyph_o[7]     = {0x00, 0x00, 0x0E, 0x11, 0x11, 0x11, 0x0E};
static const uint8_t glyph_v[7]     = {0x00, 0x00, 0x11, 0x11, 0x11, 0x0A, 0x04};

static const uint8_t *glyph_for(char character)
{
    switch (character) {
        case 'H': return glyph_H;
        case 'C': return glyph_C;
        case 'a': return glyph_a;
        case 'e': return glyph_e;
        case 'i': return glyph_i;
        case 'l': return glyph_l;
        case 'n': return glyph_n;
        case 'o': return glyph_o;
        case 'v': return glyph_v;
        case ' ': return glyph_space;
        default:  return glyph_space;
    }
}

static void fill_screen(uint16_t *framebuffer, uint16_t color)
{
    for (size_t pixel = 0; pixel < (LCD_H_RES * LCD_V_RES); ++pixel) {
        framebuffer[pixel] = color;
    }
}

static void draw_rect(uint16_t *framebuffer, int x, int y, int width, int height, uint16_t color)
{
    const int x_start = x < 0 ? 0 : x;
    const int y_start = y < 0 ? 0 : y;
    const int x_end = (x + width) > LCD_H_RES ? LCD_H_RES : (x + width);
    const int y_end = (y + height) > LCD_V_RES ? LCD_V_RES : (y + height);

    for (int row = y_start; row < y_end; ++row) {
        for (int column = x_start; column < x_end; ++column) {
            /* The panel's landscape scan direction mirrors the image horizontally.
             * Reflect every plotted pixel in the framebuffer to compensate for it. */
            framebuffer[row * LCD_H_RES + (LCD_H_RES - 1 - column)] = color;
        }
    }
}

static void draw_character(uint16_t *framebuffer, int x, int y, char character, int scale, uint16_t color)
{
    const uint8_t *glyph = glyph_for(character);
    for (int row = 0; row < 7; ++row) {
        for (int column = 0; column < 5; ++column) {
            if ((glyph[row] >> (4 - column)) & 0x01) {
                draw_rect(framebuffer, x + column * scale, y + row * scale, scale, scale, color);
            }
        }
    }
}

static void draw_text_centered(uint16_t *framebuffer, const char *text, int y, int scale, uint16_t color)
{
    int characters = 0;
    while (text[characters] != '\0') {
        ++characters;
    }

    const int character_width = 5 * scale;
    const int spacing = scale;
    const int text_width = characters * character_width + (characters - 1) * spacing;
    int x = (LCD_H_RES - text_width) / 2;

    for (int index = 0; text[index] != '\0'; ++index) {
        draw_character(framebuffer, x, y, text[index], scale, color);
        x += character_width + spacing;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Initializing ST7789 LCD");

    gpio_config_t backlight_config = {
        .pin_bit_mask = 1ULL << LCD_PIN_BACKLIGHT,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&backlight_config));
    ESP_ERROR_CHECK(gpio_set_level(LCD_PIN_BACKLIGHT, 1));

    spi_bus_config_t bus_config = {
        .sclk_io_num = LCD_PIN_SCLK,
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = LCD_H_RES * 40 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, true));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 20, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    uint16_t *framebuffer = heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (framebuffer == NULL) {
        ESP_LOGE(TAG, "Unable to allocate LCD framebuffer");
        return;
    }

    fill_screen(framebuffer, COLOR_BLACK);
    draw_rect(framebuffer, 32, 76, 216, 3, COLOR_ACCENT);
    draw_text_centered(framebuffer, "Hello Calvin", 109, 3, COLOR_WHITE);
    draw_rect(framebuffer, 32, 161, 216, 3, COLOR_ACCENT);

    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, LCD_H_RES, LCD_V_RES, framebuffer));
    ESP_LOGI(TAG, "Hello Calvin is displayed");
}
