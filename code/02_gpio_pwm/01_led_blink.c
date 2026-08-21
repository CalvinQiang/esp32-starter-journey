/**
 * 🌟 ESP32 物联网实战 —— 第 02 关 实验 1：GPIO 数字输出基础闪烁 (Blink)
 *    硬件连接: 板载蓝色 LED2 -> GPIO27 (高电平点亮)
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "EXP1_LED_BLINK";

#define LED_PIN GPIO_NUM_27

void app_main(void)
{
    // 1. 引脚重置并配置为输出模式
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);

    ESP_LOGI(TAG, "💡 LED2 GPIO 输出已就绪，开始周期闪烁...");

    int count = 0;
    while (1) {
        count++;
        // 点亮 LED
        gpio_set_level(LED_PIN, 1);
        ESP_LOGI(TAG, "[#%d] 💡 LED 开灯 (GPIO27 = High)", count);
        vTaskDelay(pdMS_TO_TICKS(500));

        // 熄灭 LED
        gpio_set_level(LED_PIN, 0);
        ESP_LOGI(TAG, "[#%d] 🌑 LED 关灯 (GPIO27 = Low)", count);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
