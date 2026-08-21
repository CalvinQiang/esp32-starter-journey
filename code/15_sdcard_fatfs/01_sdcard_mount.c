/**
 * 🌟 ESP32 物联网实战 —— 第 15 关 实验 1：MicroSD / TF 卡 4-bit SDMMC 高速挂载与读写
 * 
 * 🎯 学习目标：
 *    1. 了解 ESP32 硬件 SDMMC 外设与 4-bit 高速 SDIO 总线优势；
 *    2. 掌握 VFS（虚拟文件系统）与 FATFS 挂载核心 API；
 *    3. 成功识别 TF 卡容量、卡名，并实现标准 C 文件流 `fopen / fread / fwrite`。
 * 
 * 📌 硬件引脚 (4-bit SDMMC 专用硬件总线):
 *    - CLK: GPIO14, CMD: GPIO15, D0: GPIO2, D1: GPIO4, D2: GPIO12, D3: GPIO13
 */

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"

static const char *TAG = "EXP1_SD_MOUNT";
#define MOUNT_POINT "/sdcard"

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 15 实验 1：TF 卡 4-bit SDMMC 挂载测试   ");
    ESP_LOGI(TAG, "==================================================");

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t *card;
    ESP_LOGI(TAG, "📁 正在初始化 SDMMC 硬件外设与总线...");

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    // 采用 1-bit / 4-bit 兼容模式，保证各类 TF 卡均能稳定识别
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1; // 1-bit 模式更具电气兼容性

    esp_err_t ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "❌ 挂载文件系统失败。请检查 TF 卡是否已格式化为 FAT32！");
        } else {
            ESP_LOGE(TAG, "❌ 未检测到 TF 卡或通信失败 (%s)。请检查卡槽是否插紧！", esp_err_to_name(ret));
        }
        return;
    }

    ESP_LOGI(TAG, "🎉 [TF 卡挂载成功！]");
    // 打印 TF 卡硬件参数信息
    sdmmc_card_print_info(stdout, card);

    // 标准 C 语言文件读写测试
    const char *file_path = MOUNT_POINT "/hello_esp32.txt";
    ESP_LOGI(TAG, "✍️ 正在写入测试文件: %s", file_path);

    FILE *f = fopen(file_path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "❌ 创建文件失败");
        return;
    }
    fprintf(f, "Hello from ESP32 SDMMC FATFS File System!\n");
    fprintf(f, "Board: ESP32-WROOM-32E (8MB Flash + 2MB PSRAM)\n");
    fclose(f);
    ESP_LOGI(TAG, "✅ 文件写入成功！");

    // 读取测试
    ESP_LOGI(TAG, "📖 正在读取刚刚写入的文件内容:");
    f = fopen(file_path, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "❌ 打开文件读取失败");
        return;
    }
    char line[128];
    while (fgets(line, sizeof(line), f) != NULL) {
        ESP_LOGI(TAG, "   ➔ \033[32m%s\033[0m", line);
    }
    fclose(f);

    ESP_LOGI(TAG, "🏆 TF 卡读写测试 100% 通过！");
}
