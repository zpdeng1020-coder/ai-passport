// main/cosmic_ui.c —— 星原探针四大核心模块 UI 视图实现
#include "cosmic_ui.h"
#include "cosmic_types.h"
#include "cosmic_sonar.h"
#include "cosmic_quiet_scout.h"
#include "cosmic_orbit_weather.h"
#include "cosmic_sos_beacon.h"
#include "cosmic_power.h"
#include "bsp_display.h"
#include "bsp_battery.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(lv_font_cn_16);

// ============================================================================
// 样式与调色板
// ============================================================================
#define COLOR_BG_SPACE     0x0B0F19   // 深邃星空蓝黑
#define COLOR_CYAN_STAR    0x00E5FF   // 冰青星芒
#define COLOR_GOLD_SUN     0xFFD54F   // 暖金日冕
#define COLOR_RED_FLARE    0xFF3D00   // 耀斑赤红
#define COLOR_CARD_BG      0x161C2E   // 卡片深底
#define COLOR_TEXT_MUTED   0x8892B0   // 柔和灰蓝

// ----------------------------------------------------------------------------
// 1. 宇宙射电声呐视图 (Cosmic Sonar View)
// ----------------------------------------------------------------------------
static lv_obj_t   *s_sonar_scr = NULL;
static lv_obj_t   *s_sonar_pps_lbl = NULL;
static lv_obj_t   *s_sonar_rssi_lbl = NULL;
static lv_obj_t   *s_sonar_flares_lbl = NULL;
static lv_obj_t   *s_sonar_audio_lbl = NULL;
static lv_obj_t   *s_sonar_canvas = NULL;
static lv_timer_t *s_sonar_timer = NULL;
static lv_color_t  s_canvas_buf[COSMIC_SPECTRUM_ROWS * COSMIC_SPECTRUM_BINS];

static void sonar_timer_cb(lv_timer_t *t) {
    (void)t;
    cosmic_sonar_tick();

    cosmic_rf_snapshot_t snap;
    cosmic_sonar_get_snapshot(&snap);

    char buf[64];
    snprintf(buf, sizeof(buf), "瞬时包率: %lu pps", (unsigned long)snap.current_pps);
    lv_label_set_text(s_sonar_pps_lbl, buf);

    snprintf(buf, sizeof(buf), "场强: %d dBm (峰值 %d)", snap.current_avg_rssi, snap.peak_rssi);
    lv_label_set_text(s_sonar_rssi_lbl, buf);

    snprintf(buf, sizeof(buf), "高能射电耀斑: %u 次", snap.flare_count);
    lv_label_set_text(s_sonar_flares_lbl, buf);

    // 绘制瀑布流矩阵
    const uint8_t *matrix = cosmic_sonar_get_waterfall_matrix();
    if (matrix && s_sonar_canvas) {
        for (int r = 0; r < COSMIC_SPECTRUM_ROWS; r++) {
            for (int c = 0; c < COSMIC_SPECTRUM_BINS; c++) {
                uint8_t val = matrix[r * COSMIC_SPECTRUM_BINS + c];
                lv_color_t color;
                if (val < 10) {
                    color = lv_color_hex(0x0A0E1A); // 虚空蓝
                } else if (val < 50) {
                    color = lv_color_hex(0x005577); // 青蓝
                } else if (val < 120) {
                    color = lv_color_hex(0x00CC88); // 翠绿
                } else if (val < 200) {
                    color = lv_color_hex(0xFFAA00); // 耀金
                } else {
                    color = lv_color_hex(0xFF0055); // 强脉冲粉红
                }
                s_canvas_buf[r * COSMIC_SPECTRUM_BINS + c] = color;
            }
        }
        lv_canvas_set_buffer(s_sonar_canvas, s_canvas_buf, COSMIC_SPECTRUM_BINS, COSMIC_SPECTRUM_ROWS, LV_COLOR_FORMAT_RGB565);
    }
}

