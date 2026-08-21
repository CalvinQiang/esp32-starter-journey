/**
 * 🌟 ESP32 物联网实战 —— 第 16 关 实验 3：低功耗环境监测哨兵与自动睡眠巡航 (综合大工程)
 * 
 * 🎯 学习目标：
 *    1. 模拟电池供电的野外/智能家居环境哨兵：平时沉睡，定时或感知异常时唤醒；
 *    2. 融合【RTC 定时器 10 秒唤醒】与【按键/红外即时报警唤醒】双重机制；
 *    3. 唤醒后闪烁指示灯、采集模拟数据并保存到 RTC 内存，随后重新进入微安级休眠！
 * 
 * 📌 硬件接口：
 *    - 指示灯 LED2: GPIO27
 *    - 用户按键 SW3: GPIO39
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "esp_log.h"

static const char *TAG = "EXP3_LP_SENTRY";

#define LED2_PIN     GPIO_NUM_27
#define BUTTON_PIN   GPIO_NUM_39
#define SLEEP_SEC    10

// RTC 掉电保持变量
static RTC_DATA_ATTR int s_sentry_cycle = 0;
static RTC_DATA_ATTR float s_last_temp = 25.0f;

static void hardware_blink_active(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED2_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);

    // 唤醒后快速闪烁 2 次指示活跃工作
    gpio_set_level(LED2_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(LED2_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(LED2_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(LED2_PIN, 0);
}

void app_main(void)
{
    s_sentry_cycle++;
    hardware_blink_active();

    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 16 实验 3：低功耗环境哨兵自动巡航工程   ");
    ESP_LOGI(TAG, "==================================================");

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_EXT0) {
        ESP_LOGW(TAG, "🚨 [紧急报警唤醒] 检测到人工按键干预！立即进入全速处理模式！");
    } else if (cause == ESP_SLEEP_WAKEUP_TIMER) {
        ESP_LOGI(TAG, "⏰ [例行巡检唤醒] 哨兵巡航周期: 第 %d 次", s_sentry_cycle);
    } else {
        ESP_LOGI(TAG, "🔌 哨兵首次部署启动...");
    }

    // 模拟传感器采集与处理
    s_last_temp += 0.2f;
    ESP_LOGI(TAG, "📊 采集环境气温: %.1f °C, RTC 运行周期: %d", s_last_temp, s_sentry_cycle);
    ESP_LOGI(TAG, "💤 巡检完成，设置 %d 秒后再次巡航，即将沉睡...", SLEEP_SEC);

    // 1. 定时器唤醒配置
    esp_sleep_enable_timer_wakeup((uint64_t)SLEEP_SEC * 1000000);

    // 2. 外部按键紧急打断配置
    esp_sleep_enable_ext0_wakeup(BUTTON_PIN, 0);

    // 3. 进入微安级深度休眠
    esp_deep_sleep_start();
}
