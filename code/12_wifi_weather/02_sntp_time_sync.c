/**
 * 🌟 ESP32 物联网实战 —— 第 12 关 实验 2：SNTP 网络时间同步与北京时间毫秒级对时
 * 
 * 🎯 学习目标：
 *    1. 搞懂 NTP（Network Time Protocol）网络授时协议工作机制；
 *    2. 配置阿里云（ntp.aliyun.com）与国家授时中心 NTP 服务器；
 *    3. 设置中国标准时间时区（UTC+8: CST-8），同步 ESP32 内部硬件 RTC 实时时钟。
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "nvs_flash.h"

static const char *TAG = "EXP2_SNTP_SYNC";

#define EXAMPLE_WIFI_SSID      "YOUR_WIFI_SSID"
#define EXAMPLE_WIFI_PASS      "YOUR_WIFI_PASSWORD"

#define WIFI_CONNECTED_BIT BIT0
static EventGroupHandle_t s_wifi_event_group;

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_connect(void)
{
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = { .ssid = EXAMPLE_WIFI_SSID, .password = EXAMPLE_WIFI_PASS },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "📡 正在连接 Wi-Fi...");
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    ESP_LOGI(TAG, "✅ Wi-Fi 已连接，准备启动 SNTP 网络授时...");
}

/* 时间同步完成回调 */
static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "⏰ [SNTP 回调] 收到 NTP 服务器授时响应，硬件 RTC 时钟已校准！");
}

static void sntp_sync_init(void)
{
    ESP_LOGI(TAG, "🌐 正在初始化 SNTP 客户端...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");   // 阿里云 NTP
    esp_sntp_setservername(1, "cn.pool.ntp.org");  // 中国 NTP 池
    esp_sntp_setservername(2, "time.asia.apple.com");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();

    // 设置中国标准时区：东八区 UTC+8 (CST-8)
    setenv("TZ", "CST-8", 1);
    tzset();

    // 等待时间同步成功
    int retry = 0;
    const int retry_count = 15;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "⏳ 正在与阿里云授时中心对时中 (%d/%d)...", retry, retry_count);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 12 实验 2：SNTP 网络授时与北京时间时钟   ");
    ESP_LOGI(TAG, "==================================================");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_connect();
    sntp_sync_init();

    // 主循环：每秒读取并打印一次精准北京时间
    while (1) {
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);

        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%Y年%m月%d日  %H:%M:%S  星期%u", &timeinfo);
        ESP_LOGI(TAG, "🕒 [北京时间] \033[32m%s\033[0m", strftime_buf);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
