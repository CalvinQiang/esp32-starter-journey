/**
 * ======================================================================================
 * 🌟 ESP32 物联网实战闯关 —— 第 06 关：ESP32 RMT 硬件脉冲外设与 WS2812 幻彩 RGB 跑马灯
 * ======================================================================================
 * 
 * 🎯 【关卡目标】
 * 1. 深入理解 WS2812 单线归零码（NZR）纳秒级严苛时序协议（800kHz 数据流）；
 * 2. 掌握 ESP32 独门硬件武器 —— RMT（Remote Control）外设的硬件发波机制与零 CPU 占用特性；
 * 3. 掌握 HSV（色相/饱和度/明度）与 RGB 颜色空间的数学转换原理；
 * 4. 驱动板载/外接 WS2812 幻彩 RGB 灯珠，实现“彩虹流光”、“呼吸渐变”、“影院追逐”、“流星彗星”多种炫彩光效；
 * 5. 结合 SW3 按键实现灯效模式的实时切换与串口交互。
 * 
 * 📌 【硬件引脚连接】
 * - WS2812 数据引脚 (DIN) : GPIO26 (JP3 接口)
 *   ⚠️ 重要提醒：GPIO26 在开发板上与 LCD 屏幕背光 (BL) 共用！
 *   调试板载 WS2812 时，请务必【拔下 JP7 背光跳线帽】，防止背光电路负载干扰纳秒级高频脉冲！
 * - 用户按键 SW3           : GPIO39 (VN，仅作输入，按下为低电平)
 * 
 * 🛠️ 【核心外设驱动】
 * - ESP-IDF RMT Driver (esp_driver_rmt / led_strip 官方组件)
 * ======================================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "led_strip.h"

static const char *TAG = "LEVEL06_WS2812";

/* ========================== 硬件与灯效配置参数 ========================== */
#define WS2812_GPIO_PIN         GPIO_NUM_26   // WS2812 数据控制引脚 (JP3)
#define LED_STRIP_NUM_LEDS      12            // 灯珠数量（套件配套 HS-F12A 环形灯为 12 颗）
#define BUTTON_SW3_PIN          GPIO_NUM_39   // 模式切换按键

/* 灯效模式枚举（共 10 大模式） */
typedef enum {
    MODE_RAINBOW_FLOW = 0,  // 1. 🌈 彩虹流光瀑布
    MODE_BREATHE_PULSE,     // 2. 🫁 HSV 呼吸渐变
    MODE_THEATER_CHASE,     // 3. 🎬 影院跑马追逐
    MODE_COMET_METEOR,      // 4. ☄️ 流星拖尾光束
    MODE_SOLID_CYCLE,       // 5. 🎨 经典纯色循环
    MODE_CYBERPUNK_DUAL,    // 6. 🌀 赛博朋克双向对撞 (粉/青两束光交汇爆发白光)
    MODE_FIRE_FLICKER,      // 7. 🔥 温暖烈焰壁炉微光 (红橙金自然随机抖动)
    MODE_ARC_REACTOR,       // 8. 💓 钢铁侠反应堆脉冲心跳 (冰蓝双重强弱心跳)
    MODE_RADAR_SONAR,       // 9. 🧭 雷达声呐余辉扫描 (极速绿色探针余辉)
    MODE_ALL_OFF,           // 10. 🌑 全黑静音熄灭 (防刺眼/节能休眠)
    MODE_MAX_COUNT
} led_mode_t;

static volatile led_mode_t g_current_mode = MODE_RAINBOW_FLOW;
static led_strip_handle_t s_led_strip = NULL;

/* 模式名称文本映射 */
static const char *MODE_NAMES[] = {
    "🌈 [1/10] 彩虹流光瀑布 (Rainbow Flow)",
    "🫁 [2/10] HSV 呼吸渐变 (Breathing Pulse)",
    "🎬 [3/10] 影院跑马追逐 (Theater Chase)",
    "☄️ [4/10] 流星拖尾光束 (Comet Meteor)",
    "🎨 [5/10] 经典纯色循环 (Solid Cycle)",
    "🌀 [6/10] 赛博朋克双向对撞 (Cyberpunk Dual Clash)",
    "🔥 [7/10] 烈焰壁炉微光 (Fire & Flame Flicker)",
    "💓 [8/10] 反应堆脉冲心跳 (Arc Reactor Pulse)",
    "🧭 [9/10] 雷达声呐余辉扫描 (Radar Sonar Scan)",
    "🌑 [10/10] 全黑静音熄灭 (All Off / Sleep)"
};

