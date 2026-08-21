/**
 * 🌟 ESP32 物联网实战 —— 第 16 关 实验 2：EXT0 / EXT1 外部按键与红外传感器硬件唤醒 (External Wakeup)
 * 
 * 🎯 学习目标：
 *    1. 搞懂 RTC GPIO 概念（只有部分特定 GPIO 属于 RTC 域，支持睡眠时监听唤醒）；
 *    2. 掌握 `EXT0`（单引脚电平唤醒，如按键 SW3）与 `EXT1`（多引脚位掩码唤醒，如人体红外）；
 *    3. 理解芯片在极低功耗下如何像“门铃”一样被外部物理事件瞬间叫醒。
 * 
 * 📌 硬件唤醒源：
 *    - 用户按键 SW3: GPIO39 (RTC_GPIO3, 低电平按下)
 *    - 人体红外 SR602: GPIO34 (RTC_GPIO4, 高电平感应)
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "driver/rtc_io.h"

static const char *TAG = "EXP2_EXT_WAKEUP";

#define BUTTON_PIN   GPIO_NUM_39 // SW3 用户按键 (RTC GPIO)
#define PIR_PIN      GPIO_NUM_34 // SR602 人体红外 (RTC GPIO)

static RTC_DATA_ATTR int s_wake_count = 0;

void app_main(void)
{
    s_wake_count++;

    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 16 实验 2：外部按键与人体红外硬件唤醒   ");
    ESP_LOGI(TAG, "==================================================");

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    switch (cause) {
        case ESP_SLEEP_WAKEUP_EXT0:
            ESP_LOGI(TAG, "🔘 [唤醒来源: EXT0] 检测到按键 SW3 (GPIO39) 被按下！累计唤醒: %d 次", s_wake_count);
            break;
        case ESP_SLEEP_WAKEUP_EXT1: {
            uint64_t wakeup_pin_mask = esp_sleep_get_ext1_wakeup_status();
            if (wakeup_pin_mask & (1ULL << PIR_PIN)) {
                ESP_LOGI(TAG, "🚶 [唤醒来源: EXT1] 人体红外感应到有人靠近！累计唤醒: %d 次", s_wake_count);
            } else {
                ESP_LOGI(TAG, "⚡ [唤醒来源: EXT1] 外部 RTC 引脚组触发");
            }
            break;
        }
        default:
            ESP_LOGI(TAG, "🔌 首次上电开机，准备配置外部休眠唤醒源...");
            break;
    }

    ESP_LOGI(TAG, "⏳ 正在进入 Deep-sleep 休眠状态...");
    ESP_LOGI(TAG, "👉 【测试方法】：按下 SW3 按键 或 在 SR602 红外传感器前挥手，芯片将瞬间被唤醒！\n");

    // 1. 配置 EXT0: GPIO39 为低电平 0 时触发唤醒 (按键按下)
    esp_sleep_enable_ext0_wakeup(BUTTON_PIN, 0);

    // 2. 配置 EXT1: GPIO34 为高电平 1 时触发唤醒 (红外感应)
    const uint64_t ext1_mask = (1ULL << PIR_PIN);
    esp_sleep_enable_ext1_wakeup(ext1_mask, ESP_EXT1_WAKEUP_ANY_HIGH);

    // 进入深度睡眠
    esp_deep_sleep_start();
}
