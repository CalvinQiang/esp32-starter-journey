/**
 * 🌟 ESP32 物联网实战 —— 第 14 关 实验 3：手机 BLE 遥控器与主动 Notify 状态推送 (综合大工程)
 * 
 * 🎯 学习目标：
 *    1. 实现完整 BLE 双向交互：手机下发指令写特征值 + 单片机主动 Notify 向上推送；
 *    2. 监听板载用户按键 SW3 (GPIO39)，一旦按下，立刻通过 BLE Notify 推送按键事件至手机；
 *    3. 支持通过微信小程序或手机蓝牙调试器实现免配网的近场智能家居中控！
 * 
 * 📌 硬件接口：
 *    - 板载绿色 LED2: GPIO27
 *    - 用户按键 SW3: GPIO39 (低电平按下)
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

static const char *TAG = "EXP3_BLE_REMOTE";

#define LED2_PIN               GPIO_NUM_27
#define BUTTON_PIN             GPIO_NUM_39
#define DEVICE_NAME            "ESP32-Smart-Remote"

#define GATTS_SERVICE_UUID_HUB      0xFFE0
#define GATTS_CHAR_UUID_HUB         0xFFE1
#define GATTS_NUM_HANDLE_HUB        4

static uint16_t s_service_handle = 0;
static uint16_t s_char_handle = 0;
static esp_gatt_if_t s_gatts_if = 0;
static uint16_t s_conn_id = 0;
static bool s_is_connected = false;
static bool s_led_state = false;

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
            ESP_LOGI(TAG, "📡 [BLE 遥控器广播中] 设备名: \033[36m%s\033[0m", DEVICE_NAME);
            break;
        default:
            break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
        case ESP_GATTS_REG_EVT: {
            s_gatts_if = gatts_if;
            esp_gatt_srvc_id_t service_id = {
                .is_primary = true,
                .id = {
                    .inst_id = 0x00,
                    .uuid = {
                        .len = ESP_UUID_LEN_16,
                        .uuid = { .uuid16 = GATTS_SERVICE_UUID_HUB },
                    },
                },
            };
            esp_ble_gatts_create_service(gatts_if, &service_id, GATTS_NUM_HANDLE_HUB);
            break;
        }
        case ESP_GATTS_CREATE_EVT: {
            s_service_handle = param->create.service_handle;
            esp_ble_gatts_start_service(s_service_handle);

            // 支持 Read, Write, Notify 属性
            esp_bt_uuid_t char_uuid = {
                .len = ESP_UUID_LEN_16,
                .uuid = { .uuid16 = GATTS_CHAR_UUID_HUB },
            };
            esp_ble_gatts_add_char(s_service_handle, &char_uuid,
                                   ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                   ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                   NULL, NULL);
            break;
        }
        case ESP_GATTS_ADD_CHAR_EVT:
            s_char_handle = param->add_char.attr_handle;
            ESP_LOGI(TAG, "✅ BLE 服务与双向透传特征值就绪 (Char Handle: 0x%04x)", s_char_handle);
            esp_ble_gap_config_adv_data(&adv_data);
            break;

        case ESP_GATTS_CONNECT_EVT:
            s_conn_id = param->connect.conn_id;
            s_is_connected = true;
            ESP_LOGI(TAG, "🎉 [手机已连接 BLE 遥控器] Conn ID: %d", s_conn_id);
            break;

        case ESP_GATTS_DISCONNECT_EVT:
            s_is_connected = false;
            ESP_LOGW(TAG, "⚠️ 手机已断开，重新广播...");
            esp_ble_gap_start_advertising(&adv_params);
            break;

        case ESP_GATTS_WRITE_EVT: {
            ESP_LOGI(TAG, "📥 [收到手机控制数据] \033[32m%.*s\033[0m",
                     param->write.len, param->write.value);

            if (param->write.len > 0) {
                if (param->write.value[0] == '1') {
                    s_led_state = true;
                    gpio_set_level(LED2_PIN, 1);
                    ESP_LOGI(TAG, "💡 手机指令 ➔ 点亮 LED2");
                } else if (param->write.value[0] == '0') {
                    s_led_state = false;
                    gpio_set_level(LED2_PIN, 0);
                    ESP_LOGI(TAG, "💡 手机指令 ➔ 熄灭 LED2");
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

/* 独立任务：监听板载 SW3 按键，按下时主动 Notify 推送给手机 */
static void button_notify_task(void *pvParameters)
{
    int click_count = 0;
    while (1) {
        if (gpio_get_level(BUTTON_PIN) == 0) { // 按下
            vTaskDelay(pdMS_TO_TICKS(20));     // 消抖
            if (gpio_get_level(BUTTON_PIN) == 0) {
                click_count++;
                ESP_LOGI(TAG, "🔘 检测到 SW3 按下 (第 %d 次)", click_count);

                if (s_is_connected && s_char_handle != 0) {
                    char notify_msg[64];
                    snprintf(notify_msg, sizeof(notify_msg), "SW3_CLICK_%d", click_count);
                    esp_ble_gatts_send_indicate(s_gatts_if, s_conn_id, s_char_handle,
                                               strlen(notify_msg), (uint8_t *)notify_msg, false);
                    ESP_LOGI(TAG, "📤 [主动 Notify 推送给手机] ➔ %s", notify_msg);
                }

                while (gpio_get_level(BUTTON_PIN) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 14 实验 3：BLE 智能遥控与主动 Notify 推送 ");
    ESP_LOGI(TAG, "==================================================");

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED2_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);
    gpio_set_level(LED2_PIN, 0);

    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
    };
    gpio_config(&btn_conf);

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

    xTaskCreate(button_notify_task, "btn_notify", 3072, NULL, 5, NULL);
}
