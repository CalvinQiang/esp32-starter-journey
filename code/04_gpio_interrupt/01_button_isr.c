/**
 * 🌟 ESP32 物联网实战 —— 第 04 关 实验 1：GPIO 外部中断与事件驱动
 *    硬件连接: SW3 按键 -> GPIO39 (下降沿中断), LED2 -> GPIO27
 *    技术亮点: IRAM_ATTR 极速中断服务函数、volatile 全局共享变量、微秒级时间戳防抖
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "EXP1_BUTTON_ISR";

#define LED_PIN         GPIO_NUM_27  // 板载受控蓝色 LED2 (输出)
#define BUTTON_PIN      GPIO_NUM_39  // 用户按键 SW3 (输入专用，平时 1，按下 0)

// 全局变量：声明为 volatile，防止编译器过度优化
static volatile uint32_t g_intr_count = 0;
static volatile bool g_led_state = false;
static volatile int64_t g_last_intr_time = 0; // 上次中断时间戳 (微秒)

// ⚡ 硬件中断服务函数 (ISR)
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    int64_t now = esp_timer_get_time(); // 获取硬件微秒时间戳 (ISR 安全)

    // 中断简易消抖：两次中断间隔小于 150ms (150,000µs) 视为弹片弹跳，直接丢弃
    if (now - g_last_intr_time > 150000) {
        g_intr_count++;
        g_led_state = !g_led_state;
        
        // 瞬间翻转 LED (微秒级硬件即时响应！)
        gpio_set_level(LED_PIN, g_led_state ? 1 : 0);
        
        g_last_intr_time = now;
    }
}

static void init_system_interrupt(void)
{
    // 1. 初始化 LED2 为输出，初始置 0
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);

    // 2. 配置 SW3 (GPIO39) 为下降沿中断
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&io_conf);

    // 3. 安装中断驱动服务
    gpio_install_isr_service(0);

    // 4. 挂载中断回调函数
    gpio_isr_handler_add(BUTTON_PIN, gpio_isr_handler, (void*) BUTTON_PIN);

    ESP_LOGI(TAG, "✅ 硬件中断初始化完成：SW3 (GPIO39) 下降沿中断已挂载！");
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   ⚡ 关卡 4 启动：GPIO 外部中断与事件驱动教学     ");
    ESP_LOGI(TAG, "==================================================");

    init_system_interrupt();

    uint32_t last_reported_count = 0;

    // 主循环彻底告别轮询！CPU 可以休眠等待
    while (1) {
        // 只有发生中断时才打印日志（安全地在主任务中处理日志输出）
        if (g_intr_count != last_reported_count) {
            ESP_LOGI(TAG, "⚡ [中断事件捕获] 按键第 %lu 次硬件打断！当前灯光: %s (响应时间戳: %lld ms)",
                     g_intr_count,
                     g_led_state ? "🟢【点亮】" : "⚪【熄灭】",
                     g_last_intr_time / 1000);
            
            last_reported_count = g_intr_count;
        }

        // 主任务休眠 500ms，CPU 彻底释放，静候硬件打断
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