/**
 * @brief HSV (Hue, Saturation, Value) 转 RGB (Red, Green, Blue)
 * 
 * @param h 色相角度: 0 ~ 359 (0°=红, 120°=绿, 240°=蓝)
 * @param s 饱和度: 0 ~ 255 (0=白灰, 255=最纯艳纯色)
 * @param v 明度/亮度: 0 ~ 255 (0=全黑, 255=最大亮度)
 * @param[out] r 输出红色分量 (0 ~ 255)
 * @param[out] g 输出绿色分量 (0 ~ 255)
 * @param[out] b 输出蓝色分量 (0 ~ 255)
 */
static void hsv_to_rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    if (s == 0) {
        // 饱和度为 0 时为纯灰阶
        *r = v;
        *g = v;
        *b = v;
        return;
    }

    h %= 360; // 限制在 0~359 范围内
    uint32_t region = h / 60;      // 划分为 6 个 60° 扇区 (0~5)
    uint32_t remainder = (h - (region * 60)) * 6; // 扇区内线性偏移量 (0~359)

    uint32_t p = (v * (255 - s)) >> 8;
    uint32_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    uint32_t t = (v * (255 - ((s * (360 - remainder)) >> 8))) >> 8;

    switch (region) {
        case 0: *r = v; *g = t; *b = p; break;
        case 1: *r = q; *g = v; *b = p; break;
        case 2: *r = p; *g = v; *b = t; break;
        case 3: *r = p; *g = q; *b = v; break;
        case 4: *r = t; *g = p; *b = v; break;
        default: *r = v; *g = p; *b = q; break;
    }
}

/**
 * @brief 初始化 WS2812 RMT 硬件外设
 */
static esp_err_t ws2812_init(void)
{
    ESP_LOGI(TAG, "🔧 正在配置 RMT 硬件通道驱动 WS2812 (引脚: GPIO%d, 灯珠数: %d)...",
             WS2812_GPIO_PIN, LED_STRIP_NUM_LEDS);

    // 1. LED 灯带基础参数配置
    led_strip_config_t strip_config = {
        .strip_gpio_num = WS2812_GPIO_PIN,
        .max_leds = LED_STRIP_NUM_LEDS,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // WS2812 标准时序颜色排列为 GRB
        .flags = {
            .invert_out = false, // 正常高有效电平输出
        }
    };

    // 2. RMT 硬件发生器参数配置
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz 时基时钟 (1 个 tick = 100ns，满足纳秒级脉冲要求)
        .mem_block_symbols = 64,           // 分配 RMT 内部硬件符号内存块
        .flags = {
            .with_dma = false,             // 灯珠数量较少时无需占用 DMA 通道
        }
    };

    // 3. 创建并启动 RMT LED 灯带设备
    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ RMT 驱动初始化失败: %s", esp_err_to_name(err));
        return err;
    }

    // 先全刷黑清屏，确保干净起始状态
    led_strip_clear(s_led_strip);
    ESP_LOGI(TAG, "✅ WS2812 RMT 驱动初始化成功！硬件纳秒脉冲引擎已就绪。");
    return ESP_OK;
}

/**
 * @brief 模式 1：彩虹流光瀑布动画 (Rainbow Flow)
 */
static void anim_rainbow_flow(uint32_t step)
{
    uint32_t r, g, b;
    for (int i = 0; i < LED_STRIP_NUM_LEDS; i++) {
        // 计算每个灯珠在色相环上的相位（360度分布）
        uint32_t hue = (step * 5 + (i * 360 / LED_STRIP_NUM_LEDS)) % 360;
        hsv_to_rgb(hue, 255, 180, &r, &g, &b);
        led_strip_set_pixel(s_led_strip, i, r, g, b);
    }
    led_strip_refresh(s_led_strip);
}

/**
 * @brief 模式 2：HSV 呼吸渐变 (Breathing Pulse)
 */
