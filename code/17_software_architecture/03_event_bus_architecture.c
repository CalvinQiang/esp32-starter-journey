/**
 * 🌟 ESP32 物联网实战 —— 第 17 关 实验 3：统一事件总线 (Event Bus) 驱动的现代化解耦系统 (终极综合)
 * 
 * 🎯 学习目标：
 *    1. 学习 ESP-IDF 原生事件循环（`esp_event_loop`）作为全局系统事件总线（Event Bus）的设计模式；
 *    2. 生产者（传感器任务）只负责广播发布数据事件，完全不需要知道谁在接收；
 *    3. 消费者（UI 渲染任务、MQTT 云端任务）各自独立订阅总线，实现真正的大厂级组件完全解耦！
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event.h"
#include "esp_log.h"

static const char *TAG = "EXP3_EVENT_BUS";

/* 1. 自定义系统事件基础标识 (Event Base) */
ESP_EVENT_DEFINE_BASE(SYSTEM_EVENT_BASE);

/* 2. 事件 ID 枚举 */
typedef enum {
    SYS_EVENT_TEMP_UPDATED = 0, // 气温更新事件
    SYS_EVENT_HUMI_UPDATED,     // 湿度更新事件
    SYS_EVENT_ALARM_TRIGGERED,  // 异常报警事件
} system_event_id_t;

/* 3. 伴随事件传递的载荷数据结构体 */
typedef struct {
    float temperature;
    float humidity;
    uint32_t timestamp_ms;
} sensor_data_event_t;

/* ====================================================================
 * 📊 消费者 1：UI 屏幕渲染模块（订阅事件，刷新屏幕显示）
 * ==================================================================== */
static void ui_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
    if (base == SYSTEM_EVENT_BASE && id == SYS_EVENT_TEMP_UPDATED) {
        sensor_data_event_t *data = (sensor_data_event_t *)event_data;
        ESP_LOGI(TAG, "🖥️ [UI 渲染模块收到广播] 刷新表盘气温: \033[36m%.1f °C\033[0m, 湿度: \033[36m%.1f %%\033[0m",
                 data->temperature, data->humidity);
    }
}

/* ====================================================================
 * ☁️ 消费者 2：MQTT 云端物联网模块（订阅同一事件，异步上传阿里云）
 * ==================================================================== */
static void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t id, void* event_data)
{
    if (base == SYSTEM_EVENT_BASE && id == SYS_EVENT_TEMP_UPDATED) {
        sensor_data_event_t *data = (sensor_data_event_t *)event_data;
        ESP_LOGI(TAG, "☁️ [MQTT 云端模块收到广播] 正在将数据打包上传至阿里云 IoT...");
    }
}

/* ====================================================================
 * 📡 生产者：后台传感器采集任务（只负责采集与向总线抛出事件）
 * ==================================================================== */
static void sensor_producer_task(void *pvParameters)
{
    int count = 0;
    while (count < 5) {
        count++;
        vTaskDelay(pdMS_TO_TICKS(1500));

        sensor_data_event_t evt_payload = {
            .temperature = 25.0f + (float)count * 0.5f,
            .humidity = 60.0f + (float)count,
            .timestamp_ms = count * 1500,
        };

        ESP_LOGI(TAG, "\n📢 [传感器采集生产者] 采集完毕，向全局事件总线广播数据 (Cycle: %d)...", count);
        // 向全局系统事件总线发布事件
        esp_event_post(SYSTEM_EVENT_BASE, SYS_EVENT_TEMP_UPDATED, 
                       &evt_payload, sizeof(evt_payload), portMAX_DELAY);
    }

    ESP_LOGI(TAG, "\n🏆 事件总线生产/消费解耦演示圆满完成！");
    vTaskDelete(NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 17 实验 3：统一事件总线驱动的多模块解耦 ");
    ESP_LOGI(TAG, "==================================================");

    // 1. 初始化系统默认事件总线
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 2. 消费者独立注册事件监听
    ESP_ERROR_CHECK(esp_event_handler_instance_register(SYSTEM_EVENT_BASE, SYS_EVENT_TEMP_UPDATED, 
                                                       &ui_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(SYSTEM_EVENT_BASE, SYS_EVENT_TEMP_UPDATED, 
                                                       &mqtt_event_handler, NULL, NULL));

    // 3. 启动生产者独立采集任务
    xTaskCreate(sensor_producer_task, "sensor_task", 3072, NULL, 5, NULL);
}
