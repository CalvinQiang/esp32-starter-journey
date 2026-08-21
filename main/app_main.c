/**
 * ============================================================================
 * 第 04 关：ESP32 GPIO 外部中断(ISR)与按键事件驱动
 * ============================================================================
 * 
 * 学习目标：
 * 1. 告别 while(1) 轮询死等，掌握单片机核心机制 —— 硬件外部中断（Interrupt）。
 * 2. 学习中断服务函数（ISR）编写规范与 IRAM_ATTR 内存修饰符的作用。
 * 3. 掌握下降沿中断触发（NEGEDGE）与极低延迟的硬件事件驱动。
 * 4. 深刻理解中断上下文与任务上下文的区别，树立中断安全编程意识。
 * ============================================================================
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "LEVEL_4_INTR";

// 引脚定义
#define LED_PIN         GPIO_NUM_27  // 板载受控蓝色 LED2 (输出)
#define BUTTON_PIN      GPIO_NUM_39  // 用户按键 SW3 (输入专用，平时 1，按下 0)

// 全局变量：记录中断触发次数与灯光状态（声明为 volatile 防止编译器过度优化）
static volatile uint32_t g_intr_count = 0;
static volatile bool g_led_state = false;
static volatile int64_t g_last_intr_time = 0; // 上次中断时间戳 (微秒)

/**
 * @brief GPIO 硬件中断服务函数 (ISR: Interrupt Service Routine)
 * 
 * ⚠️ 极其重要的嵌入式编程规则：
 * 1. 必须使用 IRAM_ATTR 修饰：保证函数代码常驻内部高速 RAM，即使 Flash 正在擦写也能瞬间响应！
 * 2. 保持极致精简（快进快出）：禁止调用 vTaskDelay()、禁止使用复杂的浮点计算或大量 printf。
 */
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    // 获取当前微秒级硬件时间戳 (esp_timer_get_time 是 ISR 安全的)
    int64_t now = esp_timer_get_time();

    // 硬件简易软件消抖：如果两次中断间隔小于 150 毫秒 (150,000 微秒)，判定为按键物理弹跳，直接忽略
    if (now - g_last_intr_time > 150000) {
        g_intr_count++;
        g_led_state = !g_led_state;
        
        // 瞬间硬件翻转 LED2 电平 (微秒级超快响应！)
        gpio_set_level(LED_PIN, g_led_state ? 1 : 0);
        
        g_last_intr_time = now;
    }
}

/**
 * @brief 初始化系统 GPIO 与硬件中断
 */
static void init_system_interrupt(void)
{
    // 1. 初始化 LED2 为输出，并赋安全初始值 0 (熄灭)
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);

    // 2. 配置 SW3 按键 (GPIO39) 为下降沿中断触发
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),       // 目标引脚 GPIO39
        .mode = GPIO_MODE_INPUT,                    // 输入模式
        .pull_up_en = GPIO_PULLUP_DISABLE,          // GPIO39 内部无上拉，依靠板载硬件 10k 电阻
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE              // 下降沿触发：由 3.3V 变为 0V (按下的瞬间)
    };
    gpio_config(&io_conf);

    // 3. 安装全局 GPIO 中断服务 (参数 0 表示默认中断分配优先级)
    gpio_install_isr_service(0);

    // 4. 为特定引脚绑定专属中断处理回调函数
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

    ESP_LOGI(TAG, ">>> 系统已进入低功耗待机状态，CPU 完全无需轮询检测...");
    ESP_LOGI(TAG, ">>> 请随时按下 SW3 按键，体验硬件中断的微秒级瞬间响应！");

    // 主任务进入休闲的低频监控状态，CPU 算力 100% 解放！
    while (1) {
        // 只有当中断计数发生改变时，才在主任务中打印日志 (主任务打印更安全，不阻塞中断)
        if (g_intr_count != last_reported_count) {
            ESP_LOGI(TAG, "⚡ [中断事件捕获] 按键第 %lu 次硬件打断！当前灯光: %s (响应时间戳: %lld ms)",
                     g_intr_count,
                     g_led_state ? "🟢【点亮】" : "⚪【熄灭】",
                     g_last_intr_time / 1000);
            
            last_reported_count = g_intr_count;
        }

        // 主任务休眠 500ms，CPU 彻底释放，等待硬件下一次打断
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
