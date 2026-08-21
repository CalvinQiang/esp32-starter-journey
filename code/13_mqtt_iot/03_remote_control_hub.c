/**
 * 🌟 ESP32 物联网实战 —— 第 13 关 实验 3：手机远程控制中枢与双向联动 (终极综合)
 * 
 * 🎯 学习目标：
 *    1. 搭建双向通信闭环：手机下发控制指令 ➔ ESP32 执行并立即回传 ACK 确认报文；
 *    2. 解析 JSON 控制指令（支持 `set_led`、`reboot`、`get_status`）；
 *    3. 配合 MQTTX 桌面调试端或手机 MQTT App 实现真正的“全球远程遥控物联网中枢”！
 * 
 * 📌 硬件接口：
 *    - 板载绿色 LED2: GPIO27
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "nvs_flash.h"

static const char *TAG = "EXP3_MQTT_HUB";

#define LED2_PIN               GPIO_NUM_27

#define EXAMPLE_WIFI_SSID      "YOUR_WIFI_SSID"
#define EXAMPLE_WIFI_PASS      "YOUR_WIFI_PASSWORD"

#define MQTT_BROKER_URI        "mqtt://broker.emqx.io:1883"
#define TOPIC_CMD              "esp32_journey/device_01/command"
#define TOPIC_ACK              "esp32_journey/device_01/ack"
#define TOPIC_STATUS           "esp32_journey/device_01/status"

#define WIFI_CONNECTED_BIT BIT0
static EventGroupHandle_t s_wifi_event_group;
static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool s_led_status = false;

static void hardware_init(void)
{
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED2_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led_conf);
    gpio_set_level(LED2_PIN, 0);
}

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

    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    ESP_LOGI(TAG, "✅ Wi-Fi 已连接！");
}

/* 回复 ACK 确认报文给手机端 */
static void send_ack_response(const char *cmd_name, bool success, const char *msg)
{
    cJSON *ack = cJSON_CreateObject();
    cJSON_AddStringToObject(ack, "cmd", cmd_name);
    cJSON_AddBoolToObject(ack, "success", success);
    cJSON_AddStringToObject(ack, "message", msg);
    cJSON_AddBoolToObject(ack, "led_state", s_led_status);

    char *json_str = cJSON_PrintUnformatted(ack);
    esp_mqtt_client_publish(s_mqtt_client, TOPIC_ACK, json_str, 0, 1, 0);
    ESP_LOGI(TAG, "📤 [回传 ACK ➔ 手机] %s", json_str);

    free(json_str);
    cJSON_Delete(ack);
}

/* 解析并执行下行控制指令 */
static void handle_downlink_command(const char *payload, int len)
{
    char json_buf[512] = {0};
    if (len >= sizeof(json_buf)) len = sizeof(json_buf) - 1;
    strncpy(json_buf, payload, len);

    ESP_LOGI(TAG, "📥 [收到云端控制指令] %s", json_buf);

    cJSON *root = cJSON_Parse(json_buf);
    if (!root) {
        ESP_LOGE(TAG, "❌ JSON 格式错误");
        return;
    }

    cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
    if (cmd && cmd->valuestring) {
        if (strcmp(cmd->valuestring, "set_led") == 0) {
            cJSON *state = cJSON_GetObjectItem(root, "state");
            if (state) {
                s_led_status = (state->valueint != 0);
                gpio_set_level(LED2_PIN, s_led_status ? 1 : 0);
                ESP_LOGI(TAG, "💡 成功执行开关灯 ➔ %s", s_led_status ? "点亮 (ON)" : "熄灭 (OFF)");
                send_ack_response("set_led", true, s_led_status ? "LED Turned ON" : "LED Turned OFF");
            }
        } else if (strcmp(cmd->valuestring, "reboot") == 0) {
            send_ack_response("reboot", true, "System will reboot in 1s");
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_restart();
        } else {
            send_ack_response(cmd->valuestring, false, "Unknown Command");
        }
    }

    cJSON_Delete(root);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "🎉 MQTT 连接就绪，正在订阅控制主题: %s", TOPIC_CMD);
            esp_mqtt_client_subscribe(s_mqtt_client, TOPIC_CMD, 1);
            send_ack_response("online_init", true, "ESP32 Ready for Commands");
            break;
        case MQTT_EVENT_DATA:
            handle_downlink_command(event->data, event->data_len);
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
    ESP_LOGI(TAG, "   🚀 关卡 13 实验 3：手机远程控制中枢综合工程     ");
    ESP_LOGI(TAG, "==================================================");

    hardware_init();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init();
    mqtt_app_start();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
