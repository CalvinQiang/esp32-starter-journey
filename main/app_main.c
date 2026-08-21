/**
 * ============================================================================
 * 第 05 关：FreeRTOS 多任务调度与队列(Queue)跨任务通信
 * ============================================================================
 * 
 * 学习目标：
 * 1. 深刻理解嵌入式多任务操作系统的本质（并发、时钟节拍、时间片轮转）。
 * 2. 掌握使用 xTaskCreatePinnedToCore() 在 ESP32 双核（Core 0 / Core 1）上创建与调度任务。
 * 3. 掌握 FreeRTOS 消息队列（Queue）的创建、阻塞等待接收（xQueueReceive）与安全发送。
 * 4. 落地第四关核心思想：在硬件中断 ISR 中通过 xQueueSendFromISR() 跨界安全发消息。
 * 5. 掌握任务栈深度监控（uxTaskGetStackHighWaterMark）与任务优先级设计。
 * ============================================================================
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

// 引脚宏定义
#define LED_PIN         GPIO_NUM_27  // 板载受控蓝色 LED2 (执行器)
#define BUTTON_PIN      GPIO_NUM_39  // 用户按键 SW3 (中断生产者)
#define PIR_PIN         GPIO_NUM_34  // SR602 人体红外探头 (传感器生产者)

// ----------------------------------------------------------------------------
// 1. 结构体定义：事件数据包（装进队列传送带的数据胶囊）
// ----------------------------------------------------------------------------
typedef enum {
    EVENT_BUTTON_PRESS,   // 用户按下 SW3 按键事件
    EVENT_PIR_MOTION,     // SR602 感应到人体靠近事件
    EVENT_PIR_VACANT,     // SR602 感应到人体离开事件
    EVENT_SYSTEM_HEARTBEAT// 系统后台定时心跳事件
} event_type_t;

typedef struct {
    event_type_t type;    // 事件类别
    int64_t timestamp_us; // 事件发生时间戳 (微秒)
    uint32_t count;       // 事件累计计数
    int sender_core;      // 发送者所在的 CPU 核心 (0 或 1)
} app_event_t;

// ----------------------------------------------------------------------------
// 2. 全局句柄：FreeRTOS 消息队列
// ----------------------------------------------------------------------------
static QueueHandle_t g_event_queue = NULL;

// ----------------------------------------------------------------------------
// 3. ⚡ 硬件中断服务函数 (ISR)：按键极速生产者
// ----------------------------------------------------------------------------
static void IRAM_ATTR button_isr_handler(void* arg)
{
    static int64_t last_intr_time = 0;
    static uint32_t btn_press_count = 0;

    int64_t now = esp_timer_get_time();

    // 150ms 简易消抖
    if (now - last_intr_time > 150000) {
        btn_press_count++;
        last_intr_time = now;

        // 打包事件胶囊
        app_event_t event = {
            .type = EVENT_BUTTON_PRESS,
            .timestamp_us = now,
            .count = btn_press_count,
            .sender_core = xPortGetCoreID()
        };

        // 🌟 中断专用安全发队列 API
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(g_event_queue, &event, &xHigherPriorityTaskWoken);

        // 如果唤醒了更高优先级的消费者任务，立即触发上下文切换
        if (xHigherPriorityTaskWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }
}

// ----------------------------------------------------------------------------
// 4. 任务 1：传感器后台采集生产者任务 (运行在 Core 0)
// ----------------------------------------------------------------------------
static void task_sensor_producer(void *pvParameters)
{
    ESP_LOGI(TAG, "🟢 [任务启动] task_sensor_producer 已就绪，绑定在 Core %d (优先级: %d)",
             xPortGetCoreID(), uxTaskPriorityGet(NULL));

    int last_pir_level = -1;
    uint32_t pir_trigger_count = 0;

    while (1) {
        int current_pir_level = gpio_get_level(PIR_PIN);

        // 检测人体红外状态变化
        if (current_pir_level != last_pir_level) {
            pir_trigger_count++;
            
            app_event_t event = {
                .type = (current_pir_level == 1) ? EVENT_PIR_MOTION : EVENT_PIR_VACANT,
                .timestamp_us = esp_timer_get_time(),
                .count = pir_trigger_count,
                .sender_core = xPortGetCoreID()
            };

            // 发送到队列（等待超时 10ms）
            xQueueSend(g_event_queue, &event, pdMS_TO_TICKS(10));
            last_pir_level = current_pir_level;
        }

        // 传感器检测周期：休眠 50ms (让出 Core 0 算力)
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ----------------------------------------------------------------------------
// 5. 任务 2：执行机构核心消费者任务 (运行在 Core 1)
// ----------------------------------------------------------------------------
static void task_actuator_consumer(void *pvParameters)
{
    ESP_LOGI(TAG, "🔵 [任务启动] task_actuator_consumer 已就绪，绑定在 Core %d (优先级: %d)",
             xPortGetCoreID(), uxTaskPriorityGet(NULL));

    app_event_t rx_event;
    bool led_state = false;

    while (1) {
        // 🌟 阻塞等待队列消息！如果队列为空，当前任务彻底休眠，消耗 CPU 算力为 0！
        if (xQueueReceive(g_event_queue, &rx_event, portMAX_DELAY) == pdTRUE) {
            
            // 获取当前消费者运行所在的 CPU 核心
            int consumer_core = xPortGetCoreID();
            
            switch (rx_event.type) {
                case EVENT_BUTTON_PRESS:
                    led_state = !led_state;
                    gpio_set_level(LED_PIN, led_state ? 1 : 0);
                    ESP_LOGI(TAG, "⚡ [队列接收] 按键中断事件 #%lu | 发送端: Core %d ➔ 消费端: Core %d | 灯光翻转为: %s (耗时标记: %lld ms)",
                             rx_event.count, rx_event.sender_core, consumer_core,
                             led_state ? "🟢【点亮】" : "⚪【熄灭】",
                             rx_event.timestamp_us / 1000);
                    break;

                case EVENT_PIR_MOTION:
                    gpio_set_level(LED_PIN, 1);
                    ESP_LOGW(TAG, "🚶‍♂️ [队列接收] 人体移动感应事件 #%lu | 发送端: Core %d ➔ 消费端: Core %d | 动作: 自动亮灯护航",
                             rx_event.count, rx_event.sender_core, consumer_core);
                    break;

                case EVENT_PIR_VACANT:
                    gpio_set_level(LED_PIN, 0);
                    led_state = false;
                    ESP_LOGI(TAG, "🍃 [队列接收] 人体离开感应事件 #%lu | 发送端: Core %d ➔ 消费端: Core %d | 动作: 延时自动熄灯",
                             rx_event.count, rx_event.sender_core, consumer_core);
                    break;

                case EVENT_SYSTEM_HEARTBEAT: {
                    // 栈剩余空间探针 (Watermark: 返回该任务历史上最小剩余堆栈字数)
                    UBaseType_t stack_remaining = uxTaskGetStackHighWaterMark(NULL);
                    ESP_LOGI(TAG, "💓 [系统心跳 #%lu] 双核流水线运行正常 | 消费者剩余栈深: %u 字节",
                             rx_event.count, (unsigned int)(stack_remaining * sizeof(StackType_t)));
                    break;
                }
            }
        }
    }
}

// ----------------------------------------------------------------------------
// 6. 任务 3：系统后台心跳监视任务 (运行在 Core 0)
// ----------------------------------------------------------------------------
static void task_heartbeat(void *pvParameters)
{
    uint32_t heartbeat_count = 0;

    while (1) {
        // 每隔 5 秒向主队列投递一次心跳包
        vTaskDelay(pdMS_TO_TICKS(5000));
        heartbeat_count++;

        app_event_t event = {
            .type = EVENT_SYSTEM_HEARTBEAT,
            .timestamp_us = esp_timer_get_time(),
            .count = heartbeat_count,
            .sender_core = xPortGetCoreID()
        };

        xQueueSend(g_event_queue, &event, pdMS_TO_TICKS(10));
    }
}

// ----------------------------------------------------------------------------
// 7. 硬件外设初始化
// ----------------------------------------------------------------------------
static void init_hardware(void)
{
    // 初始化 LED (输出)
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);

    // 初始化 SR602 (输入)
    gpio_reset_pin(PIR_PIN);
    gpio_set_direction(PIR_PIN, GPIO_MODE_INPUT);

    // 初始化 SW3 按键 (下降沿中断)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&io_conf);

    // 安装中断服务并挂载
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, (void*) BUTTON_PIN);

    ESP_LOGI(TAG, "✅ 硬件初始化就绪: LED2(GPIO27), SW3按键中断(GPIO39), SR602红外(GPIO34)");
}

// ----------------------------------------------------------------------------
// 8. 主入口函数
// ----------------------------------------------------------------------------
void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 5 启动：FreeRTOS 多任务与双核队列通信   ");
    ESP_LOGI(TAG, "==================================================");

    // 1. 初始化底层硬件
    init_hardware();

    // 2. 创建容量为 10 个事件胶囊的消息队列
    g_event_queue = xQueueCreate(10, sizeof(app_event_t));
    if (g_event_queue == NULL) {
        ESP_LOGE(TAG, "❌ 错误：创建 FreeRTOS 消息队列失败！内存不足！");
        return;
    }
    ESP_LOGI(TAG, "✅ FreeRTOS 消息队列已创建 (容量: 10 个数据包, 单包: %d 字节)", sizeof(app_event_t));

    // 3. 创建多任务并绑定到指定 CPU 双核：
    // 参数说明：任务函数, 任务名, 栈大小(字节), 参数, 优先级, 任务句柄, 绑定CPU核心(0/1)
    
    // 生产者 1：传感器采集任务 (绑定 Core 0, 栈 3KB, 优先级 2)
    xTaskCreatePinnedToCore(task_sensor_producer, "task_sensor", 3072, NULL, 2, NULL, 0);

    // 生产者 2：后台心跳定时任务 (绑定 Core 0, 栈 2KB, 优先级 1)
    xTaskCreatePinnedToCore(task_heartbeat, "task_heartbeat", 2048, NULL, 1, NULL, 0);

    // 消费者：执行器处理任务 (绑定 Core 1, 栈 3.5KB, 优先级 3 - 高优先级即时处理)
    xTaskCreatePinnedToCore(task_actuator_consumer, "task_actuator", 3584, NULL, 3, NULL, 1);

    ESP_LOGI(TAG, ">>> FreeRTOS 双核多任务流水线已全面启动！");
    ESP_LOGI(TAG, ">>> 请尝试按下 SW3 按键，或靠近人体红外探头，观察跨核队列通信！");
}
