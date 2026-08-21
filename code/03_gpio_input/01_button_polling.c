/**
 * 🌟 ESP32 物联网实战 —— 第 03 关 实验 1：按键 SW3 轮询扫描与软件消抖
 *    硬件连接: SW3 按键 -> GPIO39 (纯输入，低电平有效), LED2 -> GPIO27
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "EXP1_BTN_POLL";

#define LED_PIN     GPIO_NUM_27
#define BUTTON_PIN  GPIO_NUM_39

void app_main(void)
{
    // 1. 初始化 LED 为输出
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);

    // 2. 初始化 SW3 为输入 (GPIO39 为纯输入管脚，切勿配为输出)
    gpio_reset_pin(BUTTON_PIN);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);

    ESP_LOGI(TAG, "🔘 按键扫描就绪，请按下板载 SW3 按键...");

    bool led_state = false;
    int button_last_level = 1; // 按键松开时为高电平 1

    while (1) {
        int current_btn_level = gpio_get_level(BUTTON_PIN);

        // 边沿检测：从 1 变为 0 时触发
        if (button_last_level == 1 && current_btn_level == 0) {
            // 软件消抖：延时 20ms
            vTaskDelay(pdMS_TO_TICKS(20));

            // 二次确认
            if (gpio_get_level(BUTTON_PIN) == 0) {
                led_state = !led_state;
                gpio_set_level(LED_PIN, led_state ? 1 : 0);
                ESP_LOGI(TAG, "🔘 按键触发！当前 LED2 状态: %s", led_state ? "【点亮】" : "【熄灭】");
            }
        }
        button_last_level = current_btn_level;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
