#include <stdbool.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lcd_st7789.h"

#define USER_BUTTON_GPIO 39

#define COLOR_BLACK   0x0000
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_WHITE   0xFFFF
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF

static const char *TAG = "lcd_test";

static void draw_orientation_test(lcd_st7789_t *lcd, lcd_orientation_t orientation)
{
    /* Every shape uses the same normal logical coordinate system.
     * There are deliberately no per-pixel, framebuffer, or font transforms. */
    lcd_st7789_clear(lcd, COLOR_BLACK);

    /* Logical corners: red=top-left, green=top-right, blue=bottom-left, white=bottom-right. */
    lcd_st7789_fill_rect(lcd, 0, 0, 28, 28, COLOR_RED);
    lcd_st7789_fill_rect(lcd, LCD_WIDTH - 28, 0, 28, 28, COLOR_GREEN);
    lcd_st7789_fill_rect(lcd, 0, LCD_HEIGHT - 28, 28, 28, COLOR_BLUE);
    lcd_st7789_fill_rect(lcd, LCD_WIDTH - 28, LCD_HEIGHT - 28, 28, 28, COLOR_WHITE);

    /* Yellow marks the logical top edge; cyan marks the logical left edge. */
    lcd_st7789_fill_rect(lcd, 70, 16, 140, 8, COLOR_YELLOW);
    lcd_st7789_fill_rect(lcd, 16, 50, 8, 140, COLOR_CYAN);

    /* A fixed centre marker exposes accidental x/y swapping or offsets. */
    lcd_st7789_fill_rect(lcd, (LCD_WIDTH / 2) - 8, (LCD_HEIGHT / 2) - 8, 16, 16, COLOR_WHITE);

    ESP_ERROR_CHECK(lcd_st7789_flush(lcd));
    ESP_LOGI(TAG, "test page: profile=%d, logical size=%dx%d", orientation, LCD_WIDTH, LCD_HEIGHT);
}

void app_main(void)
{
    lcd_st7789_t lcd;
    ESP_ERROR_CHECK(lcd_st7789_init(&lcd));

    gpio_config_t button_config = {
        .pin_bit_mask = 1ULL << USER_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&button_config));

    lcd_orientation_t orientation = LCD_ORIENTATION_LANDSCAPE_0;
    draw_orientation_test(&lcd, orientation);

    bool was_pressed = false;
    while (true) {
        const bool pressed = gpio_get_level(USER_BUTTON_GPIO) == 0;
        if (pressed && !was_pressed) {
            orientation = (orientation + 1) % 4;
            ESP_ERROR_CHECK(lcd_st7789_set_orientation(&lcd, orientation));
            draw_orientation_test(&lcd, orientation);
            vTaskDelay(pdMS_TO_TICKS(80));
        }
        was_pressed = pressed;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
