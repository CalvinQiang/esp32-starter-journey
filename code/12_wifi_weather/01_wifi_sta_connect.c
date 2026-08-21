/**
 * 🌟 ESP32 物联网实战 —— 第 12 关 实验 1：Wi-Fi Station 模式健壮连接器 (带断线重连)
 * 
 * 🎯 学习目标：
 *    1. 搞懂 Wi-Fi STA (客户端模式) 与 AP (热点模式) 的本质区别；
 *    2. 掌握 ESP32 默认系统事件循环（Event Loop）与事件组（EventGroup）同步机制；
 *    3. 掌握 Wi-Fi 断线自动重连与成功获取 IP（`IP_EVENT_STA_GOT_IP`）标准处理流程。
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
#include "nvs_flash.h"

static const char *TAG = "EXP1_WIFI_CONNECT";

// ⚠️ 请在此处填入你身边的 2.4GHz Wi-Fi 账号和密码
#define EXAMPLE_ESP_WIFI_SSID      "YOUR_WIFI_SSID"
#define EXAMPLE_ESP_WIFI_PASS      "YOUR_WIFI_PASSWORD"
#define EXAMPLE_ESP_MAXIMUM_RETRY  5

// 事件标志位定义
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

/* Wi-Fi 与 IP 系统事件统一回调函数 */
static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "📡 Wi-Fi 硬件启动成功，正在发起连接...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "⚠️ Wi-Fi 连接失败，正在进行第 %d 次自动重试...", s_retry_num);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "❌ Wi-Fi 达到最大重试次数，连接放弃！请检查账号密码。");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "🎉 ==================================================");
        ESP_LOGI(TAG, "🎉 Wi-Fi 联网成功！分配到的局域网 IP: \033[32m" IPSTR "\033[0m", IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "🎉 网关 Gateway: " IPSTR ", 子网掩码: " IPSTR, IP2STR(&event->ip_info.gw), IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "🎉 ==================================================");
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    // 1. 初始化底层 TCP/IP 协议栈与系统事件循环
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 2. 初始化 Wi-Fi 驱动底层
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 3. 注册 Wi-Fi 事件与 IP 事件监听器
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        NULL));

    // 4. 配置目标 SSID 与密码
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "⏳ 正在等待 Wi-Fi 握手与 IP 获取...");

    // 5. 阻塞等待连接结果 (成功或失败)
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "✅ [网络就绪] ESP32 已成功加入互联网！");
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "❌ [连接失败] 无法连入目标 Wi-Fi SSID: %s", EXAMPLE_ESP_WIFI_SSID);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 12 实验 1：Wi-Fi Station 模式健壮连接器 ");
    ESP_LOGI(TAG, "==================================================");

    // 初始化 NVS (Wi-Fi 协议栈内部需要保存校准数据)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "🌐 Wi-Fi 连接状态良好，网络就绪中...");
    }
}
