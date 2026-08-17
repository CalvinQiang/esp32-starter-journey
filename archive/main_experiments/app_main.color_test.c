#include "esp_check.h"
#include "esp_log.h"

#include "lcd_st7789.h"

#define COLOR_BLACK   0x0000
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_WHITE   0xFFFF
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF

static const char *TAG = "lcd_test";

static void draw_orientation_test(lcd_st7789_t *lcd)
{
    /* Normal application coordinates only. The LCD driver alone maps this
     * landscape canvas to the ST7789's MV=1 address/write order. */
    lcd_st7789_clear(lcd, COLOR_BLACK);

    /* logical corners: red=top-left, green=top-right,
     * blue=bottom-left, white=bottom-right */
    lcd_st7789_fill_rect(lcd, 0, 0, 28, 28, COLOR_RED);
    lcd_st7789_fill_rect(lcd, LCD_WIDTH - 28, 0, 28, 28, COLOR_GREEN);
    lcd_st7789_fill_rect(lcd, 0, LCD_HEIGHT - 28, 28, 28, COLOR_BLUE);
    lcd_st7789_fill_rect(lcd, LCD_WIDTH - 28, LCD_HEIGHT - 28, 28, 28, COLOR_WHITE);

    /* logical top edge and logical left edge */
    lcd_st7789_fill_rect(lcd, 70, 16, 140, 8, COLOR_YELLOW);
    lcd_st7789_fill_rect(lcd, 16, 50, 8, 140, COLOR_CYAN);
    lcd_st7789_fill_rect(lcd, (LCD_WIDTH / 2) - 8, (LCD_HEIGHT / 2) - 8, 16, 16, COLOR_WHITE);

    ESP_ERROR_CHECK(lcd_st7789_flush(lcd));
    ESP_LOGI(TAG, "normal landscape test page drawn: %dx%d", LCD_WIDTH, LCD_HEIGHT);
}

void app_main(void)
{
    lcd_st7789_t lcd;
    ESP_ERROR_CHECK(lcd_st7789_init(&lcd));
    draw_orientation_test(&lcd);
}
