/**
 * ============================================================================
 * 第 03 关：ESP32 按键输入检测与人体红外感应（感知现实世界的输入）
 * ============================================================================
 * 
 * 学习目标：
 * 1. 理解数字输入（GPIO Input）原理与高低电平判定机制。
 * 2. 掌握用户按键 SW3 (GPIO39) 的电平读取、机械抖动成因与 20ms 软件消抖法。
 * 3. 掌握 SR602 人体红外传感器 (GPIO34) 的电平监测与智能夜灯逻辑。
 * 4. 熟记 ESP32 纯输入管脚（GPIO34/35/36/39）的硬件约束与使用规范。
 * ============================================================================
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "LEVEL_3_INPUT";

// 引脚定义速查
#define LED_PIN         GPIO_NUM_27  // 板载受控指示灯 LED2 (输出)
#define BUTTON_PIN      GPIO_NUM_39  // 用户按键 SW3 (输入专用，按下为 0，松开为 1)
#define PIR_PIN         GPIO_NUM_34  // SR602 人体红外探头 (输入专用，有人为 1，无人为 0)

// 全局状态记录
static bool g_led_state = false; // 当前灯的亮灭状态 (false: 灭, true: 亮)

/**
 * @brief 初始化所有 GPIO 引脚
 */
static void init_system_gpios(void)
{
    // 1. 初始化输出引脚：LED2 (GPIO27)
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0); // 默认初始状态熄灭

    // 2. 初始化输入引脚：用户按键 SW3 (GPIO39)
    // ⚠️ 注意：GPIO39 为纯输入管脚，不支持软件内部上拉/下拉，硬件外部已带有上拉电阻
    gpio_reset_pin(BUTTON_PIN);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);

    // 3. 初始化输入引脚：SR602 人体红外探头 (GPIO34)
    // ⚠️ 注意：GPIO34 同样为纯输入管脚
    gpio_reset_pin(PIR_PIN);
    gpio_set_direction(PIR_PIN, GPIO_MODE_INPUT);

    ESP_LOGI(TAG, "GPIO 初始化完成: LED2 (输出), SW3按键 (输入), SR602红外 (输入)");
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🎉 关卡 3 启动：按键输入与人体红外感知教学     ");
    ESP_LOGI(TAG, "==================================================");

    init_system_gpios();

    int pir_last_state = -1; // 记录上一次红外状态，防止刷屏日志
    int button_last_level = 1; // 按键平时松开为高电平 1

    ESP_LOGI(TAG, ">>> 系统已就绪，请按下板载按键 SW3，或用手靠近 SR602 红外探头...");

    while (1) {
        // -------------------------------------------------------------
        // 【输入检测 1】：用户按键 SW3 扫描与 20ms 软件消抖
        // -------------------------------------------------------------
        int current_btn_level = gpio_get_level(BUTTON_PIN);

        // 如果检测到按键从未按下(1)变为了按下(0)
        if (button_last_level == 1 && current_btn_level == 0) {
            // 💡 软件消抖：延时 20 毫秒跳过机械金属弹片的弹性抖动区
            vTaskDelay(pdMS_TO_TICKS(20));

            // 二次确认：20ms 之后如果依然是低电平 0，才真正判定为“有效按下”
            if (gpio_get_level(BUTTON_PIN) == 0) {
                // 翻转 LED 状态 (开灯变关灯，关灯变开灯)
                g_led_state = !g_led_state;
                gpio_set_level(LED_PIN, g_led_state ? 1 : 0);

                ESP_LOGI(TAG, "🔘 [按键触发] 用户按下了 SW3！当前指示灯切换为: %s", 
                         g_led_state ? "🟢【点亮】" : "⚪【熄灭】");
            }
        }
        // 更新按键的历史电平状态
        button_last_level = current_btn_level;

        // -------------------------------------------------------------
        // 【输入检测 2】：SR602 人体红外探头状态监测
        // -------------------------------------------------------------
        int current_pir_state = gpio_get_level(PIR_PIN);

        // 只有当红外状态发生变化时（有人来 / 有人走），才打印日志
        if (current_pir_state != pir_last_state) {
            if (current_pir_state == 1) {
                ESP_LOGW(TAG, "🚶‍♂️ [红外感应] 探测到人体活动！(GPIO34 = 1 高电平)");
            } else {
                ESP_LOGI(TAG, "🍃 [红外感应] 人体离开或静止无感应。(GPIO34 = 0 低电平)");
            }
            pir_last_state = current_pir_state;
        }

        // 极短休眠 10ms，既不漏掉按键点击，又把算力让给后台系统
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
