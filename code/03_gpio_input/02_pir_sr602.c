/**
 * 🌟 ESP32 物联网实战 —— 第 03 关 实验 2：按键与 SR602 人体红外感应综合监控
 *    硬件连接: SW3 -> GPIO39, SR602 (JP5) -> GPIO34, LED2 -> GPIO27
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "EXP2_PIR_BUTTON";

#define LED_PIN     GPIO_NUM_27  // 板载受控指示灯 LED2 (输出)
#define BUTTON_PIN  GPIO_NUM_39  // 用户按键 SW3 (纯输入)
#define PIR_PIN     GPIO_NUM_34  // SR602 人体红外探头 (纯输入)

static bool g_led_state = false;

static void init_system_gpios(void)
{
    // 1. 初始化 LED2 为输出
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);

    // 2. 初始化 SW3 按键为输入
    gpio_reset_pin(BUTTON_PIN);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);

    // 3. 初始化 SR602 红外为输入
    gpio_reset_pin(PIR_PIN);
    gpio_set_direction(PIR_PIN, GPIO_MODE_INPUT);
}

void app_main(void)
{
    init_system_gpios();
    ESP_LOGI(TAG, "🚀 系统初始化完成，开始监听按键与人体红外事件...");

    int pir_last_state = -1;
    int button_last_level = 1;

    while (1) {
        // --- 模块一：按键扫描与消抖 ---
        int current_btn_level = gpio_get_level(BUTTON_PIN);

        if (button_last_level == 1 && current_btn_level == 0) {
            vTaskDelay(pdMS_TO_TICKS(20)); // 消抖
            if (gpio_get_level(BUTTON_PIN) == 0) {
                g_led_state = !g_led_state;
                gpio_set_level(LED_PIN, g_led_state ? 1 : 0);
                ESP_LOGI(TAG, "🔘 [按键触发] 用户按下了 SW3！当前指示灯: %s", 
                         g_led_state ? "🟢【点亮】" : "⚪【熄灭】");
            }
        }
        button_last_level = current_btn_level;

        // --- 模块二：人体红外状态变化监控 ---
        int current_pir_state = gpio_get_level(PIR_PIN);
        if (current_pir_state != pir_last_state) {
            if (current_pir_state == 1) {
                ESP_LOGW(TAG, "🚶‍♂️ [红外感应] 探测到人体活动！(GPIO34 = 1)");
            } else {
                ESP_LOGI(TAG, "🍃 [红外感应] 人体离开或静止无感应。(GPIO34 = 0)");
            }
            pir_last_state = current_pir_state;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
