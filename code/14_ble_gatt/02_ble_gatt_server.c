/**
 * 🌟 ESP32 物联网实战 —— 第 14 关 实验 2：GATT Server 特征值读写与数据透传 (Read/Write)
 * 
 * 🎯 学习目标：
 *    1. 搞懂 GATT 协议的四层树状模型（Profile ➔ Service ➔ Characteristic ➔ Descriptor）；
 *    2. 注册自定义 GATT 服务（Service UUID: 0x00FF）与读写特征值（Char UUID: 0xFF01）；
 *    3. 手机 App 连接蓝牙并写入数据，ESP32 接收并实时控制板载 LED2。
 * 
 * 📌 硬件接口：
 *    - 板载绿色 LED2: GPIO27
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

static const char *TAG = "EXP2_GATT_SERVER";

#define LED2_PIN               GPIO_NUM_27
#define DEVICE_NAME            "ESP32-GATT-Server"

#define GATTS_SERVICE_UUID_TEST     0x00FF
#define GATTS_CHAR_UUID_TEST        0xFF01
#define GATTS_NUM_HANDLE_TEST       4

static uint16_t s_service_handle = 0;
static uint16_t s_char_handle = 0;
static uint16_t s_conn_id = 0;
static bool s_is_connected = false;

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = true,
    .flag                = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            ESP_LOGI(TAG, "📡 [BLE 广播中] 手机 App 可连接: \033[36m%s\033[0m", DEVICE_NAME);
            break;
        default:
            break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
        case ESP_GATTS_REG_EVT: {
            esp_gatt_srvc_id_t service_id = {
                .is_primary = true,
                .id = {
                    .inst_id = 0x00,
                    .uuid = {
                        .len = ESP_UUID_LEN_16,
                        .uuid = { .uuid16 = GATTS_SERVICE_UUID_TEST },
                    },
                },
            };
            esp_ble_gatts_create_service(gatts_if, &service_id, GATTS_NUM_HANDLE_TEST);
            break;
        }
        case ESP_GATTS_CREATE_EVT: {
            s_service_handle = param->create.service_handle;
            esp_ble_gatts_start_service(s_service_handle);

            // 添加读写特征值
            esp_bt_uuid_t char_uuid = {
                .len = ESP_UUID_LEN_16,
                .uuid = { .uuid16 = GATTS_CHAR_UUID_TEST },
            };
            esp_ble_gatts_add_char(s_service_handle, &char_uuid,
                                   ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                   ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE,
                                   NULL, NULL);
            break;
        }
        case ESP_GATTS_ADD_CHAR_EVT:
            s_char_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "✅ GATT 服务与特征值创建成功 (Char Handle: 0x%04x)", s_char_handle);
            esp_ble_gap_config_adv_data(&adv_data);
            break;

        case ESP_GATTS_CONNECT_EVT:
            s_conn_id = param->connect.conn_id;
            s_is_connected = true;
            ESP_LOGI(TAG, "🎉 [手机已连接 BLE] Connection ID: %d", s_conn_id);
            break;

        case ESP_GATTS_DISCONNECT_EVT:
            s_is_connected = false;
            ESP_LOGW(TAG, "⚠️ 手机已断开连接，重新启动广播...");
            esp_ble_gap_start_advertising(&adv_params);
            break;

        case ESP_GATTS_READ_EVT: {
            esp_gatt_rsp_t rsp;
            memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
            rsp.attr_value.handle = param->read.handle;
            const char *reply_str = "ESP32_OK";
            rsp.attr_value.len = strlen(reply_str);
            memcpy(rsp.attr_value.value, reply_str, rsp.attr_value.len);
            esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
            ESP_LOGI(TAG, "📖 [手机发起读取] 回复字符串: %s", reply_str);
            break;
        }

        case ESP_GATTS_WRITE_EVT: {
            ESP_LOGI(TAG, "✍️ [手机写入数据] 长度: %d 字节, 内容: \033[32m%.*s\033[0m",
                     param->write.len, param->write.len, param->write.value);

            if (param->write.len > 0) {
                char cmd = param->write.value[0];
                if (cmd == '1') {
                    gpio_set_level(LED2_PIN, 1);
                    ESP_LOGI(TAG, "💡 收到 '1' ➔ 点亮 LED2");
                } else if (cmd == '0') {
                    gpio_set_level(LED2_PIN, 0);
                    ESP_LOGI(TAG, "💡 收到 '0' ➔ 熄灭 LED2");
                }
            }

            if (param->write.need_rsp) {
                esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            }
            break;
        }

        default:
            break;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 14 实验 2：GATT 特征值读写与数据透传     ");
    ESP_LOGI(TAG, "==================================================");

    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << LED2_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&led_conf);
    gpio_set_level(LED2_PIN, 0);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gap_set_device_name(DEVICE_NAME));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(0));

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
