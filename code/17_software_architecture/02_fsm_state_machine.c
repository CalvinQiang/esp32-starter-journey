/**
 * 🌟 ESP32 物联网实战 —— 第 17 关 实验 2：有限状态机 (FSM) 系统中枢架构设计
 * 
 * 🎯 学习目标：
 *    1. 理解为什么复杂嵌入式工程必须使用有限状态机（FSM）消除混乱的 `if-else`；
 *    2. 定义清晰的状态集合（INIT ➔ STANDBY ➔ RUNNING ➔ ALARM）；
 *    3. 掌握基于事件驱动的状态转移模型与状态进入/退出回调动作。
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "EXP2_FSM";

/* 1. 系统状态定义 */
typedef enum {
    STATE_BOOTING = 0,   // 开机自检中
    STATE_STANDBY,       // 待机守候
    STATE_ACTIVE,        // 活跃工作中
    STATE_ALARM,         // 异常报警中
    STATE_MAX
} sys_state_t;

/* 2. 外部触发事件定义 */
typedef enum {
    EVT_SELF_TEST_OK = 0, // 自检完成
    EVT_USER_ACTIVITY,    // 用户触发活动
    EVT_TIMEOUT_IDLE,     // 超时无操作
    EVT_SENSOR_OVERLIMIT, // 传感器越限异常
    EVT_ALARM_CLEAR       // 报警解除
} sys_event_t;

static const char *state_to_str(sys_state_t s) {
    switch (s) {
        case STATE_BOOTING: return "BOOTING (自检)";
        case STATE_STANDBY: return "STANDBY (待机)";
        case STATE_ACTIVE:  return "ACTIVE (活跃)";
        case STATE_ALARM:   return "ALARM (报警)";
        default: return "UNKNOWN";
    }
}

static sys_state_t s_current_state = STATE_BOOTING;

/* 状态转移核心处理引擎 */
static void fsm_handle_event(sys_event_t event)
{
    sys_state_t prev_state = s_current_state;

    switch (s_current_state) {
        case STATE_BOOTING:
            if (event == EVT_SELF_TEST_OK) {
                s_current_state = STATE_STANDBY;
            }
            break;

        case STATE_STANDBY:
            if (event == EVT_USER_ACTIVITY) {
                s_current_state = STATE_ACTIVE;
            } else if (event == EVT_SENSOR_OVERLIMIT) {
                s_current_state = STATE_ALARM;
            }
            break;

        case STATE_ACTIVE:
            if (event == EVT_TIMEOUT_IDLE) {
                s_current_state = STATE_STANDBY;
            } else if (event == EVT_SENSOR_OVERLIMIT) {
                s_current_state = STATE_ALARM;
            }
            break;

        case STATE_ALARM:
            if (event == EVT_ALARM_CLEAR) {
                s_current_state = STATE_STANDBY;
            }
            break;

        default:
            break;
    }

    if (prev_state != s_current_state) {
        ESP_LOGI(TAG, "🔄 [状态转移] \033[33m%s\033[0m ➔➔➔ \033[32m%s\033[0m", 
                 state_to_str(prev_state), state_to_str(s_current_state));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 17 实验 2：有限状态机 FSM 中枢引擎     ");
    ESP_LOGI(TAG, "==================================================");

    ESP_LOGI(TAG, "📍 初始状态: %s", state_to_str(s_current_state));

    // 模拟生命周期事件触发序列
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "\n⚡ [事件发生] ➔ 自检通过 (EVT_SELF_TEST_OK)");
    fsm_handle_event(EVT_SELF_TEST_OK);

    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "\n⚡ [事件发生] ➔ 检测到用户触摸屏幕 (EVT_USER_ACTIVITY)");
    fsm_handle_event(EVT_USER_ACTIVITY);

    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "\n⚡ [事件发生] ➔ 气温超标 40°C 报警 (EVT_SENSOR_OVERLIMIT)");
    fsm_handle_event(EVT_SENSOR_OVERLIMIT);

    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "\n⚡ [事件发生] ➔ 人工确认解除警报 (EVT_ALARM_CLEAR)");
    fsm_handle_event(EVT_ALARM_CLEAR);

    ESP_LOGI(TAG, "\n🎉 FSM 状态机验证完毕！");
}
