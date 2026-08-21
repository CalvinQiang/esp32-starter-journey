/**
 * 🌟 ESP32 物联网实战 —— 第 07 关 实验 2：NTC 热敏电阻精准摄氏度测量
 *    硬件连接: NTC 测温探头 (JP4) -> GPIO36 (ADC1_CH0)
 *    技术亮点: 串联分压电阻计算、热力学 B 值方程、精准摄氏度换算
 */
#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

static const char *TAG = "EXP2_NTC_TEMP";

#define NTC_ADC_UNIT        ADC_UNIT_1
#define NTC_ADC_CHANNEL     ADC_CHANNEL_0 // GPIO36
#define NTC_B_VALUE         3950.0f       // 热敏电阻 B 常数
#define NTC_R_SERIES        10000.0f      // 板载串联分压电阻 10kΩ
#define NTC_R25             10000.0f      // 25℃ 时的基准阻值 10kΩ
#define NTC_T25_KELVIN      298.15f       // 25℃ 对应的开尔文温度 (273.15 + 25)

static float read_ntc_temperature(adc_oneshot_unit_handle_t handle)
{
    int raw_val = 0;
    adc_oneshot_read(handle, NTC_ADC_CHANNEL, &raw_val);
    if (raw_val <= 0 || raw_val >= 4095) return -999.0f; // 异常保护

    // 1. 根据分压电路计算 NTC 实时阻值
    float v_ratio = (float)raw_val / (4095.0f - (float)raw_val);
    float r_ntc = NTC_R_SERIES * v_ratio;

    // 2. 套用 B 值方程计算开尔文温度
    float kelvin = 1.0f / ( (1.0f / NTC_T25_KELVIN) + (log(r_ntc / NTC_R25) / NTC_B_VALUE) );
    
    // 3. 换算为人类熟悉的摄氏度
    return kelvin - 273.15f;
}

void app_main(void)
{
    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_config = { .unit_id = NTC_ADC_UNIT };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, NTC_ADC_CHANNEL, &chan_config));

    ESP_LOGI(TAG, "🌡️ NTC 测温系统已启动，请用手指捏住探头测试升温效果！");

    while (1) {
        float temp_c = read_ntc_temperature(adc_handle);
        ESP_LOGI(TAG, "🌡️ 当前环境温度: \033[32m%.2f ℃\033[0m", temp_c);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
