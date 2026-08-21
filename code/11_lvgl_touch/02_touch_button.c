/**
 * 🌟 ESP32 物联网实战 —— 第 11 关 实验 2：CST816S 电容触摸与交互按钮控制
 * 
 * 🎯 学习目标：
 *    1. 掌握 CST816S I2C 电容触摸芯片的初始化与中断配置；
 *    2. 将硬件触摸屏注册为 LVGL 输入设备 (Pointer Device)；
 *    3. 编写按钮点击事件回调函数（Event Callback），手指触控切换板载 LED2 状态。
 * 
 * 📌 硬件接口：
 *    - 触摸 I2C: SCL(22), SDA(23), INT(35), RST(21)
 *    - 板载 LED2: GPIO27
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

static const char *TAG = "EXP2_TOUCH_BUTTON";

#define LCD_HOST                SPI2_HOST
#define LCD_PIN_MOSI            GPIO_NUM_19
#define LCD_PIN_SCLK            GPIO_NUM_18
#define LCD_PIN_CS              GPIO_NUM_5
#define LCD_PIN_DC              GPIO_NUM_17
#define LCD_PIN_RST             GPIO_NUM_21
#define LCD_PIN_BACKLIGHT       GPIO_NUM_26

#define TOUCH_I2C_SCL           GPIO_NUM_22
#define TOUCH_I2C_SDA           GPIO_NUM_23
#define TOUCH_I2C_INT           GPIO_NUM_35
#define TOUCH_I2C_RST           GPIO_NUM_21

#define LED2_PIN                GPIO_NUM_27

#define LCD_H_RES               240
#define LCD_V_RES               280

static esp_lcd_panel_handle_t s_panel = NULL;
static esp_lcd_touch_handle_t s_touch = NULL;
static bool s_led_state = false;
static int s_click_count = 0;
static lv_obj_t *s_btn_label = NULL;
static lv_obj_t *s_info_label = NULL;

static void hardware_init(void)
{
    // 1. 初始化 LED2
    gpio_config_t led_cfg = { .pin_bit_mask = (1ULL << LED2_PIN), .mode = GPIO_MODE_OUTPUT };
    gpio_config(&led_cfg);
    gpio_set_level(LED2_PIN, 0);

    // 2. 点亮背光
    gpio_config_t bl_cfg = { .pin_bit_mask = (1ULL << LCD_PIN_BACKLIGHT), .mode = GPIO_MODE_OUTPUT };
    gpio_config(&bl_cfg);
    gpio_set_level(LCD_PIN_BACKLIGHT, 1);

    // 3. 初始化 LCD SPI
    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_PIN_SCLK,
        .mosi_io_num = LCD_PIN_MOSI,
        .max_transfer_sz = LCD_H_RES * 40 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = 40 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &io));

    esp_lcd_panel_dev_config_t p_cfg = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io, &p_cfg, &s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(s_panel, 20, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));
}

static void touch_init(void)
{
    // 初始化 I2C 总线
    i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = TOUCH_I2C_SDA,
        .scl_io_num = TOUCH_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t i2c_bus = NULL;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus));

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_config, &tp_io_handle));

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = TOUCH_I2C_INT,
        .flags = { .swap_xy = 0, .mirror_x = 0, .mirror_y = 0 },
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &s_touch));
    ESP_LOGI(TAG, "✅ CST816S 电容触摸屏初始化成功 (I2C 0x15)");
}

/* 按钮点击事件回调 */
static void btn_click_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        s_led_state = !s_led_state;
        s_click_count++;
        gpio_set_level(LED2_PIN, s_led_state ? 1 : 0);

        ESP_LOGI(TAG, "👆 触发触摸点击！LED2 状态 ➔ %s (累计点击: %d 次)", s_led_state ? "ON" : "OFF", s_click_count);

        if (s_led_state) {
            lv_label_set_text(s_btn_label, "LED: ON 💡");
            lv_obj_set_style_bg_color(lv_event_get_target(e), lv_color_hex(0x10B981), 0); // 翠绿色
        } else {
            lv_label_set_text(s_btn_label, "LED: OFF 💤");
            lv_obj_set_style_bg_color(lv_event_get_target(e), lv_color_hex(0x64748B), 0); // 灰青色
        }

        char info_buf[64];
        snprintf(info_buf, sizeof(info_buf), "Touch Detected!\nClicks: %d", s_click_count);
        lv_label_set_text(s_info_label, info_buf);
    }
}

static void create_touch_ui(void)
{
    lvgl_port_lock(0);

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0F172A), 0);

    // 1. 标题
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Capacitive Touch");
    lv_obj_set_style_text_color(title, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    // 2. 交互大按钮
    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, 170, 70);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x64748B), 0);
    lv_obj_set_style_radius(btn, 20, 0);
    lv_obj_add_event_cb(btn, btn_click_event_cb, LV_EVENT_CLICKED, NULL);

    s_btn_label = lv_label_create(btn);
    lv_label_set_text(s_btn_label, "LED: OFF 💤");
    lv_obj_set_style_text_font(s_btn_label, &lv_font_montserrat_16, 0);
    lv_obj_center(s_btn_label);

    // 3. 点击计数卡片
    s_info_label = lv_label_create(scr);
    lv_label_set_text(s_info_label, "Touch anywhere on\nbutton to toggle LED");
    lv_obj_set_style_text_color(s_info_label, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_align(s_info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_info_label, LV_ALIGN_BOTTOM_MID, 0, -30);

    lvgl_port_unlock();
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 11 实验 2：CST816S 电容触摸与按键交互   ");
    ESP_LOGI(TAG, "==================================================");

    hardware_init();
    touch_init();

    // 初始化 LVGL 端口与显示/触摸驱动
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .panel_handle = s_panel,
        .buffer_size = LCD_H_RES * 40,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .flags = { .buff_dma = true, .swap_bytes = true }
    };
    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);

    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = disp,
        .handle = s_touch,
    };
    lvgl_port_add_touch(&touch_cfg);

    create_touch_ui();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