static void anim_breathing_pulse(uint32_t step)
{
    uint32_t r, g, b;
    uint32_t hue = (step * 2) % 360;
    
    // 使用三角波计算平滑明度（从 10 呼吸到 220）
    uint32_t phase = step % 100;
    uint32_t val = (phase < 50) ? (10 + phase * 4) : (210 - (phase - 50) * 4);

    hsv_to_rgb(hue, 255, val, &r, &g, &b);
    for (int i = 0; i < LED_STRIP_NUM_LEDS; i++) {
        led_strip_set_pixel(s_led_strip, i, r, g, b);
    }
    led_strip_refresh(s_led_strip);
}

/**
 * @brief 模式 3：影院跑马追逐 (Theater Chase)
 */
static void anim_theater_chase(uint32_t step)
{
    uint32_t r, g, b;
    uint32_t hue = (step * 10) % 360;
    hsv_to_rgb(hue, 255, 200, &r, &g, &b);

    int active_idx = step % 3;
    for (int i = 0; i < LED_STRIP_NUM_LEDS; i++) {
        if ((i + active_idx) % 3 == 0) {
            led_strip_set_pixel(s_led_strip, i, r, g, b);
        } else {
            led_strip_set_pixel(s_led_strip, i, 0, 0, 0);
        }
    }
    led_strip_refresh(s_led_strip);
}

/**
 * @brief 模式 4：流星彗星拖尾 (Comet Meteor)
 */
static void anim_comet_meteor(uint32_t step)
{
    uint32_t r, g, b;
    int head_pos = step % (LED_STRIP_NUM_LEDS + 4);
    uint32_t hue = (step * 8) % 360;

    for (int i = 0; i < LED_STRIP_NUM_LEDS; i++) {
        int dist = head_pos - i;
        if (dist == 0) {
            // 彗星头部：极亮白色/主色
            hsv_to_rgb(hue, 80, 255, &r, &g, &b);
            led_strip_set_pixel(s_led_strip, i, r, g, b);
        } else if (dist > 0 && dist <= 3) {
            // 彗星尾巴：按距离衰减
            uint32_t tail_val = 180 / (dist * 2);
            hsv_to_rgb(hue, 255, tail_val, &r, &g, &b);
            led_strip_set_pixel(s_led_strip, i, r, g, b);
        } else {
            led_strip_set_pixel(s_led_strip, i, 0, 0, 0);
        }
    }
    led_strip_refresh(s_led_strip);
}

/**
 * @brief 模式 5：经典纯色循环 (Solid Cycle)
 */
static void anim_solid_cycle(uint32_t step)
{
    static const uint32_t PALETTE[][3] = {
        {255, 0, 0},     // 红
        {255, 128, 0},   // 橙
        {255, 255, 0},   // 黄
        {0, 255, 0},     // 绿
        {0, 255, 255},   // 青
        {0, 0, 255},     // 蓝
        {160, 32, 240},  // 紫
        {255, 20, 147}   // 粉
    };
    int color_idx = (step / 30) % 8;
    for (int i = 0; i < LED_STRIP_NUM_LEDS; i++) {
        led_strip_set_pixel(s_led_strip, i, PALETTE[color_idx][0], PALETTE[color_idx][1], PALETTE[color_idx][2]);
    }
    led_strip_refresh(s_led_strip);
}

/**
 * @brief 模式 6：赛博朋克双向对撞 (Cyberpunk Dual Clash)
 * 顺时针霓虹粉光束与逆时针电光青蓝光束在圆环相遇碰撞，重叠瞬间爆发高亮纯白闪光
 */
static void anim_cyberpunk_dual(uint32_t step)
{
    led_strip_clear(s_led_strip);
    int p1 = step % LED_STRIP_NUM_LEDS;
    int p2 = (LED_STRIP_NUM_LEDS * 2 - (step % LED_STRIP_NUM_LEDS)) % LED_STRIP_NUM_LEDS;

    uint32_t r, g, b;
    if (p1 == p2) {
        // 对撞爆裂瞬间：超亮极白闪光
        for (int i = 0; i < LED_STRIP_NUM_LEDS; i++) {
            if (i == p1) {
                led_strip_set_pixel(s_led_strip, i, 255, 255, 255);
            } else {
                led_strip_set_pixel(s_led_strip, i, 15, 25, 35);
            }
        }
    } else {
        // 顺时针赛博粉 (Hue ~ 320°) + 拖尾
        hsv_to_rgb(320, 240, 255, &r, &g, &b);
        led_strip_set_pixel(s_led_strip, p1, r, g, b);
        int t1 = (p1 - 1 + LED_STRIP_NUM_LEDS) % LED_STRIP_NUM_LEDS;
        hsv_to_rgb(320, 255, 50, &r, &g, &b);
        led_strip_set_pixel(s_led_strip, t1, r, g, b);

        // 逆时针电光青 (Hue ~ 180°) + 拖尾
        hsv_to_rgb(180, 240, 255, &r, &g, &b);
        led_strip_set_pixel(s_led_strip, p2, r, g, b);
        int t2 = (p2 + 1) % LED_STRIP_NUM_LEDS;
        hsv_to_rgb(180, 255, 50, &r, &g, &b);
        led_strip_set_pixel(s_led_strip, t2, r, g, b);
    }
    led_strip_refresh(s_led_strip);
}

