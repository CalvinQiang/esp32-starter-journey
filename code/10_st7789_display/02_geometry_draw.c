/**
 * 🌟 ESP32 物联网实战 —— 第 10 关 实验 2：基本几何图形渲染 (点、线、矩形、圆与色彩卡片)
 * 
 * 🎯 学习目标：
 *    1. 理解嵌入式 2D 绘图引擎的基本实现原理（像素点映射、Bresenham 画线算法）；
 *    2. 掌握局部显存窗口刷新（`esp_lcd_panel_draw_bitmap` 区域重绘）；
 *    3. 绘制带有科技感的卡片 UI、同心圆靶心与多彩渐变网格。
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_heap_caps.h"

static const char *TAG = "EXP2_GEOMETRY";

#define LCD_HOST                SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ      (40 * 1000 * 1000)
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

// 常用 RGB565 色彩定义
#define COLOR_BLACK             0x0000
#define COLOR_WHITE             0xFFFF
#define COLOR_RED               0x00F8
#define COLOR_GREEN             0xE007
#define COLOR_BLUE              0x1F00
#define COLOR_YELLOW            0xE0FF
#define COLOR_CYAN              0xFF07
#define COLOR_DARK_GRAY         0x1021
#define COLOR_NAVY              0x0F00

static esp_lcd_panel_handle_t s_panel = NULL;

static void lcd_init(void)
{
    gpio_config_t bl = { .pin_bit_mask = (1ULL << LCD_PIN_BACKLIGHT), .mode = GPIO_MODE_OUTPUT };
    gpio_config(&bl);
    gpio_set_level(LCD_PIN_BACKLIGHT, 1);

    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_PIN_SCLK,
        .mosi_io_num = LCD_PIN_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io));

    esp_lcd_panel_dev_config_t p_cfg = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io, &p_cfg, &s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(s_panel, LCD_GAP_X, LCD_GAP_Y));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));
}

/* 填充矩形区域 */
static void fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (x >= LCD_H_RES || y >= LCD_V_RES || w <= 0 || h <= 0) return;
    if (x + w > LCD_H_RES) w = LCD_H_RES - x;
    if (y + h > LCD_V_RES) h = LCD_V_RES - y;

    int total_pixels = w * h;
    uint16_t *buf = heap_caps_malloc(total_pixels * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!buf) return;

    for (int i = 0; i < total_pixels; i++) {
        buf[i] = color;
    }
    esp_lcd_panel_draw_bitmap(s_panel, x, y, x + w, y + h, buf);
    free(buf);
}

/* 清屏 */
static void clear_screen(uint16_t color)
{
    fill_rect(0, 0, LCD_H_RES, LCD_V_RES, color);
}

/* 绘制实心圆 (Midpoint Circle 算法) */
static void fill_circle(int xc, int yc, int r, uint16_t color)
{
    for (int y = -r; y <= r; y++) {
        int w = (int)sqrt(r * r - y * y);
        fill_rect(xc - w, yc + y, 2 * w, 1, color);
    }
}

/* 绘制精美科技感仪表演示界面 */
static void render_geometry_demo(void)
{
    // 1. 深色背景
    clear_screen(COLOR_NAVY);

    // 2. 顶部标题栏卡片
    fill_rect(10, 10, 220, 40, COLOR_DARK_GRAY);
    fill_rect(15, 15, 8, 30, COLOR_CYAN); // 装饰光条

    // 3. 中部传感器数值卡片
    fill_rect(10, 60, 105, 90, COLOR_DARK_GRAY);
    fill_rect(125, 60, 105, 90, COLOR_DARK_GRAY);

    // 4. 卡片内圆形状态指示灯
    fill_circle(62, 105, 20, COLOR_GREEN);
    fill_circle(177, 105, 20, COLOR_YELLOW);

    // 5. 底部波形显示区域与同心圆雷达
    fill_rect(10, 160, 220, 110, COLOR_DARK_GRAY);
    fill_circle(120, 215, 45, COLOR_BLACK);
    fill_circle(120, 215, 30, COLOR_NAVY);
    fill_circle(120, 215, 15, COLOR_CYAN);
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 10 实验 2：ST7789 几何图形与卡片渲染   ");
    ESP_LOGI(TAG, "==================================================");

    lcd_init();
    render_geometry_demo();
    ESP_LOGI(TAG, "✅ 几何图形与多卡片 UI 渲染完毕！");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
