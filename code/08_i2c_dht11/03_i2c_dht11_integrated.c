/**
 * 🌟 ESP32 物联网实战 —— 第 08 关 实验 3：I2C 触摸在线监测与 DHT11 双总线气象站
 * 
 * 🎯 学习目标：
 *    1. 整合 I2C 硬件通信与 1-Wire 单总线两种不同的时序协议；
 *    2. 构建 FreeRTOS 双并发任务架构：
 *       - 【任务 A】：DHT11 气象数据周期性采集与有效性校验；
 *       - 【任务 B】：I2C 总线心跳探测与 CST816S 触摸芯片在线状态巡检；
 *    3. 掌握嵌入式多外设异构总线并发通信的最佳工程架构。
 * 
 * 📌 硬件连接速查：
 *    - I2C SCL:     GPIO22 (CST816S 触摸屏时钟)
 *    - I2C SDA:     GPIO23 (CST816S 触摸屏数据)
 *    - DHT11 DATA:  GPIO25 (JP1 接口温湿度数据线)
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "driver/i2c_master.h"

static const char *TAG = "LEVEL08_INTEGRATED";

/* I2C 引脚与参数 */
#define I2C_PORT_NUM        I2C_NUM_0
#define I2C_SCL_PIN         GPIO_NUM_22
#define I2C_SDA_PIN         GPIO_NUM_23
#define CST816S_I2C_ADDR    0x15

/* DHT11 引脚 */
#define DHT11_PIN           GPIO_NUM_25

static i2c_master_bus_handle_t s_i2c_bus = NULL;

/* ------------------- I2C 驱动部分 ------------------- */
static void i2c_bus_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_PORT_NUM,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &s_i2c_bus));
    ESP_LOGI(TAG, "✅ [I2C 总线] 驱动初始化完成 (SCL: GPIO%d, SDA: GPIO%d)", I2C_SCL_PIN, I2C_SDA_PIN);
}

/* ------------------- DHT11 驱动部分 ------------------- */
static int64_t dht11_wait_level(int target_level, int timeout_us)
{
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(DHT11_PIN) != target_level) {
        if (esp_timer_get_time() - start > timeout_us) return -1;
    }
    return esp_timer_get_time() - start;
}

static bool dht11_read(float *temperature, float *humidity)
{
    uint8_t data[5] = {0};

    // 1. 发送 20ms 起始低脉冲
    gpio_set_direction(DHT11_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(DHT11_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(20));

    // 2. 拉高 30us
    gpio_set_level(DHT11_PIN, 1);
    esp_rom_delay_us(30);

    // 3. 切换输入
    gpio_set_direction(DHT11_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(DHT11_PIN, GPIO_PULLUP_ONLY);

    // 4. 握手检测
    if (dht11_wait_level(0, 100) < 0) return false;
    if (dht11_wait_level(1, 100) < 0) return false;
    if (dht11_wait_level(0, 100) < 0) return false;

    // 5. 读取 40 bits
    for (int i = 0; i < 40; i++) {
        if (dht11_wait_level(1, 100) < 0) return false;
        int64_t high_start = esp_timer_get_time();
        if (dht11_wait_level(0, 100) < 0) return false;
        int64_t duration = esp_timer_get_time() - high_start;

        data[i / 8] <<= 1;
        if (duration > 40) {
            data[i / 8] |= 1;
        }
    }

    // 6. 校验和 Checksum
    if (((data[0] + data[1] + data[2] + data[3]) & 0xFF) != data[4]) {
        return false;
    }

    *humidity = (float)data[0] + (float)data[1] * 0.1f;
    *temperature = (float)data[2] + (float)data[3] * 0.1f;
    return true;
}

/* ------------------- FreeRTOS 双并发任务 ------------------- */

/* 任务 A：DHT11 单总线微气象监测 */
static void task_dht11_monitor(void *pvParameters)
{
    ESP_LOGI(TAG, "🌡️ [任务 A: DHT11 采集] 启动...");
    float temp = 0.0f, humi = 0.0f;

    while (1) {
        if (dht11_read(&temp, &humi)) {
            ESP_LOGI(TAG, "📊 [气象站报告] 🌡️ 环境温度: \033[32m%4.1f ℃\033[0m | 💧 相对湿度: \033[36m%4.1f %%\033[0m", temp, humi);
        } else {
            ESP_LOGW(TAG, "⚠️ [气象站报告] DHT11 采样超时 (请确认 JP1 模块接入良好)");
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* 任务 B：I2C 设备心跳与健康巡检 */
static void task_i2c_health_check(void *pvParameters)
{
    ESP_LOGI(TAG, "🔍 [任务 B: I2C 巡检] 启动...");

    while (1) {
        esp_err_t ret = i2c_master_probe(s_i2c_bus, CST816S_I2C_ADDR, 50);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "💚 [I2C 巡检] CST816S 触摸芯片 (0x%02X) ➔ \033[32m[在线通信正常 ACK]\033[0m", CST816S_I2C_ADDR);
        } else {
            ESP_LOGW(TAG, "💛 [I2C 巡检] CST816S 触摸芯片 (0x%02X) ➔ [无应答 NACK / 休眠中]", CST816S_I2C_ADDR);
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 08 启动：I2C 触摸巡检与 DHT11 气象雷达   ");
    ESP_LOGI(TAG, "==================================================");

    // 1. 初始化 I2C 总线
    i2c_bus_init();

    // 2. 创建双并发任务
    xTaskCreatePinnedToCore(task_dht11_monitor, "dht11_task", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(task_i2c_health_check, "i2c_task", 4096, NULL, 4, NULL, 1);
}