/**
 * @brief 模式 7：温暖烈焰壁炉微光 (Fire & Flame Flicker)
 * 模拟真实木柴燃烧的火苗跳跃，红、橙、金黄在各灯珠间随机明暗抖动
 */
static void anim_fire_flicker(uint32_t step)
{
    uint32_t r, g, b;
    for (int i = 0; i < LED_STRIP_NUM_LEDS; i++) {
        uint32_t hue = (rand() % 36);          // 0°~35° 纯红到金黄
        uint32_t sat = 230 + (rand() % 26);    // 高饱和度
        uint32_t val = 50 + (rand() % 185);    // 随机明暗抖动
        hsv_to_rgb(hue, sat, val, &r, &g, &b);
        led_strip_set_pixel(s_led_strip, i, r, g, b);
    }
    led_strip_refresh(s_led_strip);
}

/**
 * @brief 模式 8：钢铁侠方舟反应堆心跳律动 (Arc Reactor Pulse)
 * 拟真人体/机械核心“咚-咚---咚-咚”双脉冲心跳节奏，冰蓝与冷白高科技质感
 */
static void anim_arc_reactor(uint32_t step)
{
    uint32_t r, g, b;
    uint32_t tick = step % 40; // 一个心跳周期 40 * 25ms = 1000ms
    uint32_t val = 20;
    uint32_t sat = 240;

    if (tick < 6) {
        // 第一跳：主脉冲极速冲击
        val = 20 + tick * 38;
        sat = (tick > 3) ? 120 : 200; // 峰值冷白
    } else if (tick >= 6 && tick < 12) {
        val = 248 - (tick - 6) * 30;
    } else if (tick >= 12 && tick < 18) {
        // 第二跳：副脉冲回弹
        val = 68 + (tick - 12) * 25;
        sat = 180;
    } else if (tick >= 18 && tick < 24) {
        val = 218 - (tick - 18) * 33;
    } else {
        val = 20;
        sat = 255;
    }

    hsv_to_rgb(195, sat, val, &r, &g, &b); // 195° 经典方舟反应堆冰蓝色
    for (int i = 0; i < LED_STRIP_NUM_LEDS; i++) {
        led_strip_set_pixel(s_led_strip, i, r, g, b);
    }
    led_strip_refresh(s_led_strip);
}

/**
 * @brief 模式 9：雷达声呐余辉扫描 (Radar Sonar Scan)
 * 顺时针声呐扫描，头部正绿探针，尾部多级指数衰减余辉
 */
static void anim_radar_sonar(uint32_t step)
{
    int head = step % LED_STRIP_NUM_LEDS;
    uint32_t r, g, b;

    for (int i = 0; i < LED_STRIP_NUM_LEDS; i++) {
        int dist = (head - i + LED_STRIP_NUM_LEDS) % LED_STRIP_NUM_LEDS;
        if (dist == 0) {
            hsv_to_rgb(125, 255, 255, &r, &g, &b); // 探针头部
        } else if (dist <= 5) {
            uint32_t tail_val = 200 >> dist;       // 指数衰减余辉
            hsv_to_rgb(125, 255, tail_val, &r, &g, &b);
        } else {
            r = g = b = 0;
        }
        led_strip_set_pixel(s_led_strip, i, r, g, b);
    }
    led_strip_refresh(s_led_strip);
}

/**
 * @brief 模式 10：全黑静音熄灭 (All Off / Sleep)
 */
static void anim_all_off(void)
{
    led_strip_clear(s_led_strip);
}

/**
 * @brief WS2812 动画渲染总任务
 */
