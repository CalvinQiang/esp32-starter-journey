/**
 * 🌟 ESP32 物联网实战 —— 第 09 关 实验 1：NVS 开机启动计数器 (断电不丢失)
 * 
 * 🎯 学习目标：
 *    1. 掌握 ESP32 NVS（Non-Volatile Storage）非易失性存储的基本读写流程；
 *    2. 理解为什么普通变量断电会清零，而 NVS 写入 Flash 可以永久保存；
 *    3. 学习处理首次开机找不到键值（`ESP_ERR_NVS_NOT_FOUND`）的标准容错逻辑。
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "EXP1_BOOT_COUNTER";

#define NVS_NAMESPACE   "app_data"
#define KEY_BOOT_COUNT  "boot_count"

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 09 实验 1：NVS 开机持久化计数器         ");
    ESP_LOGI(TAG, "==================================================");

    // 1. 初始化默认 NVS 分区 (Flash 存储地基)
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // 如果 Flash 发生分区格式变更，自动擦除重建
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_LOGI(TAG, "✅ [第一步] NVS Flash 底层驱动初始化成功！");

    // 2. 打开命名空间（如同拉开一个叫 app_data 的抽屉）
    nvs_handle_t nvs_handle;
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ 打开 NVS 命名空间失败 (%s)", esp_err_to_name(err));
        return;
    }

    // 3. 读取已保存的开机次数
    int32_t boot_count = 0; // 默认值
    err = nvs_get_i32(nvs_handle, KEY_BOOT_COUNT, &boot_count);
    switch (err) {
        case ESP_OK:
            ESP_LOGI(TAG, "📖 [第二步] 从 Flash 成功读取到历史开机次数: \033[32m%ld\033[0m 次", (long)boot_count);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            ESP_LOGW(TAG, "💡 [第二步] 未找到历史记录！这是单片机烧录后的【第 1 次开机】！");
            boot_count = 0;
            break;
        default:
            ESP_LOGE(TAG, "❌ 读取 NVS 失败 (%s)", esp_err_to_name(err));
            break;
    }

    // 4. 计数器 +1 并写回 Flash
    boot_count++;
    ESP_LOGI(TAG, "✍️ [第三步] 本次开机计数值更新为: \033[36m%ld\033[0m 次，正在写入 Flash...", (long)boot_count);
    ESP_ERROR_CHECK(nvs_set_i32(nvs_handle, KEY_BOOT_COUNT, boot_count));

    // 5. 关键提交：将暂存数据真正刷入物理 Flash 颗粒
    ESP_ERROR_CHECK(nvs_commit(nvs_handle));
    ESP_LOGI(TAG, "💾 [第四步] nvs_commit 提交成功！数据已永久固化！");

    // 6. 关闭 NVS 句柄释放内存
    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "--------------------------------------------------");
    ESP_LOGI(TAG, "🎉 实验成功！现在请尝试按下板载的 【EN / RESET】 物理复位按键，");
    ESP_LOGI(TAG, "   或者直接拔下 USB 供电线重新插上，观察计数器是否持续累加！");
    ESP_LOGI(TAG, "--------------------------------------------------");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
