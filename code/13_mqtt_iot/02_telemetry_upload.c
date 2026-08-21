/**
 * 🌟 ESP32 物联网实战 —— 第 13 关 实验 2：设备遥测数据定时 JSON 上报 (Telemetry Upload)
 * 
 * 🎯 学习目标：
 *    1. 学习工业级物联网设备的遥测属性（Telemetry）打包规范；
 *    2. 结合 `cJSON` 动态构建设备状态报文（可用内存、系统运行时间、信号强度、传感器数据）；
 *    3. 定时（如每 5 秒）向云端主题发送 JSON 遥测数据流。
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "nvs_flash.h"

static const char *TAG = "EXP2_TELEMETRY";

#define EXAMPLE_WIFI_SSID      "YOUR_WIFI_SSID"
#define EXAMPLE_WIFI_PASS      "YOUR_WIFI_PASSWORD"

#define MQTT_BROKER_URI        "mqtt://broker.emqx.io:1883"
#define MQTT_TOPIC_TELEMETRY   "esp32_journey/device_01/telemetry"

#define WIFI_CONNECTED_BIT BIT0
static EventGroupHandle_t s_wifi_event_group;
static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool s_mqtt_connected = false;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
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

static void wifi_init(void)
{
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = { .ssid = EXAMPLE_WIFI_SSID, .password = EXAMPLE_WIFI_PASS },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "📡 正在连接 Wi-Fi...");
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    ESP_LOGI(TAG, "✅ Wi-Fi 连接成功！");
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            s_mqtt_connected = true;
            ESP_LOGI(TAG, "🎉 MQTT 连接成功，遥测通道已就绪！");
            break;
        case MQTT_EVENT_DISCONNECTED:
            s_mqtt_connected = false;
            ESP_LOGW(TAG, "⚠️ MQTT 断开连接");
            break;
        default:
            break;
    }
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
    };
    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_mqtt_client);
}

/* 定时打包并上报 JSON 遥测数据 */
static void telemetry_task(void *pvParameters)
{
    int counter = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (!s_mqtt_connected) continue;

        counter++;
        uint32_t free_heap = esp_get_free_heap_size();
        int64_t uptime_sec = esp_timer_get_time() / 1000000;
        float mock_temp = 25.0f + (float)(counter % 5);

        // 使用 cJSON 构建标准报文
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "device_id", "esp32_starter_01");
        cJSON_AddNumberToObject(root, "uptime_sec", (double)uptime_sec);
        cJSON_AddNumberToObject(root, "free_heap", (double)free_heap);
        cJSON_AddNumberToObject(root, "temperature", (double)mock_temp);
        cJSON_AddNumberToObject(root, "seq_id", (double)counter);

        char *json_str = cJSON_PrintUnformatted(root);
        esp_mqtt_client_publish(s_mqtt_client, MQTT_TOPIC_TELEMETRY, json_str, 0, 1, 0);

        ESP_LOGI(TAG, "📤 [遥测上报 ➔ %s]:\n   \033[36m%s\033[0m", MQTT_TOPIC_TELEMETRY, json_str);

        free(json_str);     // 释放打印的字符串
        cJSON_Delete(root); // 释放 JSON 树对象
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 13 实验 2：设备遥测数据定时 JSON 上报    ");
    ESP_LOGI(TAG, "==================================================");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init();
    mqtt_app_start();

    xTaskCreate(telemetry_task, "telem_task", 4096, NULL, 5, NULL);
}