static void task_led_animation(void *arg)
{
    uint32_t step = 0;
    ESP_LOGI(TAG, "🚀 灯效渲染任务已启动，帧率: 50 FPS (20ms/帧)");

    while (1) {
        switch (g_current_mode) {
            case MODE_RAINBOW_FLOW:
                anim_rainbow_flow(step);
                vTaskDelay(pdMS_TO_TICKS(20)); // 50 FPS
                break;

            case MODE_BREATHE_PULSE:
                anim_breathing_pulse(step);
                vTaskDelay(pdMS_TO_TICKS(25)); // 40 FPS
                break;

            case MODE_THEATER_CHASE:
                anim_theater_chase(step);
                vTaskDelay(pdMS_TO_TICKS(80)); // 较慢的跳跃步进
                break;

            case MODE_COMET_METEOR:
                anim_comet_meteor(step);
                vTaskDelay(pdMS_TO_TICKS(60));
                break;

            case MODE_SOLID_CYCLE:
                anim_solid_cycle(step);
                vTaskDelay(pdMS_TO_TICKS(20));
                break;

            case MODE_CYBERPUNK_DUAL:
                anim_cyberpunk_dual(step);
                vTaskDelay(pdMS_TO_TICKS(70));
                break;

            case MODE_FIRE_FLICKER:
                anim_fire_flicker(step);
                vTaskDelay(pdMS_TO_TICKS(45));
                break;

            case MODE_ARC_REACTOR:
                anim_arc_reactor(step);
                vTaskDelay(pdMS_TO_TICKS(25));
                break;

            case MODE_RADAR_SONAR:
                anim_radar_sonar(step);
                vTaskDelay(pdMS_TO_TICKS(50));
                break;

            case MODE_ALL_OFF:
                anim_all_off();
                vTaskDelay(pdMS_TO_TICKS(100)); // 熄灭模式降频运行，节能静音
                break;

            default:
                break;
        }

        step++;
    }
}

/**
 * @brief 按键监听任务：按下 SW3 切换下一组灯效
 */
static void task_button_control(void *arg)
{
    // 配置 GPIO39 为纯输入
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_SW3_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    ESP_LOGI(TAG, "🔘 按键监听已就绪：按下 SW3 (GPIO39) 可即时切换灯效！");

    int last_level = 1;

    while (1) {
        int current_level = gpio_get_level(BUTTON_SW3_PIN);
        // 检测下降沿（由高电平 1 变低电平 0，表示按下）
        if (last_level == 1 && current_level == 0) {
            vTaskDelay(pdMS_TO_TICKS(20)); // 20ms 硬件消抖
            if (gpio_get_level(BUTTON_SW3_PIN) == 0) {
                // 切换下一个模式
                g_current_mode = (g_current_mode + 1) % MODE_MAX_COUNT;
                ESP_LOGW(TAG, "🔀 【用户按键触发】切换灯效为: %s", MODE_NAMES[g_current_mode]);
                
                // 等待按键释放
                while (gpio_get_level(BUTTON_SW3_PIN) == 0) {
                    vTaskDelay(pdMS_TO_TICKS(20));
                }
            }
        }
        last_level = current_level;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ========================== 应用程序主入口 ========================== */
void app_main(void)
{
    ESP_LOGI(TAG, "=======================================================");
    ESP_LOGI(TAG, "🚀 LEVEL 06: ESP32 RMT 硬件脉冲外设与 WS2812 幻彩 RGB");
    ESP_LOGI(TAG, "   主控架构: ESP32-D0WD-V3 (8MB Flash + 2MB PSRAM)");
    ESP_LOGI(TAG, "   当前灯效: %s", MODE_NAMES[g_current_mode]);
    ESP_LOGI(TAG, "   ⚠️ 提醒: 请拔下 JP7 跳线帽（断开背光），接通 JP3 WS2812");
    ESP_LOGI(TAG, "=======================================================");

    // 1. 初始化 WS2812 RMT 驱动
    ESP_ERROR_CHECK(ws2812_init());

    // 2. 创建灯效渲染任务（优先级 3，栈深度 3072 字节）
    xTaskCreate(task_led_animation, "Task_LED_Anim", 3072, NULL, 3, NULL);

    // 3. 创建按键切换任务（优先级 2，栈深度 2048 字节）
    xTaskCreate(task_button_control, "Task_Btn_Ctrl", 2048, NULL, 2, NULL);
}
