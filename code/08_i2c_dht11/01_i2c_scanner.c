/**
 * 🌟 ESP32 物联网实战 —— 第 08 关 实验 1：I2C 总线设备扫描器 (I2C Scanner)
 * 
 * 🎯 学习目标：
 *    1. 掌握 ESP-IDF v6 最新官方 I2C Master 驱动 (`driver/i2c_master.h`) 的初始化流程；
 *    2. 理解 I2C SCL/SDA 引脚开漏上拉（Open-Drain）与 ACK 应答探测原理；
 *    3. 编写通用 I2C Scanner，自动探测扫描板载从机（如 CST816S 触摸芯片，默认地址 0x15）。
 * 
 * 📌 硬件连接：
 *    - I2C SCL: GPIO22 (板载上拉)
 *    - I2C SDA: GPIO23 (板载上拉)
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

static const char *TAG = "EXP1_I2C_SCANNER";

#define I2C_PORT_NUM        I2C_NUM_0
#define I2C_SCL_PIN         GPIO_NUM_22
#define I2C_SDA_PIN         GPIO_NUM_23
#define I2C_FREQ_HZ         100000       // 标准模式 100 kHz

static i2c_master_bus_handle_t s_bus_handle = NULL;

static void i2c_master_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_PORT_NUM,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true, // 启用 ESP32 内部弱上拉
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &s_bus_handle));
    ESP_LOGI(TAG, "✅ I2C Master 总线初始化成功 (SCL: GPIO%d, SDA: GPIO%d, 速率: %d Hz)",
             I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ_HZ);
}

static void i2c_scan_devices(void)
{
    ESP_LOGI(TAG, "🔍 开始扫描 I2C 总线 (地址范围: 0x01 ~ 0x7F)...");
    
    int device_count = 0;

    printf("\n     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");
    printf("00:         ");

    for (int addr = 1; addr < 0x78; addr++) {
        if (addr % 16 == 0) {
            printf("\n%02x: ", addr);
        }

        // 使用 ESP-IDF v6 专用 probe 函数探测是否有从机返回 ACK
        esp_err_t ret = i2c_master_probe(s_bus_handle, addr, 50);

        if (ret == ESP_OK) {
            printf("\033[32m%02x \033[0m", addr); // 绿色高亮显示发现的设备
            device_count++;
        } else {
            printf("-- ");
        }
    }
    printf("\n\n");

    if (device_count == 0) {
        ESP_LOGW(TAG, "⚠️ 未在 I2C 总线上探测到任何设备！请检查接线或跳线帽。");
    } else {
        ESP_LOGI(TAG, "🎉 扫描完成！共发现 \033[32m%d\033[0m 个 I2C 设备：", device_count);
        // 特别解析板载常见芯片
        for (int addr = 1; addr < 0x78; addr++) {
            if (i2c_master_probe(s_bus_handle, addr, 50) == ESP_OK) {
                if (addr == 0x15) {
                    ESP_LOGI(TAG, "   📍 发现地址 [0x%02X] ➔ \033[36mCST816S 电容触摸屏芯片\033[0m", addr);
                } else if (addr == 0x68 || addr == 0x69) {
                    ESP_LOGI(TAG, "   📍 发现地址 [0x%02X] ➔ MPU6050 / 六轴传感器", addr);
                } else if (addr == 0x3C || addr == 0x3D) {
                    ESP_LOGI(TAG, "   📍 发现地址 [0x%02X] ➔ SSD1306 OLED 显示屏", addr);
                } else {
                    ESP_LOGI(TAG, "   📍 发现未知设备地址 [0x%02X]", addr);
                }
            }
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 08 实验 1：I2C 硬件总线地址扫描器       ");
    ESP_LOGI(TAG, "==================================================");

    i2c_master_init();

    while (1) {
        i2c_scan_devices();
        ESP_LOGI(TAG, "⏳ 5 秒后重新执行总线扫描...\n");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
