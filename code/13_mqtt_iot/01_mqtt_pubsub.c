/**
 * 🌟 ESP32 物联网实战 —— 第 13 关 实验 1：MQTT 客户端 Pub/Sub 基础 (Hello MQTT)
 * 
 * 🎯 学习目标：
 *    1. 搞懂 MQTT（消息队列遥测传输协议）发布/订阅（Pub/Sub）通信模型；
 *    2. 掌握 `mqtt_client` 驱动初始化、事件回调与状态监听机制；
 *    3. 成功连接公共 Broker（broker.emqx.io）并完成主题订阅与消息自发自收。
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
#include "mqtt_client.h"
#include "nvs_flash.h"

static const char *TAG = "EXP1_MQTT_PUBSUB";

#define EXAMPLE_WIFI_SSID      "YOUR_WIFI_SSID"
#define EXAMPLE_WIFI_PASS      "YOUR_WIFI_PASSWORD"

// 公共免费测试 MQTT 代理服务器 (EMQX 官方公共服务)
#define MQTT_BROKER_URI        "mqtt://broker.emqx.io:1883"
#define MQTT_TOPIC_TEST        "esp32_journey/hello/test"

#define WIFI_CONNECTED_BIT BIT0
static EventGroupHandle_t s_wifi_event_group;
static esp_mqtt_client_handle_t s_mqtt_client = NULL;

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
    ESP_LOGI(TAG, "✅ Wi-Fi 已就绪！");
}

/* MQTT 事件统一处理回调 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "🎉 [MQTT 状态] 成功连入 MQTT 云端 Broker!");
            // 订阅测试主题
            esp_mqtt_client_subscribe(s_mqtt_client, MQTT_TOPIC_TEST, 0);
            ESP_LOGI(TAG, "📥 成功订阅主题: %s", MQTT_TOPIC_TEST);

            // 发布第一条问候消息
            const char *msg = "Hello World from ESP32!";
            esp_mqtt_client_publish(s_mqtt_client, MQTT_TOPIC_TEST, msg, 0, 0, 0);
            ESP_LOGI(TAG, "📤 已发送测试消息 ➔ %s", msg);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "📩 [收到下行消息] 主题: %.*s", event->topic_len, event->topic);
            ESP_LOGI(TAG, "   正文内容: \033[32m%.*s\033[0m", event->data_len, event->data);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "⚠️ MQTT 服务器断开连接，驱动将自动重连...");
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "❌ MQTT 协议层发生错误");
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

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 13 实验 1：MQTT 客户端 Pub/Sub 基础     ");
    ESP_LOGI(TAG, "==================================================");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init();
    mqtt_app_start();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