void demo_cosmic_sonar_enter(void) {
    cosmic_sonar_init();
    cosmic_sonar_start();

    s_sonar_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_sonar_scr, lv_color_hex(COLOR_BG_SPACE), 0);

    // 顶部标题栏
    lv_obj_t *title = lv_label_create(s_sonar_scr);
    lv_obj_set_style_text_font(title, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(COLOR_CYAN_STAR), 0);
    lv_obj_set_pos(title, 12, 12);
    lv_label_set_text(title, "宇宙射电声呐");

    s_sonar_audio_lbl = lv_label_create(s_sonar_scr);
    lv_obj_set_style_text_font(s_sonar_audio_lbl, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(s_sonar_audio_lbl, lv_color_hex(COLOR_GOLD_SUN), 0);
    lv_obj_set_pos(s_sonar_audio_lbl, 150, 12);
    lv_label_set_text(s_sonar_audio_lbl, "[音频: 开启]");

    // 瀑布流画布容器 (宽 216, 高 130)
    lv_obj_t *canvas_box = lv_obj_create(s_sonar_scr);
    lv_obj_set_size(canvas_box, 216, 134);
    lv_obj_set_pos(canvas_box, 12, 38);
    lv_obj_set_style_bg_color(canvas_box, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_color(canvas_box, lv_color_hex(COLOR_CYAN_STAR), 0);
    lv_obj_set_style_border_width(canvas_box, 1, 0);
    lv_obj_set_style_pad_all(canvas_box, 2, 0);
    lv_obj_clear_flag(canvas_box, LV_OBJ_FLAG_SCROLLABLE);

    s_sonar_canvas = lv_canvas_create(canvas_box);
    lv_canvas_set_buffer(s_sonar_canvas, s_canvas_buf, COSMIC_SPECTRUM_BINS, COSMIC_SPECTRUM_ROWS, LV_COLOR_FORMAT_RGB565);
    lv_obj_center(s_sonar_canvas);

    // 状态详情卡片
    lv_obj_t *card = lv_obj_create(s_sonar_scr);
    lv_obj_set_size(card, 216, 92);
    lv_obj_set_pos(card, 12, 178);
    lv_obj_set_style_bg_color(card, lv_color_hex(COLOR_CARD_BG), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    s_sonar_pps_lbl = lv_label_create(card);
    lv_obj_set_style_text_font(s_sonar_pps_lbl, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(s_sonar_pps_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(s_sonar_pps_lbl, 4, 4);
    lv_label_set_text(s_sonar_pps_lbl, "瞬时包率: 0 pps");

    s_sonar_rssi_lbl = lv_label_create(card);
    lv_obj_set_style_text_font(s_sonar_rssi_lbl, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(s_sonar_rssi_lbl, lv_color_hex(COLOR_TEXT_MUTED), 0);
    lv_obj_set_pos(s_sonar_rssi_lbl, 4, 28);
    lv_label_set_text(s_sonar_rssi_lbl, "场强: -95 dBm");

    s_sonar_flares_lbl = lv_label_create(card);
    lv_obj_set_style_text_font(s_sonar_flares_lbl, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(s_sonar_flares_lbl, lv_color_hex(COLOR_GOLD_SUN), 0);
    lv_obj_set_pos(s_sonar_flares_lbl, 4, 52);
    lv_label_set_text(s_sonar_flares_lbl, "高能射电耀斑: 0 次");

    // 底部提示
    lv_obj_t *hint = lv_label_create(s_sonar_scr);
    lv_obj_set_style_text_font(hint, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(COLOR_TEXT_MUTED), 0);
    lv_obj_set_pos(hint, 12, 288);
    lv_label_set_text(hint, "UP/DOWN:音频 | 长按确定:退出");

    s_sonar_timer = lv_timer_create(sonar_timer_cb, 100, NULL);
    lv_screen_load(s_sonar_scr);
}

void demo_cosmic_sonar_exit(void) {
    if (s_sonar_timer) {
        lv_timer_delete(s_sonar_timer);
        s_sonar_timer = NULL;
    }
    cosmic_sonar_stop();
    if (s_sonar_scr) {
        lv_obj_delete(s_sonar_scr);
        s_sonar_scr = NULL;
        s_sonar_canvas = NULL;
    }
}

void demo_cosmic_sonar_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    if (ev == BSP_BTN_CLICK) {
        if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
            bool on = cosmic_sonar_toggle_audio();
            if (s_sonar_audio_lbl) {
                lv_label_set_text(s_sonar_audio_lbl, on ? "[音频: 开启]" : "[音频: 静音]");
            }
        }
    }
}

// ----------------------------------------------------------------------------
// 2. 户外暗夜与“电磁荒野”静区罗盘视图 (Radio Quiet Scout View)
// ----------------------------------------------------------------------------
static lv_obj_t   *s_scout_scr = NULL;
static lv_obj_t   *s_scout_score_lbl = NULL;
static lv_obj_t   *s_scout_level_lbl = NULL;
static lv_obj_t   *s_scout_advice_lbl = NULL;
static lv_obj_t   *s_scout_mode_lbl = NULL;
static lv_timer_t *s_scout_timer = NULL;

static void apply_astro_view_theme(cosmic_astro_view_mode_t mode) {
    bool is_red = (mode == ASTRO_VIEW_RED_NIGHT);
    uint32_t bg_color = is_red ? 0x000000 : COLOR_BG_SPACE;
    uint32_t fg_color = is_red ? 0xFF2200 : COLOR_CYAN_STAR;
    uint32_t text_color = is_red ? 0xCC1100 : 0xFFFFFF;

    if (s_scout_scr) {
        lv_obj_set_style_bg_color(s_scout_scr, lv_color_hex(bg_color), 0);
    }
    if (s_scout_score_lbl) {
        lv_obj_set_style_text_color(s_scout_score_lbl, lv_color_hex(fg_color), 0);
    }
    if (s_scout_level_lbl) {
        lv_obj_set_style_text_color(s_scout_level_lbl, lv_color_hex(fg_color), 0);
    }
    if (s_scout_advice_lbl) {
        lv_obj_set_style_text_color(s_scout_advice_lbl, lv_color_hex(text_color), 0);
    }
    if (s_scout_mode_lbl) {
        lv_label_set_text(s_scout_mode_lbl, is_red ? "[暗夜红光模式]" : "[正常色彩模式]");
        lv_obj_set_style_text_color(s_scout_mode_lbl, lv_color_hex(fg_color), 0);
    }

#ifdef ESP_PLATFORM
    // 红光模式压低背光到 15% 保护人眼暗适应并常亮
    bsp_display_backlight(is_red ? 15 : 100);
#endif
    cosmic_power_set_keep_alive(is_red);
}

static void scout_timer_cb(lv_timer_t *t) {
    (void)t;
    cosmic_rf_snapshot_t snap;
    cosmic_sonar_get_snapshot(&snap);
    cosmic_quiet_update(snap.current_pps, snap.current_avg_rssi);

    cosmic_quiet_report_t rep;
    cosmic_quiet_get_report(&rep);

    char buf[32];
    snprintf(buf, sizeof(buf), "%d", rep.rqi_score);
    lv_label_set_text(s_scout_score_lbl, buf);

    lv_label_set_text(s_scout_level_lbl, rep.level_name);
    lv_label_set_text(s_scout_advice_lbl, rep.advice);
}

void demo_quiet_scout_enter(void) {
    cosmic_quiet_init();

    s_scout_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scout_scr, lv_color_hex(COLOR_BG_SPACE), 0);

    lv_obj_t *title = lv_label_create(s_scout_scr);
    lv_obj_set_style_text_font(title, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(COLOR_CYAN_STAR), 0);
    lv_obj_set_pos(title, 12, 12);
    lv_label_set_text(title, "电磁荒野罗盘");

    s_scout_mode_lbl = lv_label_create(s_scout_scr);
    lv_obj_set_style_text_font(s_scout_mode_lbl, &lv_font_cn_16, 0);
    lv_obj_set_pos(s_scout_mode_lbl, 130, 12);
    lv_label_set_text(s_scout_mode_lbl, "[正常色彩模式]");

    // 大号分数仪表圈
    s_scout_score_lbl = lv_label_create(s_scout_scr);
    lv_obj_set_style_text_font(s_scout_score_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_scout_score_lbl, lv_color_hex(COLOR_CYAN_STAR), 0);
    lv_obj_set_pos(s_scout_score_lbl, 95, 58);
    lv_label_set_text(s_scout_score_lbl, "95");

    s_scout_level_lbl = lv_label_create(s_scout_scr);
    lv_obj_set_style_text_font(s_scout_level_lbl, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(s_scout_level_lbl, lv_color_hex(COLOR_GOLD_SUN), 0);
    lv_obj_set_pos(s_scout_level_lbl, 85, 106);
    lv_label_set_text(s_scout_level_lbl, "纯净深空");

    // 建议卡片 (支持多行换行)
    lv_obj_t *card = lv_obj_create(s_scout_scr);
    lv_obj_set_size(card, 216, 110);
    lv_obj_set_pos(card, 12, 140);
    lv_obj_set_style_bg_color(card, lv_color_hex(COLOR_CARD_BG), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 10, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    s_scout_advice_lbl = lv_label_create(card);
    lv_obj_set_style_text_font(s_scout_advice_lbl, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(s_scout_advice_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_width(s_scout_advice_lbl, 196);
    lv_label_set_text(s_scout_advice_lbl, "电磁环境极优，极适合肉眼观星与深空摄影");

    lv_obj_t *hint = lv_label_create(s_scout_scr);
    lv_obj_set_style_text_font(hint, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(COLOR_TEXT_MUTED), 0);
    lv_obj_set_pos(hint, 12, 288);
    lv_label_set_text(hint, "确定:切换观星红光 | 长按:退出");

    apply_astro_view_theme(cosmic_quiet_get_view_mode());
    s_scout_timer = lv_timer_create(scout_timer_cb, 500, NULL);
    lv_screen_load(s_scout_scr);
}

void demo_quiet_scout_exit(void) {
    if (s_scout_timer) {
        lv_timer_delete(s_scout_timer);
        s_scout_timer = NULL;
    }
    cosmic_power_set_keep_alive(false);
#ifdef ESP_PLATFORM
    bsp_display_backlight(100);
#endif
    if (s_scout_scr) {
        lv_obj_delete(s_scout_scr);
        s_scout_scr = NULL;
    }
}

void demo_quiet_scout_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK) {
        cosmic_astro_view_mode_t m = cosmic_quiet_toggle_view_mode();
        apply_astro_view_theme(m);
    }
}

// ----------------------------------------------------------------------------
// 3. 空间站过境推算与太空天气视图 (Orbit & Weather View)
// ----------------------------------------------------------------------------
static lv_obj_t   *s_orbit_scr = NULL;
static lv_obj_t   *s_orbit_sat_lbl = NULL;
static lv_obj_t   *s_orbit_region_lbl = NULL;
static lv_obj_t   *s_orbit_cd_lbl = NULL;
static lv_obj_t   *s_orbit_elev_lbl = NULL;
static lv_obj_t   *s_orbit_azim_lbl = NULL;
static lv_obj_t   *s_orbit_mag_lbl = NULL;
static lv_obj_t   *s_orbit_kp_lbl = NULL;
static lv_timer_t *s_orbit_timer = NULL;

static void orbit_timer_cb(lv_timer_t *t) {
    (void)t;
    cosmic_orbit_tick_1s();

    cosmic_orbit_pass_t pass;
    cosmic_orbit_get_pass(&pass);

    char buf[64];
    uint32_t cd = pass.countdown_sec;
    uint32_t hours = cd / 3600;
    uint32_t mins = (cd % 3600) / 60;
    uint32_t secs = cd % 60;

    snprintf(buf, sizeof(buf), "倒计时: %02lu:%02lu:%02lu", (unsigned long)hours, (unsigned long)mins, (unsigned long)secs);
    lv_label_set_text(s_orbit_cd_lbl, buf);

    snprintf(buf, sizeof(buf), "最大天顶仰角: %d°", pass.max_elevation_deg);
    lv_label_set_text(s_orbit_elev_lbl, buf);

    snprintf(buf, sizeof(buf), "过境方位: %s", pass.azimuth_desc);
    lv_label_set_text(s_orbit_azim_lbl, buf);

    snprintf(buf, sizeof(buf), "亮度星等: %d 等 (肉眼极亮)", pass.apparent_mag);
    lv_label_set_text(s_orbit_mag_lbl, buf);

    cosmic_space_weather_t w;
    cosmic_orbit_get_weather(&w);
    snprintf(buf, sizeof(buf), "地磁指数: Kp %d (%s)", w.kp_index, w.geomag_status);
    lv_label_set_text(s_orbit_kp_lbl, buf);
}

void demo_orbit_tracker_enter(void) {
    cosmic_orbit_init();

    s_orbit_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_orbit_scr, lv_color_hex(COLOR_BG_SPACE), 0);

    lv_obj_t *title = lv_label_create(s_orbit_scr);
    lv_obj_set_style_text_font(title, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(COLOR_CYAN_STAR), 0);
    lv_obj_set_pos(title, 12, 12);
    lv_label_set_text(title, "空间站过境视界");

    // 目标与大区状态行
    s_orbit_sat_lbl = lv_label_create(s_orbit_scr);
    lv_obj_set_style_text_font(s_orbit_sat_lbl, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(s_orbit_sat_lbl, lv_color_hex(COLOR_GOLD_SUN), 0);
    lv_obj_set_pos(s_orbit_sat_lbl, 12, 36);
    lv_label_set_text(s_orbit_sat_lbl, "[目标] 中国天宫空间站");

    s_orbit_region_lbl = lv_label_create(s_orbit_scr);
    lv_obj_set_style_text_font(s_orbit_region_lbl, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(s_orbit_region_lbl, lv_color_hex(COLOR_CYAN_STAR), 0);
    lv_obj_set_pos(s_orbit_region_lbl, 150, 36);
    lv_label_set_text(s_orbit_region_lbl, "华北地区");

    // 详情卡片
    lv_obj_t *card = lv_obj_create(s_orbit_scr);
    lv_obj_set_size(card, 216, 150);
    lv_obj_set_pos(card, 12, 64);
    lv_obj_set_style_bg_color(card, lv_color_hex(COLOR_CARD_BG), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    s_orbit_cd_lbl = lv_label_create(card);
    lv_obj_set_style_text_font(s_orbit_cd_lbl, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(s_orbit_cd_lbl, lv_color_hex(0x00FF88), 0);
    lv_obj_set_pos(s_orbit_cd_lbl, 4, 4);
    lv_label_set_text(s_orbit_cd_lbl, "倒计时: 00:00:00");

    s_orbit_elev_lbl = lv_label_create(card);
    lv_obj_set_style_text_font(s_orbit_elev_lbl, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(s_orbit_elev_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(s_orbit_elev_lbl, 4, 30);
    lv_label_set_text(s_orbit_elev_lbl, "最大天顶仰角: --°");

    s_orbit_azim_lbl = lv_label_create(card);
    lv_obj_set_style_text_font(s_orbit_azim_lbl, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(s_orbit_azim_lbl, lv_color_hex(COLOR_TEXT_MUTED), 0);
    lv_obj_set_pos(s_orbit_azim_lbl, 4, 56);
    lv_label_set_text(s_orbit_azim_lbl, "过境方位: --");

    s_orbit_mag_lbl = lv_label_create(card);
    lv_obj_set_style_text_font(s_orbit_mag_lbl, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(s_orbit_mag_lbl, lv_color_hex(COLOR_GOLD_SUN), 0);
    lv_obj_set_pos(s_orbit_mag_lbl, 4, 82);
    lv_label_set_text(s_orbit_mag_lbl, "亮度星等: -2 等");

    s_orbit_kp_lbl = lv_label_create(card);
    lv_obj_set_style_text_font(s_orbit_kp_lbl, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(s_orbit_kp_lbl, lv_color_hex(COLOR_CYAN_STAR), 0);
    lv_obj_set_pos(s_orbit_kp_lbl, 4, 108);
    lv_label_set_text(s_orbit_kp_lbl, "地磁指数: Kp 2");

    lv_obj_t *hint = lv_label_create(s_orbit_scr);
    lv_obj_set_style_text_font(hint, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(COLOR_TEXT_MUTED), 0);
    lv_obj_set_pos(hint, 12, 288);
    lv_label_set_text(hint, "UP/DOWN:大区 | OK:空间站 | 长按:退出");

    s_orbit_timer = lv_timer_create(orbit_timer_cb, 1000, NULL);
    orbit_timer_cb(NULL);
    lv_screen_load(s_orbit_scr);
}

void demo_orbit_tracker_exit(void) {
    if (s_orbit_timer) {
        lv_timer_delete(s_orbit_timer);
        s_orbit_timer = NULL;
    }
    if (s_orbit_scr) {
        lv_obj_delete(s_orbit_scr);
        s_orbit_scr = NULL;
    }
}

void demo_orbit_tracker_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    if (ev == BSP_BTN_CLICK) {
        if (btn == BSP_BTN_OK) {
            cosmic_sat_t sat = cosmic_orbit_next_sat();
            if (s_orbit_sat_lbl) {
                lv_label_set_text(s_orbit_sat_lbl, (sat == SAT_TIANGONG_CSS) ? "[目标] 中国天宫空间站" : "[目标] 国际空间站");
            }
            orbit_timer_cb(NULL);
        } else if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
            cosmic_orbit_next_region();
            cosmic_orbit_pass_t pass;
            cosmic_orbit_get_pass(&pass);
            if (s_orbit_region_lbl) {
                lv_label_set_text(s_orbit_region_lbl, pass.region_name);
            }
            orbit_timer_cb(NULL);
        }
    }
}

// ----------------------------------------------------------------------------
// 4. 荒野多模射电求救信标视图 (Wilderness SOS Beacon View)
// ----------------------------------------------------------------------------
static lv_obj_t   *s_sos_scr = NULL;
static lv_obj_t   *s_sos_status_lbl = NULL;
static lv_obj_t   *s_sos_time_lbl = NULL;
static lv_obj_t   *s_sos_soc_lbl = NULL;
static lv_obj_t   *s_sos_cycles_lbl = NULL;
static lv_obj_t   *s_sos_action_btn_lbl = NULL;
static lv_timer_t *s_sos_timer = NULL;

static void sos_timer_cb(lv_timer_t *t) {
    (void)t;
    int soc = bsp_battery_soc();
    if (soc < 0) soc = 85;

    bool light_on = false;
    uint16_t audio_hz = 0;
    cosmic_sos_tick_100ms(soc, &light_on, &audio_hz);

    cosmic_sos_status_t status;
    cosmic_sos_get_status(&status);

    char buf[64];
    uint32_t cd = status.elapsed_sec;
    uint32_t hours = cd / 3600;
    uint32_t mins = (cd % 3600) / 60;
    uint32_t secs = cd % 60;

    snprintf(buf, sizeof(buf), "呼救时长: %02lu:%02lu:%02lu", (unsigned long)hours, (unsigned long)mins, (unsigned long)secs);
    lv_label_set_text(s_sos_time_lbl, buf);

    snprintf(buf, sizeof(buf), "电量: %d%% (预计续航 %d 小时)", soc, (soc * 24) / 100);
    lv_label_set_text(s_sos_soc_lbl, buf);

    snprintf(buf, sizeof(buf), "莫尔斯轮次: %lu 轮", (unsigned long)status.distress_cycle_count);
    lv_label_set_text(s_sos_cycles_lbl, buf);

#ifdef ESP_PLATFORM
    if (status.is_active) {
        // 莫尔斯时序白光爆闪
        bsp_display_backlight(light_on ? 100 : 0);
    }
#endif
}

void demo_sos_beacon_enter(void) {
    cosmic_sos_init();

    s_sos_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_sos_scr, lv_color_hex(COLOR_BG_SPACE), 0);

    lv_obj_t *title = lv_label_create(s_sos_scr);
    lv_obj_set_style_text_font(title, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(COLOR_RED_FLARE), 0);
    lv_obj_set_pos(title, 12, 12);
    lv_label_set_text(title, "荒野射电求救信标");

    s_sos_status_lbl = lv_label_create(s_sos_scr);
    lv_obj_set_style_text_font(s_sos_status_lbl, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(s_sos_status_lbl, lv_color_hex(0x00FF88), 0);
    lv_obj_set_pos(s_sos_status_lbl, 12, 38);
    lv_label_set_text(s_sos_status_lbl, "[状态] 信标就绪 - 按OK启动");

    lv_obj_t *card = lv_obj_create(s_sos_scr);
    lv_obj_set_size(card, 216, 160);
    lv_obj_set_pos(card, 12, 68);
    lv_obj_set_style_bg_color(card, lv_color_hex(COLOR_CARD_BG), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *mod1 = lv_label_create(card);
    lv_obj_set_style_text_font(mod1, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(mod1, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(mod1, 4, 4);
    lv_label_set_text(mod1, "1. 蓝牙SOS广播 (全功率)");

    lv_obj_t *mod2 = lv_label_create(card);
    lv_obj_set_style_text_font(mod2, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(mod2, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(mod2, 4, 28);
    lv_label_set_text(mod2, "2. 应急救援无线热点");

    lv_obj_t *mod3 = lv_label_create(card);
    lv_obj_set_style_text_font(mod3, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(mod3, lv_color_hex(COLOR_RED_FLARE), 0);
    lv_obj_set_pos(mod3, 4, 52);
    lv_label_set_text(mod3, "3. 莫尔斯声光求救");

    s_sos_time_lbl = lv_label_create(card);
    lv_obj_set_style_text_font(s_sos_time_lbl, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(s_sos_time_lbl, lv_color_hex(COLOR_CYAN_STAR), 0);
    lv_obj_set_pos(s_sos_time_lbl, 4, 82);
    lv_label_set_text(s_sos_time_lbl, "呼救时长: 00:00:00");

    s_sos_soc_lbl = lv_label_create(card);
    lv_obj_set_style_text_font(s_sos_soc_lbl, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(s_sos_soc_lbl, lv_color_hex(COLOR_TEXT_MUTED), 0);
    lv_obj_set_pos(s_sos_soc_lbl, 4, 106);
    lv_label_set_text(s_sos_soc_lbl, "电量: 100%");

    s_sos_cycles_lbl = lv_label_create(card);
    lv_obj_set_style_text_font(s_sos_cycles_lbl, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(s_sos_cycles_lbl, lv_color_hex(COLOR_GOLD_SUN), 0);
    lv_obj_set_pos(s_sos_cycles_lbl, 4, 130);
    lv_label_set_text(s_sos_cycles_lbl, "莫尔斯轮次: 0 轮");

    s_sos_action_btn_lbl = lv_label_create(s_sos_scr);
    lv_obj_set_style_text_font(s_sos_action_btn_lbl, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(s_sos_action_btn_lbl, lv_color_hex(0x00FF88), 0);
    lv_obj_set_pos(s_sos_action_btn_lbl, 12, 240);
    lv_label_set_text(s_sos_action_btn_lbl, "短按确定键: [启动求救发射]");

    lv_obj_t *hint = lv_label_create(s_sos_scr);
    lv_obj_set_style_text_font(hint, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(COLOR_TEXT_MUTED), 0);
    lv_obj_set_pos(hint, 12, 288);
    lv_label_set_text(hint, "短按确定:控制发射 | 长按:退出");

    s_sos_timer = lv_timer_create(sos_timer_cb, 100, NULL);
    lv_screen_load(s_sos_scr);
}

void demo_sos_beacon_exit(void) {
    if (s_sos_timer) {
        lv_timer_delete(s_sos_timer);
        s_sos_timer = NULL;
    }
    cosmic_sos_stop();
    cosmic_power_set_keep_alive(false);
#ifdef ESP_PLATFORM
    bsp_display_backlight(100);
#endif
    if (s_sos_scr) {
        lv_obj_delete(s_sos_scr);
        s_sos_scr = NULL;
    }
}

void demo_sos_beacon_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK) {
        if (cosmic_sos_is_active()) {
            cosmic_sos_stop();
            cosmic_power_set_keep_alive(false);
#ifdef ESP_PLATFORM
            bsp_display_backlight(100);
#endif
            if (s_sos_status_lbl) lv_label_set_text(s_sos_status_lbl, "[状态] 信标已暂停");
            if (s_sos_action_btn_lbl) lv_label_set_text(s_sos_action_btn_lbl, "短按确定键: [重新启动发射]");
        } else {
            cosmic_sos_start();
            cosmic_power_set_keep_alive(true); // 求救期间强制常亮保持
            if (s_sos_status_lbl) lv_label_set_text(s_sos_status_lbl, "[状态] 正在全频段呼救中!");
            if (s_sos_action_btn_lbl) lv_label_set_text(s_sos_action_btn_lbl, "短按确定键: [暂停发射]");
        }
    }
}
