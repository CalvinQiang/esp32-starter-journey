/**
 * 🌟 ESP32 物联网实战 —— 第 15 关 实验 3：TF 卡文件资源浏览器与目录树遍历 (终极综合)
 * 
 * 🎯 学习目标：
 *    1. 掌握 POSIX 标准目录遍历 API（`opendir()`, `readdir()`, `closedir()`）；
 *    2. 递归检索 TF 卡根目录下所有文件夹与文件，格式化输出文件大小（KB/MB）；
 *    3. 为后续构建“TF 卡离线电子相册”或“本地 Web 静态网页服务器”打下坚实文件系统基石！
 */

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"

static const char *TAG = "EXP3_SD_EXPLORER";
#define MOUNT_POINT "/sdcard"

static bool init_sd_card(sdmmc_card_t **out_card)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;

    esp_err_t ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, out_card);
    return (ret == ESP_OK);
}

/* 递归遍历目录树并打印 */
static void scan_directory_tree(const char *dir_path, int depth)
{
    DIR *d = opendir(dir_path);
    if (!d) {
        ESP_LOGE(TAG, "❌ 无法打开目录: %s", dir_path);
        return;
    }

    struct dirent *dir;
    while ((dir = readdir(d)) != NULL) {
        // 跳过当前与上级目录符号
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
            continue;
        }

        char full_path[300];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, dir->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0) {
            // 缩进排版
            char indent[32] = {0};
            for (int i = 0; i < depth; i++) {
                strcat(indent, "  │ ");
            }

            if (S_ISDIR(st.st_mode)) {
                ESP_LOGI(TAG, "%s📁 \033[1;33m[%s]\033[0m (文件夹)", indent, dir->d_name);
                // 递归扫描子目录
                scan_directory_tree(full_path, depth + 1);
            } else {
                float size_kb = (float)st.st_size / 1024.0f;
                ESP_LOGI(TAG, "%s📄 \033[36m%-20s\033[0m \033[90m(%.2f KB)\033[0m", 
                         indent, dir->d_name, size_kb);
            }
        }
    }

    closedir(d);
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 15 实验 3：TF 卡目录树与文件浏览器      ");
    ESP_LOGI(TAG, "==================================================");

    sdmmc_card_t *card = NULL;
    if (!init_sd_card(&card)) {
        ESP_LOGE(TAG, "❌ TF 卡挂载失败，请插入 TF 卡后重试！");
        return;
    }

    ESP_LOGI(TAG, "🌲 正在深度检索 TF 卡文件系统目录树结构...\n");
    ESP_LOGI(TAG, "📁 [SD 卡根目录: %s]", MOUNT_POINT);

    scan_directory_tree(MOUNT_POINT, 0);

    ESP_LOGI(TAG, "\n==================================================");
    ESP_LOGI(TAG, "🎉 目录扫描完毕！共检索出所有可用资源。");
    ESP_LOGI(TAG, "==================================================");
}
