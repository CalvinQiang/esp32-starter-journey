/**
 * 🌟 ESP32 物联网实战 —— 第 05 关 实验 1：FreeRTOS 多任务调度与双核队列通信
 *    硬件连接: SW3 按键 -> GPIO39 (ISR 生产者), SR602 红外 -> GPIO34 (Task 生产者), LED2 -> GPIO27 (Task 消费者)
 *    技术亮点: 双核绑定 (Core 0 / Core 1)、xQueue 消息胶囊安全通信、中断换幕 portYIELD_FROM_ISR、高水位栈监控
 */
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "LEVEL_5_RTOS";

#define LED_PIN         GPIO_NUM_27  // 板载受控蓝色 LED2 (执行器)
#define BUTTON_PIN      GPIO_NUM_39  // 用户按键 SW3 (中断生产者)
#define PIR_PIN         GPIO_NUM_34  // SR602 人体红外探头 (传感器生产者)

// 1. 数据胶囊结构体定义
typedef enum {
    EVENT_BUTTON_PRESS,    // 按键按下
    EVENT_PIR_MOTION,      // 人体靠近
    EVENT_PIR_VACANT,      // 人体离开
    EVENT_SYSTEM_HEARTBEAT // 系统心跳
} event_type_t;

typedef struct {
    event_type_t type;
    int64_t timestamp_us;
    uint32_t count;
    int sender_core;
} app_event_t;

static QueueHandle_t g_event_queue = NULL;

// 2. 按键中断生产者 (ISR)
static void IRAM_ATTR button_isr_handler(void* arg)
{
    static int64_t last_intr_time = 0;
    static uint32_t btn_press_count = 0;
    int64_t now = esp_timer_get_time();

    if (now - last_intr_time > 150000) {
        btn_press_count++;
        last_intr_time = now;

        app_event_t event = {
            .type = EVENT_BUTTON_PRESS,
            .timestamp_us = now,
            .count = btn_press_count,
            .sender_core = xPortGetCoreID()
        };

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(g_event_queue, &event, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }
}

// 3. 传感器采集生产者任务 (Core 0)
static void task_sensor_producer(void *pvParameters)
{
    int last_pir_level = -1;
    uint32_t pir_trigger_count = 0;

    while (1) {
        int current_pir_level = gpio_get_level(PIR_PIN);
        if (current_pir_level != last_pir_level) {
            pir_trigger_count++;
            app_event_t event = {
                .type = (current_pir_level == 1) ? EVENT_PIR_MOTION : EVENT_PIR_VACANT,
                .timestamp_us = esp_timer_get_time(),
                .count = pir_trigger_count,
                .sender_core = xPortGetCoreID()
            };
            xQueueSend(g_event_queue, &event, pdMS_TO_TICKS(10));
            last_pir_level = current_pir_level;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// 4. 定时心跳生产者任务 (Core 0)
static void task_heartbeat_producer(void *pvParameters)
{
    uint32_t hb_count = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        hb_count++;
        app_event_t event = {
            .type = EVENT_SYSTEM_HEARTBEAT,
            .timestamp_us = esp_timer_get_time(),
            .count = hb_count,
            .sender_core = xPortGetCoreID()
        };
        xQueueSend(g_event_queue, &event, pdMS_TO_TICKS(10));
    }
}

// 5. 执行控制核心消费者任务 (Core 1)
static void task_actuator_consumer(void *pvParameters)
{
    app_event_t rx_event;
    bool led_state = false;
    int64_t pir_auto_off_deadline = 0;
    bool pir_auto_light_active = false;

    while (1) {
        TickType_t wait_ticks = pir_auto_light_active ? pdMS_TO_TICKS(200) : portMAX_DELAY;

        if (xQueueReceive(g_event_queue, &rx_event, wait_ticks) == pdTRUE) {
            int consumer_core = xPortGetCoreID();
            
            switch (rx_event.type) {
                case EVENT_BUTTON_PRESS:
                    pir_auto_light_active = false;
                    led_state = !led_state;
                    gpio_set_level(LED_PIN, led_state ? 1 : 0);
                    ESP_LOGI(TAG, "⚡ [队列接收] 按键中断事件 #%lu | 发送: Core %d ➔ 消费: Core %d | 手动控制: %s",
                             (unsigned long)rx_event.count, rx_event.sender_core, consumer_core,
                             led_state ? "🟢【点亮】" : "⚪【熄灭】");
                    break;

                case EVENT_PIR_MOTION:
                    led_state = true;
                    pir_auto_light_active = true;
                    pir_auto_off_deadline = esp_timer_get_time() + 5000000;
                    gpio_set_level(LED_PIN, 1);
                    ESP_LOGW(TAG, "🚶‍♂️ [队列接收] 人体移动事件 #%lu | 发送: Core %d ➔ 消费: Core %d | 动作: 开启 5 秒智能亮灯倒计时",
                             (unsigned long)rx_event.count, rx_event.sender_core, consumer_core);
                    break;

                case EVENT_PIR_VACANT:
                    ESP_LOGI(TAG, "🍃 [队列接收] 人体离开事件 #%lu | 发送: Core %d ➔ 消费: Core %d | 状态: 进入 5 秒倒计时安全期，不立刻灭灯",
                             (unsigned long)rx_event.count, rx_event.sender_core, consumer_core);
                    break;

                case EVENT_SYSTEM_HEARTBEAT: {
                    UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(NULL);
                    ESP_LOGI(TAG, "💓 [系统心跳 #%lu] 双核流水线运行正常 | 消费者任务剩余栈深: %u 字节",
                             (unsigned long)rx_event.count, (unsigned int)(stack_remaining * sizeof(StackType_t)));
                    break;
                }
            }
        }

        // 检查红外 5 秒智能关灯倒计时
        if (pir_auto_light_active) {
            int64_t now = esp_timer_get_time();
            if (now >= pir_auto_off_deadline) {
                gpio_set_level(LED_PIN, 0);
                led_state = false;
                pir_auto_light_active = false;
                ESP_LOGI(TAG, "⏱️ [智能延时] 5 秒无人活动倒计时结束，指示灯自动熄灭 (节能待机)");
            }
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 5 启动：FreeRTOS 多任务与双核队列通信   ");
    ESP_LOGI(TAG, "==================================================");

    // 1. 初始化 GPIO
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);

    gpio_reset_pin(PIR_PIN);
    gpio_set_direction(PIR_PIN, GPIO_MODE_INPUT);

    // 2. 初始化中断
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&btn_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, (void*)BUTTON_PIN);

    // 3. 创建容量为 10 的消息队列
    g_event_queue = xQueueCreate(10, sizeof(app_event_t));
    if (g_event_queue == NULL) {
        ESP_LOGE(TAG, "❌ 消息队列创建失败！");
        return;
    }

    ESP_LOGI(TAG, "✅ 硬件与队列就绪，开始在双核部署任务流水线...");

    // 4. 跨核任务分配
    xTaskCreatePinnedToCore(task_sensor_producer, "task_sensor", 3072, NULL, 2, NULL, 0);
    xTaskCreatePinnedToCore(task_heartbeat_producer, "task_hb", 2048, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(task_actuator_consumer, "task_actuator", 4096, NULL, 3, NULL, 1);
}
