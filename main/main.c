// main/main.c —— FoloToy AI Passport 随身安全哨兵 (AI Security Sentinel) 全中文主入口
//
// 按键语义 (遵循官方代码约定全局统一):
//   上/下 短按   菜单中=移动选中项; 演示页中=该页自定义
//   确定  短按   菜单中=进入选中项; 演示页中=该页自定义
//   确定  长按   演示页中=返回菜单 (由本文件统一拦截并退出)
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_pins.h"
#include "demo.h"
#include "ui_pixel.h"
#include "tracker_engine.h"
#include "tracker_alarm.h"
#include "tracker_wifi_spy.h"
#include "tracker_ble_scanner.h"
#include "tracker_ui.h"
#include "tracker_power.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_sleep.h"

static const char *TAG = "main";

LV_FONT_DECLARE(lv_font_cn_16);

static const demo_entry_t DEMOS[] = {
    { "防追踪雷达", demo_radar_enter, demo_radar_exit, demo_radar_key },
    { "冷热寻物",   demo_hotcold_enter, demo_hotcold_exit, demo_hotcold_key },
    { "偷拍排查",   demo_wifi_spy_enter, demo_wifi_spy_exit, demo_wifi_spy_key },
    { "白名单",     demo_whitelist_enter, demo_whitelist_exit, demo_whitelist_key },
};
#define DEMO_COUNT (sizeof(DEMOS) / sizeof(DEMOS[0]))

static bool s_ok[DEMO_COUNT];
static lv_obj_t *s_menu_scr = NULL;
static lv_obj_t *s_cards[DEMO_COUNT];
static lv_obj_t *s_rows[DEMO_COUNT];
static lv_obj_t *s_mascot = NULL;
static int s_sel = 0;
static int s_active = -1;

static void menu_refresh(void) {
    for (size_t i = 0; i < DEMO_COUNT; i++) {
        lv_label_set_text(s_rows[i], DEMOS[i].name);
        ui_pixel_set_selected(s_cards[i], (int)i == s_sel, s_ok[i]);
        lv_obj_set_style_text_color(s_rows[i],
            s_ok[i] ? lv_color_hex(UI_INK) : lv_color_hex(0x7A2020), 0);
    }
}

static void menu_build(void) {
    s_menu_scr = ui_pixel_screen_create("安全哨兵");

    // 右上角电量显示 (不遮挡白云)
    int soc = bsp_battery_soc();
    lv_obj_t *soc_lbl = lv_label_create(s_menu_scr);
    lv_obj_set_style_text_font(soc_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(soc_lbl, lv_color_hex(UI_INK), 0);
    lv_obj_set_pos(soc_lbl, 180, 16);
    if (soc < 0) lv_label_set_text(soc_lbl, "--%");
    else         lv_label_set_text_fmt(soc_lbl, "%d%%", soc);

    // 2x2 像素风格卡片
    for (size_t i = 0; i < DEMO_COUNT; i++) {
        int x = 11 + (int)(i % 2) * 112;
        int y = 54 + (int)(i / 2) * 56;
        s_cards[i] = ui_pixel_panel_create(s_menu_scr, x, y, 102, 48, UI_PAPER);
        s_rows[i] = lv_label_create(s_cards[i]);
        lv_obj_set_style_text_font(s_rows[i], &lv_font_cn_16, 0);
        lv_obj_set_style_text_align(s_rows[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(s_rows[i]);
    }

    // 跳跃吉祥物
    s_mascot = ui_pixel_mascot_create(s_menu_scr, 101, 238);

    // 底部按键提示
    lv_obj_t *hint = lv_label_create(s_menu_scr);
    lv_obj_set_style_text_font(hint, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(UI_INK), 0);
    lv_obj_set_width(hint, 232);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(hint, 4, 292);
    lv_label_set_text(hint, "上下:选择功能 | OK:进入功能");

    menu_refresh();
    lv_screen_load(s_menu_scr);
}

static void enter_menu(void) {
    s_active = -1;
    menu_build();
}

static void on_power_timer(lv_timer_t *t) {
    (void)t;
    tracker_power_tick(0);
}

static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user) {
    (void)user;
    if (!bsp_lvgl_lock(500)) return;

    // 智能熄屏拦截: 若屏幕处于熄灭休眠状态，首按仅唤醒屏幕并消费事件，防止口袋/背包误触
    if (tracker_power_handle_key_event(btn, ev)) {
        bsp_lvgl_unlock();
        return;
    }

    if (s_active >= 0) {
        // 核心规范: 在任何子功能页面中长按 OK 键，统一拦截并返回安全哨兵主菜单
        if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG) {
            ESP_LOGI(TAG, "全局拦截: 长按确定键返回安全哨兵主菜单");
            DEMOS[s_active].exit();
            tracker_alarm_beep(1200, 60);
            enter_menu();
        } else {
            DEMOS[s_active].key(btn, ev);
        }
    } else if (ev == BSP_BTN_CLICK) {
        if (btn == BSP_BTN_UP) {
            s_sel = (s_sel + DEMO_COUNT - 1) % DEMO_COUNT;
            tracker_alarm_beep(2000, 20);
            menu_refresh();
            ui_pixel_mascot_jump(s_mascot);
        } else if (btn == BSP_BTN_DOWN) {
            s_sel = (s_sel + 1) % DEMO_COUNT;
            tracker_alarm_beep(2000, 20);
            menu_refresh();
            ui_pixel_mascot_jump(s_mascot);
        } else if (btn == BSP_BTN_OK && s_ok[s_sel]) {
            s_active = s_sel;
            tracker_alarm_beep(1600, 60);
            ui_pixel_mascot_jump(s_mascot);
            lv_obj_delete(s_menu_scr);
            s_menu_scr = NULL;
            s_mascot = NULL;
            DEMOS[s_active].enter();
        }
    }
    bsp_lvgl_unlock();
}

void app_main(void) {
    ESP_LOGI(TAG, "FoloToy AI Passport 随身安全哨兵启动");
    esp_sleep_wakeup_cause_t wakeup = esp_sleep_get_wakeup_cause();
    if (wakeup != ESP_SLEEP_WAKEUP_UNDEFINED) {
        ESP_LOGI(TAG, "休眠唤醒原因: %d", wakeup);
    }

    bsp_i2c_init();
    bsp_i2c_scan();

    // 初始化屏幕与 LVGL
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "显示/LVGL 初始化失败");
        return;
    }
    bsp_display_backlight(100);

    // 初始化按键、音频、电池
    bsp_button_init(on_key, NULL);
    bsp_audio_init();
    bsp_battery_init();

    // 初始化随身安全哨兵底层引擎 (BLE 监听、Wi-Fi 探测、威胁状态机、蜂鸣器、智能电源)
    tracker_engine_init();
    tracker_alarm_init();
    tracker_wifi_spy_init();
    tracker_power_init();
    tracker_ble_scanner_start();

    for (size_t i = 0; i < DEMO_COUNT; i++) {
        s_ok[i] = true;
    }

    // 开机进入全中文安全哨兵功能主菜单并启动智能电源定时器
    if (bsp_lvgl_lock(1000)) {
        enter_menu();
        lv_timer_create(on_power_timer, 250, NULL);
        bsp_lvgl_unlock();
    }

    ESP_LOGI(TAG, "安全哨兵系统初始化就绪，进入主菜单");
}
