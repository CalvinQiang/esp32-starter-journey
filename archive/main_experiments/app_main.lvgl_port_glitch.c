#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_lvgl_port.h"
#include "lvgl.h"

#include "lcd_st7789.h"

#define SW3_GPIO                    GPIO_NUM_39
#define BUTTON_DEBOUNCE_MS           220
#define LVGL_DRAW_LINES              52
#define LVGL_TRANS_LINES             20

#define COLOR_BG                     lv_color_hex(0x030817)
#define COLOR_PANEL                  lv_color_hex(0x08142A)
#define COLOR_GRID                   lv_color_hex(0x18355C)
#define COLOR_CYAN                   lv_color_hex(0x32E8FF)
#define COLOR_BLUE                   lv_color_hex(0x2878FF)
#define COLOR_MAGENTA                lv_color_hex(0xD144FF)
#define COLOR_PINK                   lv_color_hex(0xFF4D8D)
#define COLOR_GREEN                  lv_color_hex(0x36F7A2)
#define COLOR_TEXT                   lv_color_hex(0xE7F6FF)
#define COLOR_DIM                    lv_color_hex(0x6D8EAE)

static const char *TAG = "heart_lvgl";

static lv_obj_t *g_bpm_label;
static lv_obj_t *g_state_label;
static lv_obj_t *g_arc;
static lv_obj_t *g_chart;
static lv_chart_series_t *g_series;
static lv_obj_t *g_action_label;
static bool g_measuring;
static int g_bpm = 78;
static uint32_t g_phase;

static const int32_t HEART_24H[] = {
    63, 61, 60, 59, 60, 62, 67, 74, 81, 86, 83, 79,
    76, 78, 84, 91, 88, 82, 77, 73, 70, 68, 66, 64,
};

