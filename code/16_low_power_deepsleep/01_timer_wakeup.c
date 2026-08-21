/**
 * 🌟 ESP32 物联网实战 —— 第 16 关 实验 1：Timer 定时器深度睡眠与 RTC 数据保持 (Deep-sleep Timer)
 * 
 * 🎯 学习目标：
 *    1. 搞懂 ESP32 深度睡眠（Deep-sleep）机制（主 CPU、RAM、Wi-Fi/蓝牙全部断电，仅 RTC 控制器工作）；
 *    2. 掌握 `RTC_DATA_ATTR` 内存属性，让关键变量在芯片睡醒后不丢失（免受 Flash 擦写损耗）；
 *    3. 配置定时器自动唤醒源并进入深度睡眠。
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_sleep.h"
#include "esp_log.h"

static const char *TAG = "EXP1_DEEP_SLEEP";

#define WAKEUP_INTERVAL_US (5 * 1000 * 1000) // 5 秒后唤醒 (单位: 微秒)

// ⚡ 关键：存放在 8KB RTC 慢速慢速 RAM 中的变量，深度睡眠期间不会被清空！
static RTC_DATA_ATTR int s_boot_count = 0;

void app_main(void)
{
    s_boot_count++;

    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 16 实验 1：Timer 定时深度休眠与 RTC 保持 ");
    ESP_LOGI(TAG, "==================================================");

    ESP_LOGI(TAG, "🎉 [芯片已唤醒] 累计唤醒运行次数: \033[32m%d\033[0m", s_boot_count);

    // 检查唤醒原因
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_TIMER) {
        ESP_LOGI(TAG, "⏰ 唤醒来源: 硬件 RTC 定时器闹钟超时唤醒");
    } else {
        ESP_LOGI(TAG, "🔌 唤醒来源: 首次上电复位 (Power-on Reset)");
    }

    ESP_LOGI(TAG, "⏳ 模拟执行 1 秒的传感器采样或业务逻辑...");
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "😴 正在设置 5 秒定时器，即将进入 Deep-sleep 深度休眠 (电流降至 ~5μA)...");

    // 配置定时器唤醒
    esp_sleep_enable_timer_wakeup(WAKEUP_INTERVAL_US);

    // 立即进入深度休眠
    esp_deep_sleep_start();

    // ⚠️ 注意：下面的代码永远不会被执行，因为睡醒后相当于芯片重新复位执行 app_main()！
}
