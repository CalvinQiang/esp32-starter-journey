#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_io_i2c.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_cst816s.h"
#include "lvgl.h"

#include "lcd_st7789.h"

#define SW3_GPIO                    GPIO_NUM_39
#define BUTTON_DEBOUNCE_MS           220
#define LVGL_DRAW_LINES              40

#define TOUCH_I2C_PORT               I2C_NUM_0
#define TOUCH_I2C_SCL_GPIO           GPIO_NUM_22
#define TOUCH_I2C_SDA_GPIO           GPIO_NUM_23
#define TOUCH_INT_GPIO               GPIO_NUM_35
#define TOUCH_RAW_WIDTH              240
#define TOUCH_RAW_HEIGHT             280

#define WATCH_PAGE_COUNT             4
#define SWIPE_THRESHOLD_PX           34
#define PAGE_ANIM_MS                 240

#define COLOR_BG                     lv_color_hex(0x030817)
#define COLOR_PANEL                  lv_color_hex(0x08142A)
#define COLOR_GRID                   lv_color_hex(0x18355C)
#define COLOR_CYAN                   lv_color_hex(0x32E8FF)
#define COLOR_MAGENTA                lv_color_hex(0xD144FF)
#define COLOR_PINK                   lv_color_hex(0xFF4D8D)
#define COLOR_GREEN                  lv_color_hex(0x36F7A2)
#define COLOR_ORANGE                 lv_color_hex(0xFFB547)
#define COLOR_TEXT                   lv_color_hex(0xE7F6FF)
#define COLOR_DIM                    lv_color_hex(0x6D8EAE)

static const char *TAG = "watch_touch";

static lcd_st7789_t g_lcd;
static lv_display_t *g_display;
static esp_timer_handle_t g_lvgl_tick_timer;
static i2c_master_bus_handle_t g_touch_i2c_bus;
static esp_lcd_panel_io_handle_t g_touch_io;
static esp_lcd_touch_handle_t g_touch;
static lv_indev_t *g_touch_indev;
static lv_color_t *g_draw_buf_1;
static lv_color_t *g_draw_buf_2;

static lv_obj_t *g_screens[WATCH_PAGE_COUNT];
static uint8_t g_current_page;

static lv_obj_t *g_bpm_label;
static lv_obj_t *g_state_label;
static lv_obj_t *g_arc;
static lv_obj_t *g_chart;
static lv_chart_series_t *g_series;
static lv_obj_t *g_action_label;
static lv_obj_t *g_clock_label;
static lv_obj_t *g_focus_label;
static bool g_measuring;
static int g_bpm = 78;
static uint32_t g_phase;
static uint32_t g_clock_seconds = 20 * 3600 + 26 * 60;
static uint32_t g_focus_seconds = 25 * 60;

static volatile bool g_touch_irq_pending;
static bool g_touch_pressed;
static lv_point_t g_touch_last;
static lv_point_t g_swipe_start;
static bool g_swipe_tracking;

static const int32_t HEART_24H[] = {
    63, 61, 60, 59, 60, 62, 67, 74, 81, 86, 83, 79,
    76, 78, 84, 91, 88, 82, 77, 73, 70, 68, 66, 64,
};

static void lvgl_tick_callback(void *arg)
{
    (void)arg;
    lv_tick_inc(1);
}

static void IRAM_ATTR touch_int_isr(void *arg)
{
    (void)arg;
    g_touch_irq_pending = true;
}

static bool on_color_transfer_done(esp_lcd_panel_io_handle_t io,
                                   esp_lcd_panel_io_event_data_t *event_data,
                                   void *user_ctx)
{
    (void)io;
    (void)event_data;
    lv_display_flush_ready((lv_display_t *)user_ctx);
    return false;
}

