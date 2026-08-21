/**
 * 🌟 ESP32 物联网实战 —— 第 02 关 实验 2：LEDC 硬件 PWM 呼吸灯
 *    硬件连接: 板载蓝色 LED2 -> GPIO27
 *    技术亮点: 5000Hz 硬件自动跑圈、13 位 (8192 级) 占空比平滑调节、0 CPU 占用
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "EXP2_PWM_BREATHE";

#define LED_PIN GPIO_NUM_27

void app_main(void)
{
    ESP_LOGI(TAG, "🌊 启动 LEDC 硬件 PWM 呼吸灯引擎...");

    // 1. 配置定时器 (5000Hz 频率，13 位 8192 级分辨率)
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .duty_resolution  = LEDC_TIMER_13_BIT,
        .timer_num        = LEDC_TIMER_0,
        .freq_hz          = 5000,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // 2. 配置通道 (绑定定时器与 GPIO27)
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_LOW_SPEED_MODE,
        .channel        = LEDC_CHANNEL_0,
        .timer_sel      = LEDC_TIMER_0,
        .gpio_num       = LED_PIN,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ESP_LOGI(TAG, "✨ 呼吸灯平滑渐变循环启动！");

    while (1) {
        // 吸气：渐亮 (0 -> 8191)
        for (int duty = 0; duty <= 8191; duty += 150) {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
            vTaskDelay(pdMS_TO_TICKS(20));
        }

        // 呼气：渐暗 (8191 -> 0)
        for (int duty = 8191; duty >= 0; duty -= 150) {
            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
}
