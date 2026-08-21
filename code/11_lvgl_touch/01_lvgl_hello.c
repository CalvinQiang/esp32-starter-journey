/**
 * 🌟 ESP32 物联网实战 —— 第 11 关 实验 1：LVGL v9 图形框架初探 (Hello LVGL)
 * 
 * 🎯 学习目标：
 *    1. 掌握现代嵌入式图形框架 LVGL v9 的核心架构；
 *    2. 使用 `esp_lvgl_port` 官方中间件管理 LVGL 定时器心跳与 FreeRTOS 渲染任务；
 *    3. 在 1.69寸彩屏上渲染高颜值卡片、现代流光圆弧 (Spinner) 与多色标签。
 * 
 * 📌 硬件接口：
 *    - ST7789 屏幕 SPI: SCLK(18), MOSI(19), CS(5), DC(17), RST(21), BL(26)
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

static const char *TAG = "EXP1_LVGL_HELLO";

#define LCD_HOST                SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ      (40 * 1000 * 1000)
#define LCD_PIN_MOSI            GPIO_NUM_19
#define LCD_PIN_SCLK            GPIO_NUM_18
#define LCD_PIN_CS              GPIO_NUM_5
#define LCD_PIN_DC              GPIO_NUM_17
#define LCD_PIN_RST             GPIO_NUM_21
#define LCD_PIN_BACKLIGHT       GPIO_NUM_26

#define LCD_H_RES               240
#define LCD_V_RES               280

static esp_lcd_panel_handle_t s_panel = NULL;
static lv_display_t *s_lvgl_disp = NULL;

static void lcd_init(void)
{
    // 点亮背光
    gpio_config_t bl = { .pin_bit_mask = (1ULL << LCD_PIN_BACKLIGHT), .mode = GPIO_MODE_OUTPUT };
    gpio_config(&bl);
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
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
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

static void lvgl_init(void)
{
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = NULL,
        .panel_handle = s_panel,
        .buffer_size = LCD_H_RES * 40,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .rotation = { .swap_xy = false, .mirror_x = false, .mirror_y = false },
        .flags = { .buff_dma = true, .swap_bytes = true }
    };
    s_lvgl_disp = lvgl_port_add_disp(&disp_cfg);
}

static void create_hello_ui(void)
{
    lvgl_port_lock(0);

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0F172A), 0); // 深色科技蓝底

    // 1. 顶部标题
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "ESP32 LVGL v9");
    lv_obj_set_style_text_color(title, lv_color_hex(0x38BDF8), 0); // 荧光青
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    // 2. 现代磨砂卡片
    lv_obj_t *card = lv_obj_create(scr);
    lv_obj_set_size(card, 210, 140);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x334155), 0);
    lv_obj_set_style_radius(card, 16, 0);

    // 3. 卡片内旋转加载环 (Spinner)
    lv_obj_t *spinner = lv_spinner_create(card);
    lv_obj_set_size(spinner, 50, 50);
    lv_obj_align(spinner, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(0x38BDF8), LV_PART_INDICATOR);

    // 4. 卡片说明文字
    lv_obj_t *status_label = lv_label_create(card);
    lv_label_set_text(status_label, "UI Engine Ready!\n240x280 IPS");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -5);

    lvgl_port_unlock();
}

void app_main(void)
{
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   🚀 关卡 11 实验 1：LVGL v9 现代图形界面初探     ");
    ESP_LOGI(TAG, "==================================================");

    lcd_init();
    lvgl_init();
    create_hello_ui();

    ESP_LOGI(TAG, "✅ LVGL v9 界面构建完成，FreeRTOS 渲染中...");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
