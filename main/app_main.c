/**
 * ============================================================================
 * 关卡 2：GPIO 输出控制与 PWM 呼吸灯（点亮物理世界的第一颗灯）
 * ============================================================================
 * 
 * 学习目标：
 * 1. 理解什么是 GPIO（通用输入输出引脚）以及数字高低电平（3.3V / 0V）。
 * 2. 掌握使用 gpio_set_level() 控制板载蓝色 LED2（GPIO27）以 500ms 频率闪烁（Blink）。
 * 3. 掌握使用 LEDC (LED Controller) 外设与 PWM（脉冲宽度调制）实现平滑的“呼吸灯”效果。
 * ============================================================================
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "LEVEL_2_LED";

// 开发板板载可编程蓝色 LED2 对应的引脚为 GPIO27
#define LED_PIN GPIO_NUM_27

// PWM 呼吸灯相关硬件参数配置
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO          LED_PIN
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // 13 位分辨率：最大占空比为 2^13 - 1 = 8191
#define LEDC_FREQUENCY          (5000)            // PWM 频率 5000 Hz (5 kHz)，肉眼绝无闪烁

/**
 * @brief 阶段一：基础 GPIO 输出模式初始化（用于普通闪烁）
 */
static void init_led_gpio(void)
{
    // 1. 重置引脚为默认状态
    gpio_reset_pin(LED_PIN);
    // 2. 将引脚设置为输出模式 (Output)
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    // 3. 默认输出低电平（初始熄灭）
    gpio_set_level(LED_PIN, 0);

    ESP_LOGI(TAG, "GPIO27 初始化完成，当前处于普通数字输出模式");
}

/**
 * @brief 阶段二：LEDC (PWM) 外设初始化（用于呼吸灯渐变）
 */
static void init_ledc_pwm(void)
{
    // 1. 配置 LEDC 定时器 (频率 5kHz, 13位分辨率)
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .duty_resolution  = LEDC_DUTY_RES,
        .timer_num        = LEDC_TIMER,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // 2. 配置 LEDC 输出通道并绑定到 GPIO27
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LEDC_OUTPUT_IO,
        .duty           = 0, // 初始占空比为 0 (完全熄灭)
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);

    ESP_LOGI(TAG, "LEDC PWM 外设初始化完成，已开启 13 位硬件渐变呼吸模式");
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🎉 关卡 2 启动：板载蓝色 LED2 (GPIO27) 教学    ");
    ESP_LOGI(TAG, "==================================================");

    // -------------------------------------------------------------
    // 【实战演练 1】：基础闪烁演示（Blink）—— 快速闪烁 6 次（3秒）
    // -------------------------------------------------------------
    init_led_gpio();
    ESP_LOGI(TAG, ">>> [模式 1] 开始执行基础闪烁演示 (Blink) 6 次...");

    for (int i = 1; i <= 6; i++) {
        ESP_LOGI(TAG, "-> 第 %d 次点亮 LED (高电平 3.3V)", i);
        gpio_set_level(LED_PIN, 1);       // 输出高电平：点亮 LED2
        vTaskDelay(pdMS_TO_TICKS(500));   // 亮 500ms

        ESP_LOGI(TAG, "-> 第 %d 次熄灭 LED (低电平 0V)", i);
        gpio_set_level(LED_PIN, 0);       // 输出低电平：熄灭 LED2
        vTaskDelay(pdMS_TO_TICKS(500));   // 灭 500ms
    }

    // -------------------------------------------------------------
    // 【实战演练 2】：PWM 硬件呼吸灯（渐亮渐暗，无限循环）
    // -------------------------------------------------------------
    ESP_LOGI(TAG, "--------------------------------------------------");
    ESP_LOGI(TAG, ">>> [模式 2] 切换至 PWM 呼吸灯模式 (平滑呼吸循环)...");
    init_ledc_pwm();

    // 13 位分辨率的最大亮度值为 8191
    const int max_duty = (1 << 13) - 1;
    const int step = 150; // 亮度每次递增/递减的步长

    while (1) {
        // 1. 从暗到亮（吸气渐亮）
        for (int duty = 0; duty <= max_duty; duty += step) {
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
            vTaskDelay(pdMS_TO_TICKS(20)); // 每 20ms 调高一次亮度
        }

        // 2. 在最高亮度稍微保持 100ms
        vTaskDelay(pdMS_TO_TICKS(100));

        // 3. 从亮到暗（呼气渐暗）
        for (int duty = max_duty; duty >= 0; duty -= step) {
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
            vTaskDelay(pdMS_TO_TICKS(20)); // 每 20ms 调低一次亮度
        }

        // 4. 在完全熄灭状态稍微停顿 200ms
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
