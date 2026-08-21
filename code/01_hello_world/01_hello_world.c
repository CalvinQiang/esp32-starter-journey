/**
 * 🌟 ESP32 物联网实战 —— 第 01 关：串口通信与 Hello World
 *    学习目标: 掌握串口日志输出、芯片信息自检与 FreeRTOS 基础延时
 */
#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"

static const char *TAG = "LEVEL_1_HELLO";

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "       🎉 恭喜！ESP32 关卡 1 启动成功！          ");
    ESP_LOGI(TAG, "==================================================");

    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);

    ESP_LOGI(TAG, "【硬件信息】CPU 核心数: %d 核", chip_info.cores);
    ESP_LOGI(TAG, "【硬件信息】芯片版本: v%d", chip_info.revision);

    if (esp_flash_get_size(NULL, &flash_size) == ESP_OK) {
        ESP_LOGI(TAG, "【硬件信息】板载 Flash 大小: %" PRIu32 " MB", flash_size / (1024 * 1024));
    }

    int count = 0;

    while (1) {
        count++;
        uint32_t free_heap = esp_get_free_heap_size();

        ESP_LOGI(TAG, "[#%04d] Hello ESP32! 当前空闲内存: %" PRIu32 " 字节", count, free_heap);
        printf("       -> 来自 printf 的问候: 距离开机运行已过去 %d 秒\n", count);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