static void label_style(lv_obj_t *obj, const lv_font_t *font, lv_color_t color)
{
    lv_obj_set_style_text_font(obj, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(obj, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN);
}

static lv_obj_t *create_text(lv_obj_t *parent, const char *text, int x, int y,
                             const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    label_style(label, font, color);
    lv_obj_set_pos(label, x, y);
    return label;
}

static lv_obj_t *create_panel(lv_obj_t *parent, int x, int y, int width, int height)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, width, height);
    lv_obj_set_style_bg_color(panel, COLOR_PANEL, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(panel, lv_color_hex(0x061020), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(panel, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, COLOR_GRID, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(panel, COLOR_CYAN, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(panel, 10, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(panel, LV_OPA_20, LV_PART_MAIN);
    return panel;
}

static void set_measurement_state(bool enabled)
{
    g_measuring = enabled;
    if (enabled) {
        lv_label_set_text(g_state_label, "MEASURING");
        lv_obj_set_style_text_color(g_state_label, COLOR_GREEN, LV_PART_MAIN);
        lv_label_set_text(g_action_label, "SW3  STOP MEASUREMENT");
        lv_obj_set_style_arc_color(g_arc, COLOR_PINK, LV_PART_INDICATOR);
    } else {
        lv_label_set_text(g_state_label, "READY");
        lv_obj_set_style_text_color(g_state_label, COLOR_CYAN, LV_PART_MAIN);
        lv_label_set_text(g_action_label, "SW3  START MEASUREMENT");
        lv_obj_set_style_arc_color(g_arc, COLOR_CYAN, LV_PART_INDICATOR);
    }
}

static void update_bpm_display(void)
{
    char bpm_text[12];
    snprintf(bpm_text, sizeof(bpm_text), "%d", g_bpm);
    lv_label_set_text(g_bpm_label, bpm_text);
    lv_arc_set_value(g_arc, g_bpm);
}

static void measurement_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!g_measuring) {
        return;
    }

    static const int8_t waveform[] = {0, 2, 4, 6, 4, 2, 0, -2, -4, -3, -1, 1};
    g_bpm = 78 + waveform[g_phase % (sizeof(waveform) / sizeof(waveform[0]))];
    ++g_phase;
    update_bpm_display();
    lv_chart_set_next_value(g_chart, g_series, g_bpm);
}

static void toggle_measurement(void)
{
    set_measurement_state(!g_measuring);
    if (g_measuring) {
        g_phase = 0;
    }
}

static void button_task(void *arg)
{
    (void)arg;
    bool previous_pressed = false;
    int64_t last_press_us = 0;

    while (true) {
        const bool pressed = gpio_get_level(SW3_GPIO) == 0;
        const int64_t now_us = esp_timer_get_time();
        if (pressed && !previous_pressed && now_us - last_press_us > BUTTON_DEBOUNCE_MS * 1000LL) {
            if (lvgl_port_lock(100)) {
                toggle_measurement();
                lvgl_port_unlock();
                last_press_us = now_us;
            }
        }
        previous_pressed = pressed;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void build_heart_screen(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x071632), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);

    create_text(screen, "VITAL // SENSE", 15, 10, &lv_font_montserrat_14, COLOR_CYAN);
    create_text(screen, "24H ANALYTICS", 164, 11, &lv_font_montserrat_14, COLOR_DIM);

    lv_obj_t *top_card = create_panel(screen, 10, 33, 260, 94);
    create_text(top_card, "PULSE", 17, 36, &lv_font_montserrat_14, COLOR_PINK);

    g_arc = lv_arc_create(top_card);
    lv_obj_set_pos(g_arc, 12, 12);
    lv_obj_set_size(g_arc, 76, 76);
    lv_arc_set_range(g_arc, 45, 150);
    lv_arc_set_bg_angles(g_arc, 0, 270);
    lv_arc_set_rotation(g_arc, 135);
    lv_arc_set_value(g_arc, g_bpm);
    lv_obj_remove_flag(g_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(g_arc, 7, LV_PART_MAIN);
    lv_obj_set_style_arc_color(g_arc, COLOR_GRID, LV_PART_MAIN);
    lv_obj_set_style_arc_width(g_arc, 7, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(g_arc, COLOR_CYAN, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(g_arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_opa(g_arc, LV_OPA_TRANSP, LV_PART_KNOB);

    g_bpm_label = create_text(top_card, "78", 106, 15, &lv_font_montserrat_32, COLOR_TEXT);
    create_text(top_card, "BPM", 181, 37, &lv_font_montserrat_16, COLOR_CYAN);
    g_state_label = create_text(top_card, "READY", 108, 58, &lv_font_montserrat_14, COLOR_CYAN);
    create_text(top_card, "REST 62", 108, 77, &lv_font_montserrat_14, COLOR_DIM);
    create_text(top_card, "OXY 98%", 183, 77, &lv_font_montserrat_14, COLOR_DIM);

    lv_obj_t *chart_card = create_panel(screen, 10, 137, 260, 84);
    create_text(chart_card, "HEART RHYTHM // 24 HOURS", 13, 9, &lv_font_montserrat_14, COLOR_TEXT);
    create_text(chart_card, "150", 5, 26, &lv_font_montserrat_14, COLOR_DIM);
    create_text(chart_card, "90", 9, 48, &lv_font_montserrat_14, COLOR_DIM);
    create_text(chart_card, "45", 9, 67, &lv_font_montserrat_14, COLOR_DIM);

    g_chart = lv_chart_create(chart_card);
    lv_obj_set_pos(g_chart, 31, 24);
    lv_obj_set_size(g_chart, 217, 47);
    lv_chart_set_type(g_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(g_chart, 24);
    lv_chart_set_axis_range(g_chart, LV_CHART_AXIS_PRIMARY_Y, 45, 150);
    lv_chart_set_div_line_count(g_chart, 3, 4);
    lv_chart_set_update_mode(g_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_obj_set_style_bg_opa(g_chart, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_color(g_chart, COLOR_GRID, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_chart, 1, LV_PART_MAIN);
    lv_obj_set_style_line_color(g_chart, COLOR_GRID, LV_PART_MAIN);
    lv_obj_set_style_line_width(g_chart, 1, LV_PART_MAIN);
    lv_obj_set_style_pad_left(g_chart, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_right(g_chart, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_top(g_chart, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(g_chart, 2, LV_PART_MAIN);
    lv_obj_set_style_line_color(g_chart, COLOR_MAGENTA, LV_PART_ITEMS);
    lv_obj_set_style_line_width(g_chart, 3, LV_PART_ITEMS);
    lv_obj_set_style_size(g_chart, 4, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(g_chart, COLOR_CYAN, LV_PART_INDICATOR);
    lv_obj_set_style_radius(g_chart, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);

    g_series = lv_chart_add_series(g_chart, COLOR_MAGENTA, LV_CHART_AXIS_PRIMARY_Y);
    for (uint32_t point = 0; point < 24; ++point) {
        lv_chart_set_series_value_by_id(g_chart, g_series, point, HEART_24H[point]);
    }
    lv_chart_refresh(g_chart);

    create_text(chart_card, "00", 28, 70, &lv_font_montserrat_14, COLOR_DIM);
    create_text(chart_card, "06", 82, 70, &lv_font_montserrat_14, COLOR_DIM);
    create_text(chart_card, "12", 136, 70, &lv_font_montserrat_14, COLOR_DIM);
    create_text(chart_card, "18", 190, 70, &lv_font_montserrat_14, COLOR_DIM);
    create_text(chart_card, "24", 231, 70, &lv_font_montserrat_14, COLOR_DIM);

    g_action_label = create_text(screen, "SW3  START MEASUREMENT", 55, 226,
                                  &lv_font_montserrat_14, COLOR_CYAN);
    set_measurement_state(false);
}

void app_main(void)
{
    lcd_st7789_t lcd;
    ESP_ERROR_CHECK(lcd_st7789_init_panel(&lcd));

    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = lcd.io,
        .panel_handle = lcd.panel,
        .control_handle = NULL,
        .buffer_size = LCD_WIDTH * LVGL_DRAW_LINES,
        .double_buffer = true,
        .trans_size = LCD_WIDTH * LVGL_TRANS_LINES,
        .hres = LCD_WIDTH,
        .vres = LCD_HEIGHT,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .rounder_cb = NULL,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = false,
            .buff_spiram = true,
            .sw_rotate = false,
            .swap_bytes = false,
            .full_refresh = false,
            .direct_mode = false,
        },
    };
    lv_display_t *display = lvgl_port_add_disp(&display_cfg);
    ESP_ERROR_CHECK(display == NULL ? ESP_FAIL : ESP_OK);

    gpio_config_t button_config = {
        .pin_bit_mask = 1ULL << SW3_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&button_config));

    ESP_LOGI(TAG, "LVGL heart screen: PSRAM total=%u free=%u bytes",
             (unsigned)esp_psram_get_size(),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    if (lvgl_port_lock(0)) {
        build_heart_screen();
        lv_timer_create(measurement_timer_cb, 180, NULL);
        lvgl_port_unlock();
    }

    BaseType_t task_created = xTaskCreate(button_task, "sw3_button", 3072, NULL, 3, NULL);
    ESP_ERROR_CHECK(task_created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
