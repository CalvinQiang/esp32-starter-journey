/**
 * 🌟 ESP32 物联网实战 —— 第 11 关 实验 3：智能家居中控触控面板 (Smart Home Panel)
 * 
 * 🎯 学习目标：
 *    1. 掌握 LVGL v9 核心高级控件体系（`lv_switch`、`lv_slider`、`lv_arc`、`lv_card`）；
 *    2. 掌握滑动条与圆形仪表盘的数值实时双向绑定；
 *    3. 打造一个兼具高颜值、全功能触控交互的智能家居微型中控屏。
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

static const char *TAG = "EXP3_SMART_PANEL";

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

#define LED2_PIN                GPIO_NUM_27

#define LCD_H_RES               240
#define LCD_V_RES               280

static esp_lcd_panel_handle_t s_panel = NULL;
static esp_lcd_touch_handle_t s_touch = NULL;

static lv_obj_t *s_slider_label = NULL;
static lv_obj_t *s_temp_arc = NULL;
static lv_obj_t *s_temp_label = NULL;

static void hardware_init(void)
{
    gpio_config_t led_cfg = { .pin_bit_mask = (1ULL << LED2_PIN), .mode = GPIO_MODE_OUTPUT };
    gpio_config(&led_cfg);
    gpio_set_level(LED2_PIN, 0);

    gpio_config_t bl_cfg = { .pin_bit_mask = (1ULL << LCD_PIN_BACKLIGHT), .mode = GPIO_MODE_OUTPUT };
    gpio_config(&bl_cfg);
    gpio_set_level(LCD_PIN_BACKLIGHT, 1);

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

    // Touch
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

    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_cfg, &tp_io));

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = TOUCH_I2C_INT,
        .flags = { .swap_xy = 0, .mirror_x = 0, .mirror_y = 0 },
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(tp_io, &tp_cfg, &s_touch));
}

/* 开关事件回调 */
static void switch_event_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    bool is_on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    gpio_set_level(LED2_PIN, is_on ? 1 : 0);
    ESP_LOGI(TAG, "💡 智能灯光开关切换 ➔ %s", is_on ? "ON" : "OFF");
}

/* 滑动条事件回调 */
static void slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    char buf[32];
    snprintf(buf, sizeof(buf), "Brightness: %ld%%", (long)val);
    lv_label_set_text(s_slider_label, buf);
    ESP_LOGI(TAG, "🔆 亮度调节 ➔ %ld%%", (long)val);
}

static void create_smart_home_ui(void)
{
    lvgl_port_lock(0);

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0F172A), 0);

    // 1. 顶部状态栏
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Smart Station");
    lv_obj_set_style_text_color(title, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 15, 12);

    // 2. 居中温度圆弧仪表盘
    s_temp_arc = lv_arc_create(scr);
    lv_obj_set_size(s_temp_arc, 110, 110);
    lv_arc_set_rotation(s_temp_arc, 135);
    lv_arc_set_bg_angles(s_temp_arc, 0, 270);
    lv_arc_set_range(s_temp_arc, 0, 50);
    lv_arc_set_value(s_temp_arc, 26);
    lv_obj_set_style_arc_color(s_temp_arc, lv_color_hex(0x10B981), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_temp_arc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_temp_arc, 10, LV_PART_MAIN);
    lv_obj_remove_style(s_temp_arc, NULL, LV_PART_KNOB); // 去掉拖拽把手，作为纯展示仪表
    lv_obj_align(s_temp_arc, LV_ALIGN_TOP_MID, 0, 42);

    s_temp_label = lv_label_create(scr);
    lv_label_set_text(s_temp_label, "26.5°C");
    lv_obj_set_style_text_font(s_temp_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_temp_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(s_temp_label, LV_ALIGN_TOP_MID, 0, 85);

    // 3. 灯光触控开关卡片
    lv_obj_t *card_sw = lv_obj_create(scr);
    lv_obj_set_size(card_sw, 210, 50);
    lv_obj_align(card_sw, LV_ALIGN_TOP_MID, 0, 160);
    lv_obj_set_style_bg_color(card_sw, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_color(card_sw, lv_color_hex(0x334155), 0);
    lv_obj_set_style_radius(card_sw, 12, 0);

    lv_obj_t *lbl_sw = lv_label_create(card_sw);
    lv_label_set_text(lbl_sw, "Main Light");
    lv_obj_set_style_text_color(lbl_sw, lv_color_hex(0xF8FAFC), 0);
    lv_obj_align(lbl_sw, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *sw = lv_switch_create(card_sw);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(sw, switch_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // 4. 亮度触控滑动条卡片
    lv_obj_t *card_sl = lv_obj_create(scr);
    lv_obj_set_size(card_sl, 210, 55);
    lv_obj_align(card_sl, LV_ALIGN_TOP_MID, 0, 215);
    lv_obj_set_style_bg_color(card_sl, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_color(card_sl, lv_color_hex(0x334155), 0);
    lv_obj_set_style_radius(card_sl, 12, 0);

    s_slider_label = lv_label_create(card_sl);
    lv_label_set_text(s_slider_label, "Brightness: 80%");
    lv_obj_set_style_text_font(s_slider_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_slider_label, lv_color_hex(0x94A3B8), 0);
    lv_obj_align(s_slider_label, LV_ALIGN_TOP_LEFT, 0, -2);

    lv_obj_t *slider = lv_slider_create(card_sl);
    lv_obj_set_size(slider, 180, 10);
    lv_slider_set_value(slider, 80, LV_ANIM_OFF);
    lv_obj_align(slider, LV_ALIGN_BOTTOM_MID, 0, 2);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lvgl_port_unlock();
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 11 实验 3：智能家居触控中控面板 (终极)  ");
    ESP_LOGI(TAG, "==================================================");

    hardware_init();

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

    create_smart_home_ui();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
