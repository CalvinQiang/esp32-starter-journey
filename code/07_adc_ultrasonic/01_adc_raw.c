/**
 * 🌟 ESP32 物联网实战 —— 第 07 关 实验 1：ADC1 单次采样与估算电压测量 (Hello ADC)
 *    硬件连接: NTC 测温探头 (JP4) -> GPIO36 (ADC1_CH0 / VP)
 *    技术亮点: ESP-IDF 最新 ADC Oneshot 驱动、12位分辨率 (0~4095) 采样、毫伏电压换算
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

static const char *TAG = "EXP1_ADC_RAW";

#define NTC_ADC_UNIT        ADC_UNIT_1
#define NTC_ADC_CHANNEL     ADC_CHANNEL_0 // GPIO36 (VP)

void app_main(void)
{
    // 1. 初始化 ADC1 单元
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = NTC_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    // 2. 配置通道 0：12位分辨率 + 12dB 衰减 (测量范围 0 ~ 3.3V)
    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, NTC_ADC_CHANNEL, &chan_config));

    ESP_LOGI(TAG, "✅ ADC1 Channel 0 (GPIO36) 初始化成功！开始周期采集原始电压...");

    while (1) {
        int raw_val = 0;
        // 3. 读取 ADC 原始值 (0 ~ 4095)
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, NTC_ADC_CHANNEL, &raw_val));
        
        // 简易换算估算电压 (毫伏 mV)
        int voltage_mv = raw_val * 3300 / 4095;

        ESP_LOGI(TAG, "📊 ADC 原始数值: %4d | 估算电压: %4d mV", raw_val, voltage_mv);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
