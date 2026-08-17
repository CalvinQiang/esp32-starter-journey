#include "lcd_st7789.h"

#include <stddef.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
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

/* These values match the board vendor's 280x240 ST7789 configuration:
 * MADCTL = MX | MV = 0x60 and the 280-pixel visible region begins at x=20
 * inside the controller's 320-pixel address dimension. */
#define LCD_LANDSCAPE_X_GAP     20
#define LCD_LANDSCAPE_Y_GAP     0
#define LCD_MADCTL_LANDSCAPE    0x60

static const char *TAG = "st7789";

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
        .max_transfer_sz = LCD_WIDTH * 40 * sizeof(uint16_t),
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
    esp_lcd_panel_io_handle_t io_handle = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle), TAG, "create SPI panel IO failed");

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(io_handle, &panel_config, &lcd->panel), TAG, "create ST7789 panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(lcd->panel), TAG, "reset panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(lcd->panel), TAG, "initialize panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(lcd->panel, true), TAG, "set color inversion failed");

    /* The only coordinate transform in the driver. The canvas and fonts are
     * never mirrored, rotated, transposed, or otherwise altered. */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(lcd->panel, true), TAG, "set MV failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(lcd->panel, true, false), TAG, "set MX/MY failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(lcd->panel, LCD_LANDSCAPE_X_GAP, LCD_LANDSCAPE_Y_GAP), TAG, "set panel gap failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(lcd->panel, true), TAG, "turn on panel failed");

    lcd->framebuffer = heap_caps_malloc(LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (lcd->framebuffer == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "MADCTL=0x%02X, logical=%dx%d, gap=(%d,%d)",
             LCD_MADCTL_LANDSCAPE, LCD_WIDTH, LCD_HEIGHT,
             LCD_LANDSCAPE_X_GAP, LCD_LANDSCAPE_Y_GAP);
    return ESP_OK;
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

esp_err_t lcd_st7789_flush(lcd_st7789_t *lcd)
{
    if (lcd == NULL || lcd->panel == NULL || lcd->framebuffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return esp_lcd_panel_draw_bitmap(lcd->panel, 0, 0, LCD_WIDTH, LCD_HEIGHT, lcd->framebuffer);
}
