/**
 * 🌟 ESP32 物联网实战 —— 第 18 关 终极大结局 实验 1：多任务并发多中枢系统架构 (System Dashboard)
 * 
 * 🎯 学习目标：
 *    1. 搭建桌面智能中控台核心多任务骨架（4 大并行 FreeRTOS 任务）；
 *    2. 掌握任务优先级分配、双核负载均衡与栈空间优化原则；
 *    3. 验证传感器融合任务、UI 渲染任务与云端遥测任务高频协同调度。
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "EXP1_FINAL_HUB";

#define LED2_PIN     GPIO_NUM_27
#define BUTTON_PIN   GPIO_NUM_39

/* 统一多传感器融合数据结构体 */
typedef struct {
    float temperature;
    float humidity;
    float distance_cm;
    uint32_t free_heap_kb;
    uint32_t timestamp_sec;
} sensor_fusion_data_t;

static QueueHandle_t s_sensor_queue = NULL;

/* 任务 1：多传感器融合采集任务 (Core 0, 优先级 4) */
static void sensor_fusion_task(void *pvParameters)
{
    ESP_LOGI(TAG, "🌡️ [任务1-传感器中枢] 启动运行于 Core %d...", xPortGetCoreID());
    sensor_fusion_data_t data = {0};
    uint32_t tick = 0;

    while (1) {
        tick += 2;
        data.temperature = 26.5f + (float)(tick % 5) * 0.3f;
        data.humidity = 58.0f + (float)(tick % 10);
        data.distance_cm = 15.0f + (float)(tick % 20);
        data.free_heap_kb = esp_get_free_heap_size() / 1024;
        data.timestamp_sec = tick;

        // 发送给 UI 与云端中枢队列
        xQueueSend(s_sensor_queue, &data, 0);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* 任务 2：UI 渲染与触控响应任务 (Core 1, 优先级 5) */
static void ui_render_task(void *pvParameters)
{
    ESP_LOGI(TAG, "🖥️ [任务2-UI 交互引擎] 启动运行于 Core %d...", xPortGetCoreID());
    sensor_fusion_data_t recv_data;

    while (1) {
        if (xQueueReceive(s_sensor_queue, &recv_data, pdMS_TO_TICKS(500)) == pdPASS) {
            ESP_LOGI(TAG, "🎨 [UI 刷新表盘] 气温: \033[36m%.1f°C\033[0m | 湿度: \033[36m%.1f%%\033[0m | 测距: \033[33m%.1fcm\033[0m | 内存: %luKB",
                     recv_data.temperature, recv_data.humidity, recv_data.distance_cm, recv_data.free_heap_kb);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* 任务 3：电源管理与节能哨兵任务 (Core 0, 优先级 2) */
static void sentry_power_task(void *pvParameters)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED2_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);

    while (1) {
        // 心跳指示灯微弱闪烁
        gpio_set_level(LED2_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_set_level(LED2_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(2950));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🏆 关卡 18 终极大结局：多中枢多任务系统架构   ");
    ESP_LOGI(TAG, "==================================================");

    s_sensor_queue = xQueueCreate(5, sizeof(sensor_fusion_data_t));

    // 调度三大核心并发任务
    xTaskCreatePinnedToCore(sensor_fusion_task, "sensor_task", 3072, NULL, 4, NULL, 0);
    xTaskCreatePinnedToCore(ui_render_task, "ui_task", 4096, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(sentry_power_task, "power_task", 2048, NULL, 2, NULL, 0);
}
