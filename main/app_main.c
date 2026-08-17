/**
 * ============================================================================
 * 关卡 1：串口通信与 Hello World 打印
 * ============================================================================
 * 
 * 学习目标：
 * 1. 理解 ESP32 程序的入口函数 app_main()。
 * 2. 掌握使用 printf() 和 ESP_LOGI() 向电脑串口打印调试日志。
 * 3. 理解 FreeRTOS 的任务延时函数 vTaskDelay() 与死循环 (while(1))。
 * 4. 读取当前芯片的基础运行信息（CPU 核心、主频、剩余内存）。
 * ============================================================================
 */

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"

// 定义当前模块的日志标签（TAG），用于在串口日志前标识来源
static const char *TAG = "LEVEL_1_HELLO";

void app_main(void)
{
    // 1. 打印带有高亮颜色的欢迎标题
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "       🎉 恭喜！ESP32 关卡 1 启动成功！          ");
    ESP_LOGI(TAG, "==================================================");

    // 2. 获取并打印当前芯片的基础硬件参数
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);

    ESP_LOGI(TAG, "【硬件信息】CPU 核心数: %d 核", chip_info.cores);
    ESP_LOGI(TAG, "【硬件信息】芯片版本 (Revision): v%d", chip_info.revision);
    ESP_LOGI(TAG, "【硬件信息】无线特性: %s%s",
             (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "2.4GHz Wi-Fi " : "",
             (chip_info.features & CHIP_FEATURE_BT) ? "+ 经典蓝牙/BLE" : "");

    // 读取 Flash 容量
    if (esp_flash_get_size(NULL, &flash_size) == ESP_OK) {
        ESP_LOGI(TAG, "【硬件信息】板载 Flash 大小: %" PRIu32 " MB", flash_size / (1024 * 1024));
    }

    ESP_LOGI(TAG, "--------------------------------------------------");
    ESP_LOGI(TAG, "开始进入主循环，每秒打印一次心跳...");

    int count = 0;

    // 3. 进入主循环：单片机程序绝大部分时间都在循环等待或周期性执行任务
    while (1) {
        count++;

        // 获取当前系统剩余可用内存 (SRAM)
        uint32_t free_heap = esp_get_free_heap_size();

        // 打印带计数和内存信息的日志
        ESP_LOGI(TAG, "[#%04d] Hello ESP32! 当前空闲内存: %" PRIu32 " 字节", count, free_heap);

        // 使用 printf 普通打印对比
        printf("       -> 来自 printf 的问候: 距离开机运行已过去 %d 秒\n", count);

        // 延时 1000 毫秒 (1 秒)
        // pdMS_TO_TICKS() 会把毫秒转换为 FreeRTOS 的系统时钟节拍(Ticks)
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
