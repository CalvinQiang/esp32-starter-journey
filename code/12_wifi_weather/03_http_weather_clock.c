/**
 * 🌟 ESP32 物联网实战 —— 第 12 关 实验 3：HTTP RESTful 天气客户端与 cJSON 键值解析 (终极综合)
 * 
 * 🎯 学习目标：
 *    1. 掌握 `esp_http_client` 发起 HTTP GET 请求获取云端 JSON 数据；
 *    2. 掌握轻量级嵌入式 `cJSON` 库解析多层嵌套 JSON 对象的标准范式；
 *    3. 构建联网自动授时、动态获取互联网气象的综合天气时钟站。
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
#include "esp_http_client.h"
#include "cJSON.h"
#include "nvs_flash.h"

static const char *TAG = "EXP3_WEATHER_CLOCK";

#define EXAMPLE_WIFI_SSID      "YOUR_WIFI_SSID"
#define EXAMPLE_WIFI_PASS      "YOUR_WIFI_PASSWORD"

// 公开免 Key 天气 API 示例 (Open-Meteo 实时气象)
#define WEATHER_API_URL        "http://api.open-meteo.com/v1/forecast?latitude=39.9042&longitude=116.4074&current_weather=true"

#define WIFI_CONNECTED_BIT BIT0
static EventGroupHandle_t s_wifi_event_group;

#define MAX_HTTP_RECV_BUFFER 1024
static char s_response_buffer[MAX_HTTP_RECV_BUFFER];
static int s_buffer_len = 0;

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
    ESP_LOGI(TAG, "✅ Wi-Fi 联网就绪！");
}

static void sntp_sync(void)
{
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_init();
    setenv("TZ", "CST-8", 1);
    tzset();
}

/* HTTP 事件接收回调 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (s_buffer_len + evt->data_len < MAX_HTTP_RECV_BUFFER) {
                memcpy(s_response_buffer + s_buffer_len, evt->data, evt->data_len);
                s_buffer_len += evt->data_len;
                s_response_buffer[s_buffer_len] = '\0';
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

/* 解析天气 JSON 报文 */
static void parse_weather_json(const char *json_str)
{
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        ESP_LOGE(TAG, "❌ cJSON 解析失败: 格式无效");
        return;
    }

    cJSON *current = cJSON_GetObjectItem(root, "current_weather");
    if (current) {
        cJSON *temp = cJSON_GetObjectItem(current, "temperature");
        cJSON *wind = cJSON_GetObjectItem(current, "windspeed");
        cJSON *code = cJSON_GetObjectItem(current, "weathercode");

        ESP_LOGI(TAG, "🌦️ ==================================================");
        ESP_LOGI(TAG, "🌦️ [云端气象快报 - 北京站]                          ");
        ESP_LOGI(TAG, "🌡️ 当前气温: \033[32m%.1f °C\033[0m", temp ? temp->valuedouble : 0.0);
        ESP_LOGI(TAG, "💨 实时风速: \033[36m%.1f km/h\033[0m", wind ? wind->valuedouble : 0.0);
        ESP_LOGI(TAG, "🌤️ 天气代号: %d", code ? code->valueint : 0);
        ESP_LOGI(TAG, "🌦️ ==================================================");
    }

    cJSON_Delete(root); // 必须释放 cJSON 根节点内存
}

/* 发起 HTTP 请求获取天气 */
static void fetch_weather_task(void *pvParameters)
{
    while (1) {
        s_buffer_len = 0;
        memset(s_response_buffer, 0, sizeof(s_response_buffer));

        esp_http_client_config_t config = {
            .url = WEATHER_API_URL,
            .event_handler = http_event_handler,
            .timeout_ms = 5000,
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_err_t err = esp_http_client_perform(client);

        if (err == ESP_OK) {
            int status_code = esp_http_client_get_status_code(client);
            if (status_code == 200) {
                ESP_LOGI(TAG, "📥 成功获取云端 HTTP 200 响应，正在解析 JSON...");
                parse_weather_json(s_response_buffer);
            } else {
                ESP_LOGW(TAG, "⚠️ HTTP 请求响应码异常: %d", status_code);
            }
        } else {
            ESP_LOGE(TAG, "❌ HTTP 请求发送失败: %s", esp_err_to_name(err));
        }

        esp_http_client_cleanup(client);

        // 每 60 秒更新一次天气
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 12 实验 3：HTTP 天气时钟综合系统       ");
    ESP_LOGI(TAG, "==================================================");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_connect();
    sntp_sync();

    // 启动天气抓取异步后台任务
    xTaskCreate(fetch_weather_task, "weather_task", 4096, NULL, 5, NULL);

    // 主循环：每秒打印北京时间
    while (1) {
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);

        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
        ESP_LOGI(TAG, "⏰ [RTC 时钟] %s", time_str);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
