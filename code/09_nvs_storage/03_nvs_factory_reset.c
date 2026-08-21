/**
 * 🌟 ESP32 物联网实战 —— 第 09 关 实验 3：配置管理中心与按键长按恢复出厂设置 (Factory Reset)
 * 
 * 🎯 学习目标：
 *    1. 学习产品级的 NVS 配置管理架构（开机自检、动态更新、持久化保存）；
 *    2. 掌握 NVS 分区数据全量清空机制（`nvs_erase_all` 与 `nvs_flash_erase`）；
 *    3. 结合用户按键 SW3（GPIO39），实现长按 3 秒“恢复出厂设置”并自动重启单片机。
 * 
 * 📌 硬件引脚：
 *    - SW3 用户按键: GPIO39 (VN，低电平有效)
 *    - LED2 板载指示: GPIO27 (高电平点亮，用于重置闪烁提示)
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "LEVEL09_MANAGER";

#define SW3_BUTTON_PIN      GPIO_NUM_39
#define LED2_PIN            GPIO_NUM_27
#define NVS_NAMESPACE       "device_cfg"
#define KEY_RUN_SECONDS     "run_seconds"
#define KEY_DEVICE_NAME     "dev_name"

static void hardware_init(void)
{
    // 配置 SW3 按键 (纯输入)
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << SW3_BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&btn_conf);

    // 配置 LED2
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED2_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led_conf);
    gpio_set_level(LED2_PIN, 0);
}

/* 恢复出厂设置：清空 NVS 并重启芯片 */
static void perform_factory_reset(void)
{
    ESP_LOGW(TAG, "🚨 [系统警告] 触发长按 3 秒！正在执行【恢复出厂设置 (Factory Reset)】...");
    
    // LED2 快闪 5 次警示
    for (int i = 0; i < 5; i++) {
        gpio_set_level(LED2_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(LED2_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // 格式化擦除整个 NVS 分区
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_LOGI(TAG, "🧹 NVS 分区已全部擦除归零！系统将在 1 秒后自动重启...");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart(); // 软重启
}

/* 按键长按监控任务 */
static void task_button_monitor(void *pvParameters)
{
    int press_counter = 0;
    while (1) {
        if (gpio_get_level(SW3_BUTTON_PIN) == 0) { // 按键按下
            press_counter++;
            gpio_set_level(LED2_PIN, 1);
            ESP_LOGW(TAG, "⚠️ 正在长按 SW3 按键进行重置倒计时: %d / 3 秒...", press_counter);
            if (press_counter >= 3) {
                perform_factory_reset();
            }
        } else {
            press_counter = 0;
            gpio_set_level(LED2_PIN, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    hardware_init();

    // 1. 初始化 NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // 2. 打开配置
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle));

    int32_t run_sec = 0;
    nvs_get_i32(handle, KEY_RUN_SECONDS, &run_sec);

    char dev_name[32] = "ESP32-Smart-Station";
    size_t name_len = sizeof(dev_name);
    if (nvs_get_str(handle, KEY_DEVICE_NAME, dev_name, &name_len) == ESP_ERR_NVS_NOT_FOUND) {
        nvs_set_str(handle, KEY_DEVICE_NAME, dev_name);
        nvs_commit(handle);
    }

    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "🏷️ 设备名称: \033[36m%s\033[0m", dev_name);
    ESP_LOGI(TAG, "⏱️ 累计历史运行时间: \033[32m%ld\033[0m 秒", (long)run_sec);
    ESP_LOGI(TAG, "💡 提示：按住 SW3 按键 3 秒可恢复出厂设置并清空数据！");
    ESP_LOGI(TAG, "==================================================");

    // 启动按键监控后台任务
    xTaskCreate(task_button_monitor, "btn_task", 2048, NULL, 5, NULL);

    // 主循环：每 5 秒将运行时间累加写回 NVS
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        run_sec += 5;
        nvs_set_i32(handle, KEY_RUN_SECONDS, run_sec);
        nvs_commit(handle);
        ESP_LOGI(TAG, "💾 [自动持久化] 累计运行时间已存入 Flash ➔ %ld 秒", (long)run_sec);
    }
}
