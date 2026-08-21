/**
 * 🌟 ESP32 物联网实战 —— 第 17 关 实验 1：BSP 硬件驱动层与业务逻辑解耦 (BSP Interface)
 * 
 * 🎯 学习目标：
 *    1. 彻底告别在业务逻辑代码中到处硬编码 `GPIO_NUM_27`、`gpio_set_level()` 的坏习惯；
 *    2. 掌握 BSP（Board Support Package，板级支持包）分层思想；
 *    3. 业务层只依赖清晰抽象的语义化接口（如 `bsp_led_set(true)`、`bsp_button_is_pressed()`）。
 */

#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "EXP1_BSP_LAYER";

/* ====================================================================
 * 📌 一、 BSP 驱动抽象层 (底层：负责直接和硬件引脚打交道)
 * ==================================================================== */
#define BSP_LED2_PIN      GPIO_NUM_27
#define BSP_BUTTON_PIN    GPIO_NUM_39

typedef enum {
    LED_STATE_OFF = 0,
    LED_STATE_ON  = 1,
    LED_STATE_TOGGLE
} bsp_led_cmd_t;

static bool s_current_led_state = false;

static void bsp_init(void)
{
    // 初始化 LED2 输出
    gpio_config_t led_cfg = {
        .pin_bit_mask = (1ULL << BSP_LED2_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led_cfg);
    gpio_set_level(BSP_LED2_PIN, 0);

    // 初始化按键输入
    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << BSP_BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
    };
    gpio_config(&btn_cfg);
    ESP_LOGI(TAG, "🔧 [BSP 硬件层] 底层 GPIO 引脚配置完毕！");
}

static void bsp_led_control(bsp_led_cmd_t cmd)
{
    if (cmd == LED_STATE_TOGGLE) {
        s_current_led_state = !s_current_led_state;
    } else {
        s_current_led_state = (cmd == LED_STATE_ON);
    }
    gpio_set_level(BSP_LED2_PIN, s_current_led_state ? 1 : 0);
}

static bool bsp_button_is_pressed(void)
{
    return (gpio_get_level(BSP_BUTTON_PIN) == 0);
}

/* ====================================================================
 * 🎯 二、 业务应用层 (上层：纯业务逻辑，完全不知道具体是哪根引脚！)
 * ==================================================================== */
static void business_logic_task(void *pvParameters)
{
    ESP_LOGI(TAG, "💼 [业务逻辑层] 业务中枢任务启动运行...");

    while (1) {
        if (bsp_button_is_pressed()) {
            vTaskDelay(pdMS_TO_TICKS(20)); // 消抖
            if (bsp_button_is_pressed()) {
                ESP_LOGI(TAG, "🔘 [业务层感知] 用户触发了按键动作，切换灯光状态！");
                bsp_led_control(LED_STATE_TOGGLE);

                while (bsp_button_is_pressed()) {
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 17 实验 1：BSP 驱动层与业务逻辑解耦     ");
    ESP_LOGI(TAG, "==================================================");

    bsp_init();
    xTaskCreate(business_logic_task, "biz_task", 3072, NULL, 5, NULL);
}
