/**
 * 🌟 ESP32 物联网实战 —— 第 06 关 实验 1：RMT 硬件脉冲驱动 WS2812 幻彩 RGB (彩虹流光)
 *    硬件连接: WS2812 (JP3) -> GPIO26 (注意：必须拔下 JP7 背光跳线帽！)
 *    技术亮点: RMT 硬件脉冲发射、HSV 360° 色相转 RGB 算法、50FPS 丝滑彩虹流光
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led_strip.h"

#define WS2812_GPIO_PIN   26
#define WS2812_NUM_LEDS   12

// 色彩翻译官：输入角度 hue (0~359)，自动输出对应的红绿蓝 RGB 数值
static void hsv_to_rgb(uint32_t h, uint32_t s, uint32_t v,
                       uint32_t *r, uint32_t *g, uint32_t *b)
{
    if (s == 0) { *r = *g = *b = v; return; }
    h = h % 360;
    uint32_t sector = h / 60;
    uint32_t fract = (h % 60) * 255 / 60;
    uint32_t p = (v * (255 - s)) / 255;
    uint32_t q = (v * (255 - (s * fract) / 255)) / 255;
    uint32_t t = (v * (255 - (s * (255 - fract)) / 255)) / 255;

    switch (sector) {
        case 0: *r = v; *g = t; *b = p; break;
        case 1: *r = q; *g = v; *b = p; break;
        case 2: *r = p; *g = v; *b = t; break;
        case 3: *r = p; *g = q; *b = v; break;
        case 4: *r = t; *g = p; *b = v; break;
        default:*r = v; *g = p; *b = q; break;
    }
}

void app_main(void)
{
    // 1. 初始化 RMT 硬件脉冲驱动
    led_strip_handle_t led_strip;
    led_strip_config_t strip_config = {
        .strip_gpio_num = WS2812_GPIO_PIN,
        .max_leds = WS2812_NUM_LEDS,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

    uint32_t step = 0;
    while (1) {
        // 2. 为 12 颗灯分别计算当前时刻的彩虹颜色
        for (int i = 0; i < WS2812_NUM_LEDS; i++) {
            uint32_t r, g, b;
            // 空间相隔 30° + 时间向前步进 5°
            uint32_t hue = (step * 5 + i * (360 / WS2812_NUM_LEDS)) % 360;
            hsv_to_rgb(hue, 255, 180, &r, &g, &b); // 饱和度 255, 亮度 180 (护眼)
            led_strip_set_pixel(led_strip, i, r, g, b);
        }
        // 3. 触发硬件脉冲，一次性点亮所有灯
        led_strip_refresh(led_strip);

        step++;
        vTaskDelay(pdMS_TO_TICKS(20)); // 每秒刷新 50 帧 (极度丝滑流畅)
    }
}
