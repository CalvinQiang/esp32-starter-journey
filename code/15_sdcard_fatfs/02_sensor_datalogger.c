/**
 * 🌟 ESP32 物联网实战 —— 第 15 关 实验 2：传感器黑匣子 CSV 数据记录仪 (Data Logger)
 * 
 * 🎯 学习目标：
 *    1. 学习工业级传感器数据持续持久化落盘规范（CSV 逗号分隔格式）；
 *    2. 掌握 `fopen(..., "a")` 追加模式与 `fflush()` 缓存及时刷盘技巧，防止断电丢数据；
 *    3. 模拟周期性记录环境气温、剩余内存与时间戳至 TF 卡。
 */

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "EXP2_DATALOGGER";
#define MOUNT_POINT "/sdcard"
#define CSV_FILE_PATH MOUNT_POINT "/sensor_log.csv"

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

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 15 实验 2：传感器 CSV 数据黑匣子记录仪 ");
    ESP_LOGI(TAG, "==================================================");

    sdmmc_card_t *card = NULL;
    if (!init_sd_card(&card)) {
        ESP_LOGE(TAG, "❌ TF 卡挂载失败，请插入 FAT32 格式 TF 卡！");
        return;
    }
    ESP_LOGI(TAG, "✅ TF 卡挂载成功，准备写入 CSV 数据...");

    // 检查 CSV 是否存在，若不存在则写入表头
    struct stat st;
    bool file_exists = (stat(CSV_FILE_PATH, &st) == 0);

    FILE *f = fopen(CSV_FILE_PATH, "a");
    if (!f) {
        ESP_LOGE(TAG, "❌ 无法打开 CSV 文件进行写入");
        return;
    }

    if (!file_exists || st.st_size == 0) {
        fprintf(f, "Timestamp_Sec,Temperature_C,Free_Heap_Bytes,Log_Level\n");
        fflush(f);
        ESP_LOGI(TAG, "📝 已为新文件写入 CSV 表头");
    }

    int record_count = 0;
    while (record_count < 10) { // 连续记录 10 条数据示例
        record_count++;
        int64_t uptime_sec = esp_timer_get_time() / 1000000;
        float mock_temp = 24.5f + (float)(record_count % 3) * 0.4f;
        uint32_t free_heap = esp_get_free_heap_size();

        // 写入一行 CSV 格式记录
        fprintf(f, "%lld,%.2f,%lu,INFO\n", uptime_sec, mock_temp, (unsigned long)free_heap);
        fflush(f); // 关键！强制从 RAM 缓存刷入 TF 卡物理闪存

        ESP_LOGI(TAG, "📊 [写入第 %d 条记录] 时间:%llds, 温度:%.2f°C, 剩余RAM:%luB",
                 record_count, uptime_sec, mock_temp, (unsigned long)free_heap);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    fclose(f);
    ESP_LOGI(TAG, "🎉 记录完毕！已安全关闭文件。");
}
