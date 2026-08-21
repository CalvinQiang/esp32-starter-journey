/**
 * 🌟 ESP32 物联网实战 —— 第 14 关 实验 1：BLE 低功耗蓝牙广播与手机扫描发现 (BLE Beacon)
 * 
 * 🎯 学习目标：
 *    1. 搞懂 BLE（低功耗蓝牙）与经典蓝牙（Classic BT）的区别与超低功耗优势；
 *    2. 掌握 ESP32 蓝牙控制器（BT Controller）与 Bluedroid 协议栈初始化流程；
 *    3. 配置 GAP（通用访问协议）广播参数与广播名，让手机蓝牙 App 秒级发现！
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"

static const char *TAG = "EXP1_BLE_ADV";

#define DEVICE_NAME "ESP32-Journey-Beacon"

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20, // 广播最小间隔 (20ms * 0.625ms = 12.5ms)
    .adv_int_max        = 0x40, // 广播最大间隔 (40ms * 0.625ms = 25ms)
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = true,
    .min_interval        = 0x0006,
    .max_interval        = 0x0010,
    .appearance          = 0x00,
    .manufacturer_len    = 0,
    .p_manufacturer_data = NULL,
    .service_data_len    = 0,
    .p_service_data      = NULL,
    .service_uuid_len    = 0,
    .p_service_uuid      = NULL,
    .flag                = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            ESP_LOGI(TAG, "📡 广播数据设置完成，正在启动 BLE 广播...");
            esp_ble_gap_start_advertising(&adv_params);
            break;
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "🎉 [BLE 广播中] 请打开手机微信小程序或 LightBlue App 搜索: \033[32m%s\033[0m", DEVICE_NAME);
            } else {
                ESP_LOGE(TAG, "❌ BLE 广播启动失败, 错误码: %d", param->adv_start_cmpl.status);
            }
            break;
        default:
            break;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 14 实验 1：BLE 广播与手机扫描发现       ");
    ESP_LOGI(TAG, "==================================================");

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

    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gap_set_device_name(DEVICE_NAME));
    ESP_ERROR_CHECK(esp_ble_gap_config_adv_data(&adv_data));

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