static void st7789_lvgl_flush(lv_display_t *display, const lv_area_t *area, uint8_t *pixel_map)
{
    const esp_err_t result = esp_lcd_panel_draw_bitmap(g_lcd.panel,
                                                        area->x1,
                                                        area->y1,
                                                        area->x2 + 1,
                                                        area->y2 + 1,
                                                        pixel_map);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "SPI flush failed: %s", esp_err_to_name(result));
        lv_display_flush_ready(display);
    }
}

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

static lv_obj_t *create_screen_base(const char *title, const char *page_id)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, COLOR_BG, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x071632), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);

    create_text(screen, title, 15, 10, &lv_font_montserrat_14, COLOR_CYAN);
    create_text(screen, page_id, 225, 10, &lv_font_montserrat_14, COLOR_DIM);
    lv_obj_t *scan_line = lv_obj_create(screen);
    lv_obj_remove_flag(scan_line, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(scan_line, 15, 28);
    lv_obj_set_size(scan_line, 250, 1);
    lv_obj_set_style_bg_color(scan_line, COLOR_GRID, LV_PART_MAIN);
    lv_obj_set_style_border_width(scan_line, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(scan_line, 0, LV_PART_MAIN);
    return screen;
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

static void clock_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    char time_text[12];
    ++g_clock_seconds;
    const uint32_t hour = (g_clock_seconds / 3600) % 24;
    const uint32_t minute = (g_clock_seconds / 60) % 60;
    snprintf(time_text, sizeof(time_text), "%02lu:%02lu", (unsigned long)hour, (unsigned long)minute);
    lv_label_set_text(g_clock_label, time_text);

    if (g_focus_seconds > 0) {
        --g_focus_seconds;
    }
    snprintf(time_text, sizeof(time_text), "%02lu:%02lu",
             (unsigned long)(g_focus_seconds / 60),
             (unsigned long)(g_focus_seconds % 60));
    lv_label_set_text(g_focus_label, time_text);
}

static lv_screen_load_anim_t animation_for_delta(int delta)
{
    switch (delta) {
    case -1:
        return LV_SCREEN_LOAD_ANIM_MOVE_RIGHT;
    case 1:
        return LV_SCREEN_LOAD_ANIM_MOVE_LEFT;
    case -2:
        return LV_SCREEN_LOAD_ANIM_MOVE_TOP;
    default:
        return LV_SCREEN_LOAD_ANIM_MOVE_BOTTOM;
    }
}

static void load_page(uint8_t target_page, lv_screen_load_anim_t animation)
{
    if (target_page >= WATCH_PAGE_COUNT || target_page == g_current_page) {
        return;
    }
    g_current_page = target_page;
    lv_screen_load_anim(g_screens[target_page], animation, PAGE_ANIM_MS, 0, false);
    ESP_LOGI(TAG, "Navigation: loaded page %u", (unsigned)(target_page + 1));
}

static void handle_swipe(int32_t dx, int32_t dy)
{
    const int32_t abs_x = dx < 0 ? -dx : dx;
    const int32_t abs_y = dy < 0 ? -dy : dy;
    if (abs_x < SWIPE_THRESHOLD_PX && abs_y < SWIPE_THRESHOLD_PX) {
        return;
    }

    if (abs_x >= abs_y) {
        if (dx < 0) {
            load_page((g_current_page + 1) % WATCH_PAGE_COUNT, animation_for_delta(1));
        } else {
            load_page((g_current_page + WATCH_PAGE_COUNT - 1) % WATCH_PAGE_COUNT,
                      animation_for_delta(-1));
        }
    } else if (dy < 0) {
        load_page(1, animation_for_delta(-2));
    } else {
        load_page(3, animation_for_delta(2));
    }
}

static void handle_cst816s_gesture(uint8_t gesture)
{
    switch (gesture) {
    case 0x01: /* CST816S: slide down */
        handle_swipe(0, SWIPE_THRESHOLD_PX);
        break;
    case 0x02: /* CST816S: slide up */
        handle_swipe(0, -SWIPE_THRESHOLD_PX);
        break;
    case 0x03: /* CST816S: slide left */
        handle_swipe(-SWIPE_THRESHOLD_PX, 0);
        break;
    case 0x04: /* CST816S: slide right */
        handle_swipe(SWIPE_THRESHOLD_PX, 0);
        break;
    default:
        return;
    }
    ESP_LOGI(TAG, "CST816S hardware gesture=0x%02X", gesture);
}

static void gesture_surface_event_cb(lv_event_t *event)
{
    const lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *indev = lv_indev_active();
    if (indev == NULL) {
        return;
    }

    lv_point_t point;
    lv_indev_get_point(indev, &point);
    if (code == LV_EVENT_PRESSED) {
        g_swipe_start = point;
        g_swipe_tracking = true;
    } else if (code == LV_EVENT_RELEASED && g_swipe_tracking) {
        g_swipe_tracking = false;
        handle_swipe((int32_t)point.x - g_swipe_start.x, (int32_t)point.y - g_swipe_start.y);
    }
}

static void add_gesture_surface(lv_obj_t *screen)
{
    lv_obj_t *surface = lv_obj_create(screen);
    lv_obj_remove_flag(surface, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(surface, 0, 0);
    lv_obj_set_size(surface, LCD_WIDTH, LCD_HEIGHT);
    lv_obj_set_style_bg_opa(surface, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(surface, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(surface, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(surface, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(surface, gesture_surface_event_cb, LV_EVENT_ALL, NULL);
}

static void build_heart_screen(void)
{
    lv_obj_t *screen = create_screen_base("VITAL // SENSE", "01 / 04");
    g_screens[0] = screen;

    lv_obj_t *top_card = create_panel(screen, 10, 36, 260, 91);
    create_text(top_card, "PULSE", 17, 13, &lv_font_montserrat_14, COLOR_PINK);

    g_arc = lv_arc_create(top_card);
    lv_obj_set_pos(g_arc, 12, 10);
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

    g_bpm_label = create_text(top_card, "78", 106, 10, &lv_font_montserrat_32, COLOR_TEXT);
    create_text(top_card, "BPM", 181, 32, &lv_font_montserrat_16, COLOR_CYAN);
    g_state_label = create_text(top_card, "READY", 108, 53, &lv_font_montserrat_14, COLOR_CYAN);
    create_text(top_card, "REST 62", 108, 72, &lv_font_montserrat_14, COLOR_DIM);
    create_text(top_card, "OXY 98%", 183, 72, &lv_font_montserrat_14, COLOR_DIM);

    lv_obj_t *chart_card = create_panel(screen, 10, 137, 260, 76);
    create_text(chart_card, "HEART RHYTHM // 24 HOURS", 13, 8, &lv_font_montserrat_14, COLOR_TEXT);
    create_text(chart_card, "150", 5, 26, &lv_font_montserrat_14, COLOR_DIM);
    create_text(chart_card, "90", 9, 45, &lv_font_montserrat_14, COLOR_DIM);
    create_text(chart_card, "45", 9, 62, &lv_font_montserrat_14, COLOR_DIM);

    g_chart = lv_chart_create(chart_card);
    lv_obj_set_pos(g_chart, 31, 23);
    lv_obj_set_size(g_chart, 217, 43);
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

    g_action_label = create_text(screen, "SW3  START MEASUREMENT", 48, 220,
                                  &lv_font_montserrat_14, COLOR_CYAN);
    set_measurement_state(false);
    add_gesture_surface(screen);
}

static void build_clock_screen(void)
{
    lv_obj_t *screen = create_screen_base("ORBIT // TIME", "02 / 04");
    g_screens[1] = screen;

    lv_obj_t *clock_card = create_panel(screen, 10, 39, 260, 125);
    create_text(clock_card, "LOCAL TIME", 16, 15, &lv_font_montserrat_14, COLOR_DIM);
    g_clock_label = create_text(clock_card, "20:26", 44, 37, &lv_font_montserrat_32, COLOR_TEXT);
    create_text(clock_card, "THU 14 AUG 2026", 60, 83, &lv_font_montserrat_16, COLOR_CYAN);

    lv_obj_t *orbit = lv_arc_create(clock_card);
    lv_obj_set_pos(orbit, 183, 10);
    lv_obj_set_size(orbit, 64, 64);
    lv_arc_set_range(orbit, 0, 100);
    lv_arc_set_value(orbit, 72);
    lv_arc_set_bg_angles(orbit, 0, 300);
    lv_arc_set_rotation(orbit, 120);
    lv_obj_remove_flag(orbit, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(orbit, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_color(orbit, COLOR_GRID, LV_PART_MAIN);
    lv_obj_set_style_arc_width(orbit, 5, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(orbit, COLOR_ORANGE, LV_PART_INDICATOR);
    lv_obj_set_style_opa(orbit, LV_OPA_TRANSP, LV_PART_KNOB);

    lv_obj_t *status = create_panel(screen, 10, 176, 260, 36);
    create_text(status, "SYNC", 14, 10, &lv_font_montserrat_14, COLOR_GREEN);
    create_text(status, "TAP + SWIPE NAVIGATION ONLINE", 67, 10, &lv_font_montserrat_14, COLOR_TEXT);
    create_text(screen, "UP: TIME  •  DOWN: FOCUS", 54, 220, &lv_font_montserrat_14, COLOR_DIM);
    add_gesture_surface(screen);
}

static void build_system_screen(void)
{
    lv_obj_t *screen = create_screen_base("CORE // STATUS", "03 / 04");
    g_screens[2] = screen;

    lv_obj_t *identity = create_panel(screen, 10, 38, 260, 59);
    create_text(identity, "ESP32-WROOM-32E", 16, 11, &lv_font_montserrat_16, COLOR_TEXT);
    create_text(identity, "D0WDR2-V3  //  DUAL CORE", 16, 33, &lv_font_montserrat_14, COLOR_CYAN);

    lv_obj_t *cpu = create_panel(screen, 10, 109, 124, 48);
    create_text(cpu, "CPU", 12, 8, &lv_font_montserrat_14, COLOR_DIM);
    create_text(cpu, "240 MHz", 12, 25, &lv_font_montserrat_16, COLOR_GREEN);

    lv_obj_t *ram = create_panel(screen, 146, 109, 124, 48);
    create_text(ram, "PSRAM", 12, 8, &lv_font_montserrat_14, COLOR_DIM);
    create_text(ram, "2.00 MB", 12, 25, &lv_font_montserrat_16, COLOR_MAGENTA);

    lv_obj_t *bus = create_panel(screen, 10, 169, 260, 42);
    create_text(bus, "TOUCH BUS", 13, 8, &lv_font_montserrat_14, COLOR_DIM);
    create_text(bus, "CST816S  •  I2C 0x15  •  ONLINE", 13, 24,
                &lv_font_montserrat_14, COLOR_CYAN);
    create_text(screen, "LEFT / RIGHT TO CYCLE", 67, 220, &lv_font_montserrat_14, COLOR_DIM);
    add_gesture_surface(screen);
}

static void build_focus_screen(void)
{
    lv_obj_t *screen = create_screen_base("FOCUS // FLOW", "04 / 04");
    g_screens[3] = screen;

    lv_obj_t *focus_card = create_panel(screen, 10, 39, 260, 119);
    create_text(focus_card, "DEEP WORK PROTOCOL", 17, 14, &lv_font_montserrat_14, COLOR_CYAN);
    g_focus_label = create_text(focus_card, "25:00", 64, 38, &lv_font_montserrat_32, COLOR_TEXT);
    create_text(focus_card, "NEXT BREAK // 5 MIN", 63, 83, &lv_font_montserrat_14, COLOR_DIM);

    lv_obj_t *progress = lv_bar_create(focus_card);
    lv_obj_set_pos(progress, 25, 103);
    lv_obj_set_size(progress, 210, 6);
    lv_bar_set_range(progress, 0, 100);
    lv_bar_set_value(progress, 70, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(progress, COLOR_GRID, LV_PART_MAIN);
    lv_obj_set_style_bg_color(progress, COLOR_MAGENTA, LV_PART_INDICATOR);
    lv_obj_set_style_radius(progress, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_radius(progress, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);

    lv_obj_t *signal = create_panel(screen, 10, 171, 260, 40);
    create_text(signal, "MODE", 14, 9, &lv_font_montserrat_14, COLOR_DIM);
    create_text(signal, "SILENT SIGNAL  //  70% COMPLETE", 68, 9,
                &lv_font_montserrat_14, COLOR_GREEN);
    create_text(screen, "DOWN: FOCUS  •  SW3: HOME", 51, 220, &lv_font_montserrat_14, COLOR_DIM);
    add_gesture_surface(screen);
}

static void init_touch(void)
{
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = TOUCH_I2C_PORT,
        .sda_io_num = TOUCH_I2C_SDA_GPIO,
        .scl_io_num = TOUCH_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags.enable_internal_pullup = false,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &g_touch_i2c_bus));

    const esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(g_touch_i2c_bus, &io_config, &g_touch_io));

    const esp_lcd_touch_config_t touch_config = {
        .x_max = TOUCH_RAW_WIDTH - 1,
        .y_max = TOUCH_RAW_HEIGHT - 1,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        /* LCD MADCTL=0x60 maps logical (x,y) to native touch (y, 239-x). */
        .flags = {
            .swap_xy = true,
            .mirror_x = true,
            .mirror_y = false,
        },
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(g_touch_io, &touch_config, &g_touch));

    const gpio_config_t int_config = {
        .pin_bit_mask = 1ULL << TOUCH_INT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&int_config));
    const esp_err_t isr_result = gpio_install_isr_service(0);
    ESP_ERROR_CHECK((isr_result == ESP_OK || isr_result == ESP_ERR_INVALID_STATE) ? ESP_OK : isr_result);
    ESP_ERROR_CHECK(gpio_isr_handler_add(TOUCH_INT_GPIO, touch_int_isr, NULL));

    g_touch_indev = lv_indev_create();
    ESP_ERROR_CHECK(g_touch_indev == NULL ? ESP_ERR_NO_MEM : ESP_OK);
    lv_indev_set_type(g_touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_disp(g_touch_indev, g_display);

    ESP_LOGI(TAG, "Touch online: CST816S at I2C 0x15; raw=%dx%d -> LVGL=%dx%d",
             TOUCH_RAW_WIDTH, TOUCH_RAW_HEIGHT, LCD_WIDTH, LCD_HEIGHT);
}

static void touch_indev_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    const bool int_low = gpio_get_level(TOUCH_INT_GPIO) == 0;

    if (g_touch_irq_pending) {
        g_touch_irq_pending = false;
        uint8_t gesture = 0;
        const esp_err_t gesture_result = esp_lcd_panel_io_rx_param(g_touch_io, 0x01, &gesture, 1);
        if (gesture_result == ESP_OK) {
            handle_cst816s_gesture(gesture);
        }

        esp_lcd_touch_point_data_t touch_point;
        uint8_t point_count = 0;
        const esp_err_t read_result = esp_lcd_touch_read_data(g_touch);
        const esp_err_t data_result = read_result == ESP_OK
            ? esp_lcd_touch_get_data(g_touch, &touch_point, &point_count, 1)
            : read_result;
        if (data_result == ESP_OK && point_count > 0) {
            g_touch_last.x = touch_point.x < LCD_WIDTH ? touch_point.x : LCD_WIDTH - 1;
            g_touch_last.y = touch_point.y < LCD_HEIGHT ? touch_point.y : LCD_HEIGHT - 1;
            g_touch_pressed = true;
            ESP_LOGI(TAG, "Touch point: LVGL=(%d,%d)", g_touch_last.x, g_touch_last.y);
        } else if (data_result != ESP_OK) {
            ESP_LOGW(TAG, "Touch I2C read failed: %s", esp_err_to_name(data_result));
            g_touch_pressed = false;
        }
    }

    if (!int_low) {
        g_touch_irq_pending = false;
        g_touch_pressed = false;
    }

    data->point = g_touch_last;
    data->state = g_touch_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void button_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    static int previous_level = 1;
    static uint32_t last_press_ms;
    const int level = gpio_get_level(SW3_GPIO);
    const bool pressed = level == 0;
    const bool new_press = pressed && previous_level != 0;
    const uint32_t now_ms = lv_tick_get();

    if (level != previous_level) {
        ESP_LOGI(TAG, "SW3 GPIO39 raw level=%d (%s)", level, pressed ? "pressed" : "released");
        previous_level = level;
    }

    if (new_press && now_ms - last_press_ms > BUTTON_DEBOUNCE_MS) {
        if (g_current_page == 0) {
            set_measurement_state(!g_measuring);
            if (g_measuring) {
                g_phase = 0;
            }
            ESP_LOGI(TAG, "SW3 event accepted: measurement %s", g_measuring ? "started" : "stopped");
        } else {
            load_page(0, LV_SCREEN_LOAD_ANIM_FADE_IN);
            ESP_LOGI(TAG, "SW3 event accepted: returned to VITAL page");
        }
        last_press_ms = now_ms;
    }
}

static void lvgl_task(void *arg)
{
    (void)arg;
    while (true) {
        uint32_t wait_ms = lv_timer_handler();
        if (wait_ms > 20) {
            wait_ms = 20;
        }
        vTaskDelay(pdMS_TO_TICKS(wait_ms));
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(lcd_st7789_init_panel(&g_lcd));

    const size_t draw_buffer_bytes = LCD_WIDTH * LVGL_DRAW_LINES * sizeof(lv_color_t);
    g_draw_buf_1 = heap_caps_malloc(draw_buffer_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    g_draw_buf_2 = heap_caps_malloc(draw_buffer_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    ESP_ERROR_CHECK((g_draw_buf_1 != NULL && g_draw_buf_2 != NULL) ? ESP_OK : ESP_ERR_NO_MEM);

    lv_init();
    const esp_timer_create_args_t tick_timer_args = {
        .callback = lvgl_tick_callback,
        .name = "lvgl_tick",
    };
    ESP_ERROR_CHECK(esp_timer_create(&tick_timer_args, &g_lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(g_lvgl_tick_timer, 1000));

    g_display = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    ESP_ERROR_CHECK(g_display == NULL ? ESP_ERR_NO_MEM : ESP_OK);
    lv_display_set_color_format(g_display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(g_display, g_draw_buf_1, g_draw_buf_2, draw_buffer_bytes,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(g_display, st7789_lvgl_flush);

    const esp_lcd_panel_io_callbacks_t callbacks = {
        .on_color_trans_done = on_color_transfer_done,
    };
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(g_lcd.io, &callbacks, g_display));

    const gpio_config_t button_config = {
        .pin_bit_mask = 1ULL << SW3_GPIO,
        .mode = GPIO_MODE_INPUT,
        /* GPIO39 is input-only and has no internal pull resistor; the board's hardware pull-up is used. */
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&button_config));

    ESP_LOGI(TAG, "Watch UI: PSRAM total=%u free=%u, draw buffers=%u bytes each",
             (unsigned)esp_psram_get_size(),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             (unsigned)draw_buffer_bytes);

    build_heart_screen();
    build_clock_screen();
    build_system_screen();
    build_focus_screen();
    lv_screen_load(g_screens[0]);

    init_touch();
    lv_indev_set_read_cb(g_touch_indev, touch_indev_read);

    lv_timer_create(measurement_timer_cb, 180, NULL);
    lv_timer_create(clock_timer_cb, 1000, NULL);
    lv_timer_create(button_timer_cb, 20, NULL);
    const BaseType_t task_created = xTaskCreate(lvgl_task, "lvgl_ui", 7168, NULL, 5, NULL);
    ESP_ERROR_CHECK(task_created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
