/**
 * 🌟 ESP32 物联网实战 —— 第 10 关 实验 1：ST7789 彩屏驱动与全屏三原色刷屏 (Hello Screen)
 * 
 * 🎯 学习目标：
 *    1. 掌握 1.69寸 ST7789 SPI LCD 屏幕的硬件引脚与 DMA 显存加速配置；
 *    2. 搞懂 RGB565 色彩格式（16位高低字节）与屏幕物理偏移量（Offset Gap: x=20, y=0）；
 *    3. 实现高帧率全屏纯色刷屏（红、绿、蓝、黄、青、洋红、白）。
 * 
 * 📌 硬件引脚分配：
 *    - SPI SCLK:      GPIO18
 *    - SPI MOSI:      GPIO19
 *    - LCD CS:        GPIO5
 *    - LCD DC:        GPIO17
 *    - LCD RST:       GPIO21
 *    - LCD Backlight: GPIO26 (背光引脚)
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_heap_caps.h"

static const char *TAG = "EXP1_SCREEN_COLORS";

#define LCD_HOST                SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ      (40 * 1000 * 1000) // 40MHz 高速 SPI
#define LCD_PIN_MOSI            GPIO_NUM_19
#define LCD_PIN_SCLK            GPIO_NUM_18
#define LCD_PIN_CS              GPIO_NUM_5
#define LCD_PIN_DC              GPIO_NUM_17
#define LCD_PIN_RST             GPIO_NUM_21
#define LCD_PIN_BACKLIGHT       GPIO_NUM_26

#define LCD_H_RES               240
#define LCD_V_RES               280
#define LCD_GAP_X               20
#define LCD_GAP_Y               0

// RGB565 常用色彩定义 (高低字节小端适配)
#define COLOR_BLACK             0x0000
#define COLOR_WHITE             0xFFFF
#define COLOR_RED               0x00F8  // RGB565 Red in little-endian
#define COLOR_GREEN             0xE007  // RGB565 Green in little-endian
#define COLOR_BLUE              0x1F00  // RGB565 Blue in little-endian
#define COLOR_YELLOW            0xE0FF
#define COLOR_CYAN              0xFF07
#define COLOR_MAGENTA           0x1FF8

static esp_lcd_panel_handle_t s_panel_handle = NULL;
static uint16_t *s_line_buffer = NULL;

static void lcd_backlight_init(void)
{
    gpio_config_t bl_conf = {
        .pin_bit_mask = (1ULL << LCD_PIN_BACKLIGHT),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&bl_conf);
    gpio_set_level(LCD_PIN_BACKLIGHT, 1); // 点亮背光
}

static void lcd_st7789_init(void)
{
    lcd_backlight_init();

    // 1. 初始化 SPI 总线 (开启 DMA 自动通道)
    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_PIN_SCLK,
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = LCD_H_RES * 40 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 2. 挂载 ST7789 Panel IO
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

    // 3. 初始化 ST7789 驱动
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel_handle, true)); // ST7789 需开启反色
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(s_panel_handle, LCD_GAP_X, LCD_GAP_Y)); // 240x280 物理视口偏移
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel_handle, true));

    // 分配一行 DMA 颜色缓冲区
    s_line_buffer = heap_caps_malloc(LCD_H_RES * 20 * sizeof(uint16_t), MALLOC_CAP_DMA);
    ESP_LOGI(TAG, "✅ 1.69寸 ST7789 彩屏初始化成功 (分辨率: 240x280, 速率: 40MHz)");
}

static void fill_screen(uint16_t color)
{
    // 填充 20 行的缓冲区
    for (int i = 0; i < LCD_H_RES * 20; i++) {
        s_line_buffer[i] = color;
    }

    // 分块刷满 280 行
    for (int y = 0; y < LCD_V_RES; y += 20) {
        esp_lcd_panel_draw_bitmap(s_panel_handle, 0, y, LCD_H_RES, y + 20, s_line_buffer);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 10 实验 1：ST7789 彩屏三原色刷屏测试   ");
    ESP_LOGI(TAG, "==================================================");

    lcd_st7789_init();

    uint16_t colors[] = {
        COLOR_RED, COLOR_GREEN, COLOR_BLUE,
        COLOR_YELLOW, COLOR_CYAN, COLOR_MAGENTA, COLOR_WHITE, COLOR_BLACK
    };
    const char *color_names[] = {
        "🔴 鲜艳纯红", "🟢 炫酷纯绿", "🔵 深邃纯蓝",
        "🟡 明亮纯黄", "🔷 科技纯青", "🟣 梦幻洋红", "⚪ 纯净亮白", "⚫ 纯粹极黑"
    };

    int color_idx = 0;
    while (1) {
        ESP_LOGI(TAG, "🎨 正在全屏刷新色彩 ➔ %s", color_names[color_idx]);
        fill_screen(colors[color_idx]);
        color_idx = (color_idx + 1) % 8;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
