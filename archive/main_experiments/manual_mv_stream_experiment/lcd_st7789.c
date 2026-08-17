#include "lcd_st7789.h"

#include <stddef.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"

#define LCD_HOST                SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ      (40 * 1000 * 1000)
#define LCD_PIN_MOSI            19
#define LCD_PIN_SCLK            18
#define LCD_PIN_CS              5
#define LCD_PIN_DC              17
#define LCD_PIN_RST             21
#define LCD_PIN_BACKLIGHT       26

/* The ST7789 DDRAM is 240x320. This board's 240x280 panel is centered in the
 * 320-pixel address dimension, so landscape columns start at address 20. */
#define LCD_LANDSCAPE_X_OFFSET  20
#define LCD_MADCTL_LANDSCAPE    0x20  /* MV=1, MX=0, MY=0, RGB order */

static const char *TAG = "st7789";

static esp_err_t lcd_set_landscape_mapping(lcd_st7789_t *lcd)
{
    /* This is the sole controller-space transform. Application code always
     * draws with a conventional landscape origin at top-left. */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(lcd->panel, true), TAG, "set MV failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(lcd->panel, false, false), TAG, "set MX/MY failed");
    ESP_LOGI(TAG, "MADCTL=0x%02X; logical landscape=%dx%d; DDRAM x offset=%d",
             LCD_MADCTL_LANDSCAPE, LCD_WIDTH, LCD_HEIGHT, LCD_LANDSCAPE_X_OFFSET);
    return ESP_OK;
}

esp_err_t lcd_st7789_init(lcd_st7789_t *lcd)
{
    if (lcd == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *lcd = (lcd_st7789_t) {0};

    gpio_config_t backlight_config = {
        .pin_bit_mask = 1ULL << LCD_PIN_BACKLIGHT,
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&backlight_config), TAG, "configure backlight failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_PIN_BACKLIGHT, 1), TAG, "enable backlight failed");

    spi_bus_config_t bus_config = {
        .sclk_io_num = LCD_PIN_SCLK,
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = LCD_HEIGHT * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO), TAG, "initialize SPI bus failed");

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 1,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &lcd->io), TAG, "create SPI panel IO failed");

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(lcd->io, &panel_config, &lcd->panel), TAG, "create ST7789 panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(lcd->panel), TAG, "reset panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(lcd->panel), TAG, "initialize panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(lcd->panel, true), TAG, "set color inversion failed");
    ESP_RETURN_ON_ERROR(lcd_set_landscape_mapping(lcd), TAG, "set landscape mapping failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(lcd->panel, true), TAG, "turn on panel failed");

    lcd->framebuffer = heap_caps_malloc(LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t), MALLOC_CAP_DMA);
    lcd->transfer_buffer = heap_caps_malloc(LCD_HEIGHT * sizeof(uint16_t), MALLOC_CAP_DMA);
    return (lcd->framebuffer == NULL || lcd->transfer_buffer == NULL) ? ESP_ERR_NO_MEM : ESP_OK;
}

void lcd_st7789_clear(lcd_st7789_t *lcd, uint16_t color)
{
    for (size_t pixel = 0; pixel < LCD_WIDTH * LCD_HEIGHT; ++pixel) {
        lcd->framebuffer[pixel] = color;
    }
}

void lcd_st7789_fill_rect(lcd_st7789_t *lcd, int x, int y, int width, int height, uint16_t color)
{
    if (lcd == NULL || lcd->framebuffer == NULL || width <= 0 || height <= 0) {
        return;
    }

    const int x_start = x < 0 ? 0 : x;
    const int y_start = y < 0 ? 0 : y;
    const int x_end = (x + width) > LCD_WIDTH ? LCD_WIDTH : (x + width);
    const int y_end = (y + height) > LCD_HEIGHT ? LCD_HEIGHT : (y + height);

    for (int row = y_start; row < y_end; ++row) {
        for (int column = x_start; column < x_end; ++column) {
            lcd->framebuffer[row * LCD_WIDTH + column] = color;
        }
    }
}

static esp_err_t lcd_flush_column(lcd_st7789_t *lcd, int logical_x)
{
    const int ddram_x = logical_x + LCD_LANDSCAPE_X_OFFSET;
    const uint8_t column_window[] = {
        (ddram_x >> 8) & 0xFF, ddram_x & 0xFF,
        (ddram_x >> 8) & 0xFF, ddram_x & 0xFF,
    };
    const uint8_t row_window[] = {0x00, 0x00, 0x00, LCD_HEIGHT - 1};

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(lcd->io, LCD_CMD_CASET, column_window, sizeof(column_window)), TAG, "set column window failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(lcd->io, LCD_CMD_RASET, row_window, sizeof(row_window)), TAG, "set row window failed");

    /* With MADCTL.MV=1, ST7789 increments Y for each pixel written. A normal
     * C framebuffer is row-major, so gather one logical column for this one
     * hardware column. This is the only pixel-order transform in the driver. */
    for (int logical_y = 0; logical_y < LCD_HEIGHT; ++logical_y) {
        lcd->transfer_buffer[logical_y] = lcd->framebuffer[logical_y * LCD_WIDTH + logical_x];
    }
    return esp_lcd_panel_io_tx_color(lcd->io, LCD_CMD_RAMWR, lcd->transfer_buffer,
                                     LCD_HEIGHT * sizeof(uint16_t));
}

esp_err_t lcd_st7789_flush(lcd_st7789_t *lcd)
{
    if (lcd == NULL || lcd->io == NULL || lcd->framebuffer == NULL || lcd->transfer_buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int logical_x = 0; logical_x < LCD_WIDTH; ++logical_x) {
        ESP_RETURN_ON_ERROR(lcd_flush_column(lcd, logical_x), TAG, "flush column %d failed", logical_x);
    }
    return ESP_OK;
}
