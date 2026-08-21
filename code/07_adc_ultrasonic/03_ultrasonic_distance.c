/**
 * 🌟 ESP32 物联网实战 —— 第 07 关 实验 3：HC-SR04 超声波微秒级测距仪
 *    硬件连接: HC-SR04 (JP2) -> Trig (GPIO32), Echo (GPIO33)
 *    技术亮点: 10µs 触发脉冲、esp_timer 微秒级飞行时间捕获、30ms 超时防死锁保护
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

static const char *TAG = "EXP3_ULTRASONIC";

#define TRIG_PIN    GPIO_NUM_32
#define ECHO_PIN    GPIO_NUM_33

static void ultrasonic_init(void)
{
    // 1. 配置 Trig 为输出引脚
    gpio_config_t trig_conf = {
        .pin_bit_mask = (1ULL << TRIG_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&trig_conf);
    gpio_set_level(TRIG_PIN, 0);

    // 2. 配置 Echo 为输入引脚
    gpio_config_t echo_conf = {
        .pin_bit_mask = (1ULL << ECHO_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&echo_conf);
}

static float measure_distance_cm(void)
{
    // 1. 发射 10 微秒的高电平触发脉冲
    gpio_set_level(TRIG_PIN, 0);
    esp_rom_delay_us(2);
    gpio_set_level(TRIG_PIN, 1);
    esp_rom_delay_us(10);
    gpio_set_level(TRIG_PIN, 0);

    // 2. 等待 Echo 变高电平 (带 30ms 超时保护)
    int64_t start_wait = esp_timer_get_time();
    while (gpio_get_level(ECHO_PIN) == 0) {
        if (esp_timer_get_time() - start_wait > 30000) return -1.0f; // 超时
    }

    // 3. 记录高电平开始时刻
    int64_t echo_start = esp_timer_get_time();

    // 4. 等待 Echo 变低电平
    while (gpio_get_level(ECHO_PIN) == 1) {
        if (esp_timer_get_time() - echo_start > 30000) return -1.0f; // 超时
    }
    int64_t echo_end = esp_timer_get_time();

    // 5. 计算持续时间并换算为厘米
    int64_t duration_us = echo_end - echo_start;
    return (float)duration_us / 58.8f;
}

void app_main(void)
{
    ultrasonic_init();
    ESP_LOGI(TAG, "📡 HC-SR04 超声波测距模块已就绪 (Trig: GPIO32, Echo: GPIO33)");

    while (1) {
        float distance = measure_distance_cm();
        if (distance > 0) {
            ESP_LOGI(TAG, "📏 目标距离: \033[36m%6.1f cm\033[0m (%4.2f m)", distance, distance / 100.0f);
        } else {
            ESP_LOGW(TAG, "⚠️ 超出量程或无障碍物 (Out of Range)");
        }
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}
