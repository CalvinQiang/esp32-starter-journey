/**
 * 🌟 ESP32 物联网实战 —— 第 07 关 实验 4：双任务温声融合环境监测雷达 (终极综合)
 *    硬件连接: NTC 测温 (GPIO36), 超声波 Trig(GPIO32), Echo(GPIO33)
 *    技术亮点: FreeRTOS 双任务并发、ADC 实时测温、空气声速动态温度补偿 (v = 331.3 + 0.606 * T)
 */
#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_adc/adc_oneshot.h"

static const char *TAG = "LEVEL07_RADAR";

/* 引脚定义 */
#define NTC_ADC_UNIT        ADC_UNIT_1
#define NTC_ADC_CHANNEL     ADC_CHANNEL_0 // GPIO36 (VP)
#define ULTRASONIC_TRIG_PIN GPIO_NUM_32
#define ULTRASONIC_ECHO_PIN GPIO_NUM_33

/* NTC 算法常数 */
#define NTC_B_VALUE         3950.0f
#define NTC_R_SERIES        10000.0f
#define NTC_R25             10000.0f
#define NTC_T25_KELVIN      298.15f

/* 全局共享数据 (多任务安全访问) */
static volatile float g_current_temperature = 25.0f; // 默认 25℃
static adc_oneshot_unit_handle_t s_adc_handle = NULL;

/* 1. NTC 测温驱动初始化与采样 */
static void ntc_sensor_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config = { .unit_id = NTC_ADC_UNIT };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &s_adc_handle));

    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, NTC_ADC_CHANNEL, &chan_config));
}

static float ntc_get_temperature(void)
{
    int raw_val = 0;
    adc_oneshot_read(s_adc_handle, NTC_ADC_CHANNEL, &raw_val);
    if (raw_val <= 0 || raw_val >= 4095) return 25.0f;

    float v_ratio = (float)raw_val / (4095.0f - (float)raw_val);
    float r_ntc = NTC_R_SERIES * v_ratio;
    float kelvin = 1.0f / ((1.0f / NTC_T25_KELVIN) + (log(r_ntc / NTC_R25) / NTC_B_VALUE));
    return kelvin - 273.15f;
}

/* 2. HC-SR04 超声波测距初始化与微秒测距 */
static void ultrasonic_sensor_init(void)
{
    gpio_config_t trig_conf = {
        .pin_bit_mask = (1ULL << ULTRASONIC_TRIG_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&trig_conf);
    gpio_set_level(ULTRASONIC_TRIG_PIN, 0);

    gpio_config_t echo_conf = {
        .pin_bit_mask = (1ULL << ULTRASONIC_ECHO_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&echo_conf);
}

static int64_t ultrasonic_get_raw_duration_us(void)
{
    gpio_set_level(ULTRASONIC_TRIG_PIN, 0);
    esp_rom_delay_us(2);
    gpio_set_level(ULTRASONIC_TRIG_PIN, 1);
    esp_rom_delay_us(10);
    gpio_set_level(ULTRASONIC_TRIG_PIN, 0);

    int64_t start_wait = esp_timer_get_time();
    while (gpio_get_level(ULTRASONIC_ECHO_PIN) == 0) {
        if (esp_timer_get_time() - start_wait > 30000) return -1;
    }

    int64_t echo_start = esp_timer_get_time();
    while (gpio_get_level(ULTRASONIC_ECHO_PIN) == 1) {
        if (esp_timer_get_time() - echo_start > 30000) return -1;
    }
    int64_t echo_end = esp_timer_get_time();

    return echo_end - echo_start;
}

/* 3. 后台任务：环境温度连续监测任务 (每秒更新一次全局气温) */
static void task_temperature_monitor(void *pvParameters)
{
    ESP_LOGI(TAG, "🌡️ [任务 A] 温度监测后台任务启动...");
    while (1) {
        float temp = ntc_get_temperature();
        g_current_temperature = temp; // 实时更新全局气温
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* 4. 前台任务：温声融合测距雷达任务 (每 200ms 刷新一次高精度物距) */
static void task_radar_ranging(void *pvParameters)
{
    ESP_LOGI(TAG, "📡 [任务 B] 温声融合雷达测距任务启动...");
    while (1) {
        int64_t duration_us = ultrasonic_get_raw_duration_us();
        
        // 核心亮点：利用当前环境温度，动态计算真实空气声速 (米/秒)
        float sound_speed = 331.3f + 0.606f * g_current_temperature;
        
        if (duration_us > 0) {
            // S (cm) = (t (us) / 1000000.0) * (sound_speed * 100.0) / 2.0
            float distance_cm = ((float)duration_us * sound_speed) / 20000.0f;
            
            ESP_LOGI(TAG, "[温声融合雷达] 🌡️ 气温: \033[32m%5.2f ℃\033[0m | 🔊 修正声速: \033[33m%6.2f m/s\033[0m | 📏 目标物距: \033[36m%6.1f cm\033[0m (%4.2f m)",
                     g_current_temperature, sound_speed, distance_cm, distance_cm / 100.0f);
        } else {
            ESP_LOGW(TAG, "[温声融合雷达] 🌡️ 气温: %5.2f ℃ | ⚠️ 前方无障碍物或超出量程", g_current_temperature);
        }
        
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 07 启动：温声融合高精度雷达测距仪       ");
    ESP_LOGI(TAG, "==================================================");

    // 1. 初始化两路硬件驱动
    ntc_sensor_init();
    ultrasonic_sensor_init();

    // 2. 启动 FreeRTOS 双任务并发协作
    xTaskCreate(task_temperature_monitor, "task_temp", 3072, NULL, 2, NULL);
    xTaskCreate(task_radar_ranging,       "task_radar", 3584, NULL, 3, NULL);
}
