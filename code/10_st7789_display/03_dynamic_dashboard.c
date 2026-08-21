/**
 * 🌟 ESP32 物联网实战 —— 第 10 关 实验 3：动态示波器波形与高帧率仪表盘 (终极综合)
 * 
 * 🎯 学习目标：
 *    1. 掌握局部显存重绘与防闪烁双缓冲绘制思想；
 *    2. 实时生成平滑正弦波 (Sine Wave) 动态折线示波器；
 *    3. 动态渲染色彩呼吸进度条与 FPS 帧率实时监测。
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_heap_caps.h"

static const char *TAG = "EXP3_DASHBOARD";

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

#define COLOR_BLACK             0x0000
#define COLOR_WHITE             0xFFFF
#define COLOR_RED               0x00F8
#define COLOR_GREEN             0xE007
#define COLOR_BLUE              0x1F00
#define COLOR_YELLOW            0xE0FF
#define COLOR_CYAN              0xFF07
#define COLOR_BG                0x1010
#define COLOR_CARD_BG           0x2018

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

static void fill_rect(int x, int y, int w, int h, uint16_t color)
{
    if (x >= LCD_H_RES || y >= LCD_V_RES || w <= 0 || h <= 0) return;
    if (x + w > LCD_H_RES) w = LCD_H_RES - x;
    if (y + h > LCD_V_RES) h = LCD_V_RES - y;

    int total_pixels = w * h;
    uint16_t *buf = heap_caps_malloc(total_pixels * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!buf) return;

    for (int i = 0; i < total_pixels; i++) buf[i] = color;
    esp_lcd_panel_draw_bitmap(s_panel, x, y, x + w, y + h, buf);
    free(buf);
}

/* 动态波形画布区域 (宽 200, 高 100) */
#define WAVE_W 200
#define WAVE_H 100
#define WAVE_X 20
#define WAVE_Y 140

static void draw_waveform_frame(float phase)
{
    uint16_t *canvas = heap_caps_malloc(WAVE_W * WAVE_H * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!canvas) return;

    // 清空画布背景
    for (int i = 0; i < WAVE_W * WAVE_H; i++) {
        canvas[i] = COLOR_BLACK;
    }

    // 绘制中心虚线网格
    for (int x = 0; x < WAVE_W; x += 20) {
        for (int y = 0; y < WAVE_H; y += 4) {
            canvas[y * WAVE_W + x] = 0x2104; // 暗灰网格
        }
    }
    for (int x = 0; x < WAVE_W; x += 4) {
        canvas[(WAVE_H / 2) * WAVE_W + x] = 0x2104;
    }

    // 计算并绘制平滑动态正弦波 (青色)
    for (int x = 0; x < WAVE_W; x++) {
        float angle = (float)x * 0.08f + phase;
        int y = (int)(sinf(angle) * 35.0f) + (WAVE_H / 2);
        if (y >= 0 && y < WAVE_H) {
            canvas[y * WAVE_W + x] = COLOR_CYAN;
            if (y + 1 < WAVE_H) canvas[(y + 1) * WAVE_W + x] = COLOR_CYAN; // 加粗
        }
    }

    // 局部极速推屏
    esp_lcd_panel_draw_bitmap(s_panel, WAVE_X, WAVE_Y, WAVE_X + WAVE_W, WAVE_Y + WAVE_H, canvas);
    free(canvas);
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 10 实验 3：ST7789 动态波形与高帧率示波器 ");
    ESP_LOGI(TAG, "==================================================");

    lcd_init();

    // 1. 绘制静态深色大背景
    fill_rect(0, 0, LCD_H_RES, LCD_V_RES, COLOR_BG);

    // 2. 绘制顶部信息卡片
    fill_rect(15, 15, 210, 45, COLOR_CARD_BG);
    fill_rect(20, 20, 6, 35, COLOR_GREEN); // 运行状态指示条

    // 3. 绘制进度条边框卡片
    fill_rect(15, 70, 210, 50, COLOR_CARD_BG);

    float phase = 0.0f;
    int progress = 0;
    int frame_count = 0;
    int64_t last_time = esp_timer_get_time();

    while (1) {
        // 刷新动态波形
        draw_waveform_frame(phase);
        phase += 0.15f;

        // 刷新动态进度条 (宽 180, 高 12)
        progress = (progress + 2) % 180;
        fill_rect(30, 95, progress, 12, COLOR_YELLOW);
        fill_rect(30 + progress, 95, 180 - progress, 12, COLOR_BLACK);

        frame_count++;
        int64_t now = esp_timer_get_time();
        if (now - last_time >= 1000000) { // 每秒统计一次 FPS
            float fps = (float)frame_count * 1000000.0f / (float)(now - last_time);
            ESP_LOGI(TAG, "📈 [LCD 渲染性能] 实时刷新帧率: \033[32m%5.1f FPS\033[0m (DMA 硬件加速中)", fps);
            frame_count = 0;
            last_time = now;
        }

        vTaskDelay(pdMS_TO_TICKS(20)); // 约 50 FPS
    }
}
