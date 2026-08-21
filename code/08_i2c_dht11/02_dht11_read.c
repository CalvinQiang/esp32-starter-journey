/**
 * 🌟 ESP32 物联网实战 —— 第 08 关 实验 2：DHT11 单总线微秒级温湿度采样
 * 
 * 🎯 学习目标：
 *    1. 掌握 1-Wire（单总线）双向时序驱动原理；
 *    2. 掌握 GPIO 动态输入/输出模式切换与微秒级精准脉宽捕获；
 *    3. 学习 40-bit 数据包（湿度整数/小数、温度整数/小数、校验和）解码与 Checksum 校验。
 * 
 * 📌 硬件连接：
 *    - DHT11 DATA: GPIO25 (JP1 接口 Pin 2)
 *    - DHT11 VCC:  +3.3V (JP1 接口 Pin 1)
 *    - DHT11 GND:  GND   (JP1 接口 Pin 4)
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

static const char *TAG = "EXP2_DHT11";

#define DHT11_PIN   GPIO_NUM_25

typedef struct {
    float temperature;
    float humidity;
} dht11_data_t;

/**
 * @brief 等待 GPIO 电平变为目标电平，带微秒超时保护
 * @return 持续时间 (微秒)，若超时返回 -1
 */
static int64_t dht11_wait_level(int target_level, int timeout_us)
{
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(DHT11_PIN) != target_level) {
        if (esp_timer_get_time() - start > timeout_us) {
            return -1; // 超时
        }
    }
    return esp_timer_get_time() - start;
}

/**
 * @brief 读取一次 DHT11 数据 (40 bits)
 */
static bool dht11_read(dht11_data_t *result)
{
    uint8_t data[5] = {0};

    // 1. ESP32 发送起始信号：拉低至少 18ms
    gpio_set_direction(DHT11_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(DHT11_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(20)); // 拉低 20ms

    // 2. ESP32 拉高 20~40us，准备接收
    gpio_set_level(DHT11_PIN, 1);
    esp_rom_delay_us(30);

    // 3. 切换为输入模式，开启内部上拉
    gpio_set_direction(DHT11_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(DHT11_PIN, GPIO_PULLUP_ONLY);

    // 4. 等待 DHT11 响应握手信号 (80us 低电平 + 80us 高电平)
    if (dht11_wait_level(0, 100) < 0) return false; // 等待低电平握手
    if (dht11_wait_level(1, 100) < 0) return false; // 等待高电平握手
    if (dht11_wait_level(0, 100) < 0) return false; // 等待高电平结束开始传数据

    // 5. 连续接收 40 位数据 (5 个字节)
    for (int i = 0; i < 40; i++) {
        // 每个 bit 开始前都有 50us 低电平
        if (dht11_wait_level(1, 100) < 0) return false;

        // 测量高电平持续时间：26~28us 为 '0'，70us 为 '1'
        int64_t high_start = esp_timer_get_time();
        if (dht11_wait_level(0, 100) < 0) return false;
        int64_t high_duration = esp_timer_get_time() - high_start;

        int byte_idx = i / 8;
        data[byte_idx] <<= 1;
        if (high_duration > 40) { // 超过 40us 判定为 1
            data[byte_idx] |= 1;
        }
    }

    // 6. Checksum 校验和比对 (前 4 字节之和 == 第 5 字节)
    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4]) {
        ESP_LOGW(TAG, "❌ Checksum 校验失败: 期望 0x%02X, 收到 0x%02X", checksum, data[4]);
        return false;
    }

    // 7. 解析温湿度数值 (DHT11 data[0]=湿度整数, data[2]=温度整数)
    result->humidity = (float)data[0] + (float)data[1] * 0.1f;
    result->temperature = (float)data[2] + (float)data[3] * 0.1f;
    return true;
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 08 实验 2：DHT11 单总线温湿度采样       ");
    ESP_LOGI(TAG, "==================================================");

    dht11_data_t sensor_data;

    while (1) {
        if (dht11_read(&sensor_data)) {
            ESP_LOGI(TAG, "🌡️ 传感器读数成功 ➔ 温度: \033[32m%4.1f ℃\033[0m | 相对湿度: \033[36m%4.1f %%\033[0m",
                     sensor_data.temperature, sensor_data.humidity);
        } else {
            ESP_LOGW(TAG, "⚠️ DHT11 采样超时或未接入 (请检查 JP1 接线)");
        }
        
        // DHT11 物理刷新间隔建议不少于 2 秒
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
