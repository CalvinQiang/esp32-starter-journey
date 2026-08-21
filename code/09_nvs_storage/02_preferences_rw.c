/**
 * 🌟 ESP32 物联网实战 —— 第 09 关 实验 2：用户偏好设置与 Wi-Fi 账号密码持久化
 * 
 * 🎯 学习目标：
 *    1. 掌握在 NVS 中存储字符串（String）与整型配置的方法；
 *    2. 掌握动态获取字符串长度与缓冲区安全读取技巧；
 *    3. 模拟物联网设备保存“Wi-Fi 账号、密码、屏幕亮度、深色模式”全套用户偏好。
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "EXP2_USER_PREFS";

#define NVS_NAMESPACE       "user_config"
#define KEY_WIFI_SSID       "wifi_ssid"
#define KEY_WIFI_PASS       "wifi_pass"
#define KEY_BRIGHTNESS      "brightness"
#define KEY_DARK_MODE       "dark_mode"

static void save_mock_user_preferences(nvs_handle_t handle)
{
    ESP_LOGI(TAG, "✍️ 正在将用户自定义配置写入 NVS Flash...");

    // 1. 写入字符串类型数据 (Wi-Fi SSID 和 密码)
    ESP_ERROR_CHECK(nvs_set_str(handle, KEY_WIFI_SSID, "My_Home_WiFi_5G"));
    ESP_ERROR_CHECK(nvs_set_str(handle, KEY_WIFI_PASS, "SuperSecretPassword123"));

    // 2. 写入整型数据 (屏幕亮度 85%, 深色模式 开启)
    ESP_ERROR_CHECK(nvs_set_i32(handle, KEY_BRIGHTNESS, 85));
    ESP_ERROR_CHECK(nvs_set_i32(handle, KEY_DARK_MODE, 1));

    // 3. 必须调用 commit 提交持久化
    ESP_ERROR_CHECK(nvs_commit(handle));
    ESP_LOGI(TAG, "✅ 用户配置已成功写入并持久化至 Flash！");
}

static void read_user_preferences(nvs_handle_t handle)
{
    ESP_LOGI(TAG, "📖 正在从 Flash 中读取用户配置参数：");

    // 1. 读取字符串：先获取实际字符串字节长度，再安全读取
    size_t required_size = 0;
    char ssid[64] = {0};
    char pass[64] = {0};

    // 读取 Wi-Fi SSID
    if (nvs_get_str(handle, KEY_WIFI_SSID, NULL, &required_size) == ESP_OK) {
        nvs_get_str(handle, KEY_WIFI_SSID, ssid, &required_size);
    }
    // 读取 Wi-Fi Password
    if (nvs_get_str(handle, KEY_WIFI_PASS, NULL, &required_size) == ESP_OK) {
        nvs_get_str(handle, KEY_WIFI_PASS, pass, &required_size);
    }

    // 2. 读取整型数据
    int32_t brightness = 50; // 缺省默认值 50%
    int32_t dark_mode = 0;   // 缺省默认值 关闭
    nvs_get_i32(handle, KEY_BRIGHTNESS, &brightness);
    nvs_get_i32(handle, KEY_DARK_MODE, &dark_mode);

    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "📶 [网络配置] Wi-Fi 名称: \033[36m%s\033[0m", ssid[0] ? ssid : "(未配置)");
    ESP_LOGI(TAG, "🔑 [网络配置] Wi-Fi 密码: \033[36m%s\033[0m", pass[0] ? pass : "(未配置)");
    ESP_LOGI(TAG, "💡 [显示偏好] 屏幕亮度: \033[32m%ld %%\033[0m", (long)brightness);
    ESP_LOGI(TAG, "🌙 [界面偏好] 深色模式: \033[33m%s\033[0m", dark_mode ? "已开启 (ON)" : "已关闭 (OFF)");
    ESP_LOGI(TAG, "==================================================");
}

void app_main(void)
{
    // 初始化 NVS 分区
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // 打开命名空间
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle));

    // 检查是否存在历史配置，不存在则写入默认模拟数据
    size_t size = 0;
    if (nvs_get_str(nvs_handle, KEY_WIFI_SSID, NULL, &size) == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "💡 首次检测到未保存配置，正在初始化写入默认偏好...");
        save_mock_user_preferences(nvs_handle);
    }

    // 从 Flash 中读出配置并打印
    read_user_preferences(nvs_handle);

    nvs_close(nvs_handle);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
