// main/tracker_ui.c —— AI Passport 随身安全哨兵 4 大功能模块页面与安全文本渲染
#include "tracker_ui.h"
#include "tracker_engine.h"
#include "tracker_alarm.h"
#include "tracker_wifi_spy.h"
#include "tracker_ble_scanner.h"
#include "tracker_power.h"
#include "ui_pixel.h"
#include "bsp_battery.h"
#include "lvgl.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#if defined(ESP_PLATFORM)
#include "esp_timer.h"
#include "esp_log.h"
#else
#include <time.h>
static uint32_t get_sim_time(void) { return (uint32_t)time(NULL); }
#endif

// 引入官方验证的 16px GB2312 常用 3755 汉字字库
LV_FONT_DECLARE(lv_font_cn_16);

// UTF-8 安全过滤: 遇到字库缺失字符以 '?' 代替，杜绝方块占位符 (Tofu)
static size_t utf8_seq_len(uint8_t lead)
{
    if (lead < 0x80) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 0;
}

static uint32_t utf8_decode(const uint8_t *bytes, size_t count)
{
    if (count == 1) return bytes[0];
    uint32_t val = bytes[0] & ((1u << (7u - count)) - 1u);
    for (size_t i = 1; i < count; ++i) {
        val = (val << 6) | (bytes[i] & 0x3Fu);
    }
    return val;
}

static void safe_text_filter(const char *in, const lv_font_t *font, char *out, size_t cap)
{
    size_t i = 0, o = 0;
    if (cap == 0) return;
    while (in && in[i] != '\0' && o + 1 < cap) {
        const uint8_t *b = (const uint8_t *)in + i;
        size_t count = utf8_seq_len(b[0]);
        bool valid = (count > 0);
        for (size_t k = 1; valid && k < count; ++k) {
            valid = ((b[k] & 0xC0) == 0x80);
        }
        if (!valid) {
            out[o++] = '?';
            i++;
            continue;
        }
        uint32_t cp = utf8_decode(b, count);
        lv_font_glyph_dsc_t gdsc;
        bool ok = (cp == '\n' || cp == '\r' || lv_font_get_glyph_dsc(font, &gdsc, cp, 0));
        if (!ok) {
            out[o++] = '?';
        } else if (o + count < cap) {
            memcpy(out + o, b, count);
            o += count;
        } else {
            break;
        }
        i += count;
    }
    out[o] = '\0';
}

static char s_filter_buf[2048];

static lv_obj_t *create_safe_label(lv_obj_t *parent, const char *text, int width, uint32_t color)
{
    safe_text_filter(text ? text : "", &lv_font_cn_16, s_filter_buf, sizeof(s_filter_buf));
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, s_filter_buf);
    if (width > 0) {
        lv_obj_set_width(lbl, width);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    }
    lv_obj_set_style_text_font(lbl, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(color), 0);
    lv_obj_set_style_text_line_space(lbl, 4, 0);
    return lbl;
}

static void set_safe_label_text(lv_obj_t *lbl, const char *text)
{
    if (!lbl) return;
    safe_text_filter(text ? text : "", &lv_font_cn_16, s_filter_buf, sizeof(s_filter_buf));
    lv_label_set_text(lbl, s_filter_buf);
}

static uint32_t get_current_sec(void)
{
#if defined(ESP_PLATFORM)
    return (uint32_t)(esp_timer_get_time() / 1000000ULL);
#else
    return get_sim_time();
#endif
}

static void add_header_battery(lv_obj_t *scr)
{
    lv_obj_t *soc_lbl = lv_label_create(scr);
    lv_obj_set_style_text_font(soc_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(soc_lbl, lv_color_hex(UI_INK), 0);
    lv_obj_set_pos(soc_lbl, 180, 16);
    int soc = bsp_battery_soc();
    if (soc < 0) lv_label_set_text(soc_lbl, "--%");
    else         lv_label_set_text_fmt(soc_lbl, "%d%%", soc);
}

// ============================================================================
// 1. 【防追踪雷达】模块 (demo_radar)
// ============================================================================
static lv_obj_t *s_radar_scr = NULL;
static lv_timer_t *s_radar_timer = NULL;
static int s_radar_cursor = 0;
static uint32_t s_radar_phase = 0;

static lv_obj_t *s_radar_threat_panel = NULL;
static lv_obj_t *s_radar_threat_label = NULL;
static lv_obj_t *s_radar_sub_label = NULL;
static lv_obj_t *s_radar_card_label = NULL;
static lv_obj_t *s_radar_footer_label = NULL;

static void update_radar_display(const tracker_summary_t *sum)
{
    if (!s_radar_threat_panel || !s_radar_card_label) return;

    // 1. 威胁等级面板颜色与文本
    uint32_t color = UI_GRASS;
    const char *status_str = "状态: 正常安全";
    uint32_t text_color = UI_INK;

    if (sum->max_threat == THREAT_LEVEL_ALERT) {
        color = (s_radar_phase % 2 == 0) ? UI_RED : UI_INK;
        text_color = UI_PAPER;
        status_str = "警告: 持续伴随追踪!!";
    } else if (sum->max_threat == THREAT_LEVEL_NOTICE) {
        color = UI_YELLOW;
        text_color = UI_INK;
        status_str = "提示: 发现伴随信标";
    }
    lv_obj_set_style_bg_color(s_radar_threat_panel, lv_color_hex(color), 0);
    lv_obj_set_style_text_color(s_radar_threat_label, lv_color_hex(text_color), 0);
    set_safe_label_text(s_radar_threat_label, status_str);

    // 2. 扫描动效与实时收包计数
    static const char *const SPINS[] = { "[/]", "[-]", "[\\]", "[|]" };
    uint32_t packets = tracker_ble_scanner_get_packet_count();
    char sub_buf[128];
    snprintf(sub_buf, sizeof(sub_buf), "%s 侦听中... [抓包: %u | 目标: %u]",
             SPINS[s_radar_phase % 4], (unsigned)packets, (unsigned)sum->beacon_count);
    set_safe_label_text(s_radar_sub_label, sub_buf);

    // 3. 信标卡片详情
    if (sum->beacon_count == 0) {
        set_safe_label_text(s_radar_card_label,
            "周围暂未发现追踪器\n\n"
            "正在持续静默排查:\n"
            "- Apple AirTag (0x12)\n"
            "- Apple 随身设备 (0x10)\n"
            "- 三星 SmartTag\n"
            "- Tile 寻物瓷贴\n\n"
            "长按 OK 键返回主菜单");
        set_safe_label_text(s_radar_footer_label, "上下:静默侦听 | 长按OK:菜单");
    } else {
        if (s_radar_cursor >= (int)sum->beacon_count) {
            s_radar_cursor = (int)sum->beacon_count - 1;
        }
        if (s_radar_cursor < 0) s_radar_cursor = 0;

        const tracker_beacon_t *b = tracker_get_beacon((size_t)s_radar_cursor);
        if (b) {
            uint32_t dur = (sum->current_time_sec >= b->first_seen_sec)
                         ? (sum->current_time_sec - b->first_seen_sec)
                         : 0;
            int pct = tracker_rssi_to_proximity_pct(b->smoothed_rssi);
            char card_buf[384];
            snprintf(card_buf, sizeof(card_buf),
                     "[%d/%d] %s\n"
                     "MAC: %02X:%02X:%02X:%02X:%02X:%02X\n"
                     "瞬时信号: %d dBm (邻近度 %d%%)\n"
                     "伴随时长: %u分%u秒\n"
                     "评估: %s%s",
                     s_radar_cursor + 1, (int)sum->beacon_count,
                     tracker_type_to_str(b->type),
                     b->mac[0], b->mac[1], b->mac[2], b->mac[3], b->mac[4], b->mac[5],
                     b->smoothed_rssi, pct,
                     (unsigned)(dur / 60), (unsigned)(dur % 60),
                     threat_level_to_str(b->threat),
                     b->is_whitelisted ? " (信任白名单)" : "");
            set_safe_label_text(s_radar_card_label, card_buf);
        }
        set_safe_label_text(s_radar_footer_label, "上下:切换 | OK:加白 | 长按OK:菜单");
    }
}

static void on_radar_timer(lv_timer_t *t)
{
    (void)t;
    s_radar_phase++;
    uint32_t now = get_current_sec();
    tracker_engine_tick(now);

    tracker_summary_t sum;
    tracker_get_summary(&sum);

    if (sum.max_threat == THREAT_LEVEL_ALERT) {
        tracker_power_wake();
        tracker_power_set_inhibit(true);
        if (s_radar_phase % 4 == 0) {
            tracker_alarm_trigger_alert();
        }
    } else {
        tracker_power_set_inhibit(false);
    }

    update_radar_display(&sum);
}

void demo_radar_enter(void)
{
    s_radar_cursor = 0;
    s_radar_phase = 0;
    s_radar_scr = ui_pixel_screen_create("防追踪雷达");
    add_header_battery(s_radar_scr);

    // 威胁状态面板
    s_radar_threat_panel = ui_pixel_panel_create(s_radar_scr, 10, 46, 220, 32, UI_GRASS);
    s_radar_threat_label = create_safe_label(s_radar_threat_panel, "状态: 正常安全", 204, UI_INK);
    lv_obj_set_style_text_align(s_radar_threat_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_radar_threat_label);

    // 动效与抓包统计
    s_radar_sub_label = create_safe_label(s_radar_scr, "[/] 侦听中... [抓包: 0 | 目标: 0]", 220, UI_INK);
    lv_obj_set_style_text_align(s_radar_sub_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_radar_sub_label, 10, 82);

    // 信标卡片内容面板
    lv_obj_t *card = ui_pixel_panel_create(s_radar_scr, 10, 104, 220, 174, UI_PAPER);
    s_radar_card_label = create_safe_label(card, "", 204, UI_INK);
    lv_obj_align(s_radar_card_label, LV_ALIGN_TOP_LEFT, 2, 2);

    // 底部提示
    s_radar_footer_label = create_safe_label(s_radar_scr, "上下:切换 | OK:加白 | 长按OK:菜单", 232, UI_INK);
    lv_obj_set_style_text_align(s_radar_footer_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_radar_footer_label, 4, 292);

    tracker_summary_t sum;
    tracker_get_summary(&sum);
    update_radar_display(&sum);

    lv_screen_load(s_radar_scr);
    s_radar_timer = lv_timer_create(on_radar_timer, 250, NULL);
}

void demo_radar_exit(void)
{
    tracker_power_set_inhibit(false);
    if (s_radar_timer) {
        lv_timer_delete(s_radar_timer);
        s_radar_timer = NULL;
    }
    if (s_radar_scr) {
        lv_obj_delete(s_radar_scr);
        s_radar_scr = NULL;
    }
}

void demo_radar_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    tracker_summary_t sum;
    tracker_get_summary(&sum);

    if (ev == BSP_BTN_CLICK) {
        if (btn == BSP_BTN_UP) {
            if (sum.beacon_count > 0) {
                s_radar_cursor = (s_radar_cursor + (int)sum.beacon_count - 1) % (int)sum.beacon_count;
                tracker_alarm_beep(2000, 20);
                update_radar_display(&sum);
            }
        } else if (btn == BSP_BTN_DOWN) {
            if (sum.beacon_count > 0) {
                s_radar_cursor = (s_radar_cursor + 1) % (int)sum.beacon_count;
                tracker_alarm_beep(2000, 20);
                update_radar_display(&sum);
            }
        } else if (btn == BSP_BTN_OK) {
            if (sum.beacon_count > 0 && s_radar_cursor < (int)sum.beacon_count) {
                const tracker_beacon_t *b = tracker_get_beacon((size_t)s_radar_cursor);
                if (b) {
                    tracker_whitelist_toggle(b->mac);
                    tracker_alarm_beep(1600, 80);
                    tracker_get_summary(&sum);
                    update_radar_display(&sum);
                }
            }
        }
    }
}

// ============================================================================
// 2. 【冷热寻物测距】模块 (demo_hotcold)
// ============================================================================
static lv_obj_t *s_hotcold_scr = NULL;
static lv_timer_t *s_hotcold_timer = NULL;
static int s_hotcold_target_idx = 0;

static lv_obj_t *s_hotcold_val_label = NULL;
static lv_obj_t *s_hotcold_bar = NULL;
static lv_obj_t *s_hotcold_card_label = NULL;

static void update_hotcold_display(void)
{
    tracker_summary_t sum;
    tracker_get_summary(&sum);

    if (sum.beacon_count == 0) {
        set_safe_label_text(s_hotcold_val_label, "0%  暂无信标目标");
        set_safe_label_text(s_hotcold_card_label,
            "周围暂未发现追踪器信标\n\n"
            "说明:\n"
            "本功能根据信标信号强度 (RSSI)\n"
            "发出类似金属探测器的滴答声,\n"
            "帮助排查藏匿在背包或衣物中的 AirTag.\n\n"
            "长按 OK 键返回主菜单");
        lv_bar_set_value(s_hotcold_bar, 0, LV_ANIM_OFF);
        return;
    }

    if (s_hotcold_target_idx >= (int)sum.beacon_count) {
        s_hotcold_target_idx = (int)sum.beacon_count - 1;
    }
    if (s_hotcold_target_idx < 0) s_hotcold_target_idx = 0;

    const tracker_beacon_t *b = tracker_get_beacon((size_t)s_hotcold_target_idx);
    if (!b) return;

    int pct = tracker_rssi_to_proximity_pct(b->rssi);
    lv_bar_set_value(s_hotcold_bar, pct, LV_ANIM_ON);

    const char *heat = "远离 (冷 COLD)";
    uint32_t bar_color = UI_SKY;
    if (pct > 75) {
        heat = "极近!! (热 HOT!)";
        bar_color = UI_RED;
    } else if (pct > 50) {
        heat = "较近 (温 WARM)";
        bar_color = UI_ORANGE;
    } else if (pct > 25) {
        heat = "较远 (微温 TEPID)";
        bar_color = UI_YELLOW;
    }
    lv_obj_set_style_bg_color(s_hotcold_bar, lv_color_hex(bar_color), LV_PART_INDICATOR);

    char val_buf[64];
    snprintf(val_buf, sizeof(val_buf), "%d%%  %s", pct, heat);
    set_safe_label_text(s_hotcold_val_label, val_buf);

    char desc_buf[320];
    snprintf(desc_buf, sizeof(desc_buf),
             "目标 [%d/%d]: %s\n"
             "瞬时信号: %d dBm\n"
             "邻近度估算: %d%%\n\n"
             "请持设备在背包或衣物周围移动\n"
             "靠近物理目标时蜂鸣更急促\n\n"
             "长按 OK 键返回主菜单",
             s_hotcold_target_idx + 1, (int)sum.beacon_count,
             tracker_type_to_str(b->type), b->rssi, pct);
    set_safe_label_text(s_hotcold_card_label, desc_buf);

    // 声音脉冲寻物反馈
    tracker_alarm_tick_hotcold(pct);
}

static void on_hotcold_timer(lv_timer_t *t)
{
    (void)t;
    update_hotcold_display();
}

void demo_hotcold_enter(void)
{
    s_hotcold_target_idx = 0;
    s_hotcold_scr = ui_pixel_screen_create("冷热寻物");
    add_header_battery(s_hotcold_scr);

    // 邻近百分比与冷热评估状态
    s_hotcold_val_label = create_safe_label(s_hotcold_scr, "0%  远离 (冷 COLD)", 220, UI_INK);
    lv_obj_set_style_text_align(s_hotcold_val_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_hotcold_val_label, 10, 48);

    // 视觉进度条
    s_hotcold_bar = lv_bar_create(s_hotcold_scr);
    lv_obj_set_size(s_hotcold_bar, 200, 22);
    lv_obj_set_pos(s_hotcold_bar, 20, 74);
    lv_bar_set_range(s_hotcold_bar, 0, 100);
    lv_obj_set_style_border_color(s_hotcold_bar, lv_color_hex(UI_INK), 0);
    lv_obj_set_style_border_width(s_hotcold_bar, 2, 0);
    lv_obj_set_style_radius(s_hotcold_bar, 0, 0);
    lv_obj_set_style_radius(s_hotcold_bar, 0, LV_PART_INDICATOR);

    // 测距指导卡片
    lv_obj_t *panel = ui_pixel_panel_create(s_hotcold_scr, 10, 106, 220, 172, UI_PAPER);
    s_hotcold_card_label = create_safe_label(panel, "", 204, UI_INK);
    lv_obj_align(s_hotcold_card_label, LV_ALIGN_TOP_LEFT, 2, 2);

    // 底部提示
    lv_obj_t *footer = create_safe_label(s_hotcold_scr, "上下:切目标 | 长按OK:菜单", 232, UI_INK);
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(footer, 4, 292);

    update_hotcold_display();
    lv_screen_load(s_hotcold_scr);
    tracker_power_set_inhibit(true); // 冷热寻物处于高频手持交互态，全程抑制熄屏
    s_hotcold_timer = lv_timer_create(on_hotcold_timer, 200, NULL);
}

void demo_hotcold_exit(void)
{
    tracker_power_set_inhibit(false);
    if (s_hotcold_timer) {
        lv_timer_delete(s_hotcold_timer);
        s_hotcold_timer = NULL;
    }
    if (s_hotcold_scr) {
        lv_obj_delete(s_hotcold_scr);
        s_hotcold_scr = NULL;
    }
}

void demo_hotcold_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    tracker_summary_t sum;
    tracker_get_summary(&sum);

    if (ev == BSP_BTN_CLICK && sum.beacon_count > 0) {
        if (btn == BSP_BTN_UP) {
            s_hotcold_target_idx = (s_hotcold_target_idx + (int)sum.beacon_count - 1) % (int)sum.beacon_count;
            tracker_alarm_beep(2000, 20);
            update_hotcold_display();
        } else if (btn == BSP_BTN_DOWN) {
            s_hotcold_target_idx = (s_hotcold_target_idx + 1) % (int)sum.beacon_count;
            tracker_alarm_beep(2000, 20);
            update_hotcold_display();
        }
    }
}

// ============================================================================
// 3. 【无线偷拍排查】模块 (demo_wifi_spy)
// ============================================================================
static lv_obj_t *s_wifi_scr = NULL;
static lv_timer_t *s_wifi_timer = NULL;
static uint32_t s_wifi_dots = 0;
static lv_obj_t *s_wifi_panel = NULL;
static lv_obj_t *s_wifi_card_label = NULL;

static void update_wifi_spy_display(void)
{
    if (!s_wifi_card_label) return;

    if (tracker_wifi_spy_is_busy()) {
        static const char *const DOTS[] = { ".  ", ".. ", "..." };
        char wait_buf[128];
        snprintf(wait_buf, sizeof(wait_buf),
                 "正在全信道扫描 2.4G 频段%s\n"
                 "排查微型摄像头与隐藏热点\n"
                 "请等待 3 秒...",
                 DOTS[s_wifi_dots % 3]);
        set_safe_label_text(s_wifi_card_label, wait_buf);
        return;
    }

    if (!tracker_wifi_spy_has_scanned()) {
        set_safe_label_text(s_wifi_card_label,
            "按 OK 键开始排查\n\n"
            "探测原理:\n"
            "- 排查关闭 SSID 广播的偷拍设备\n"
            "- 识别典型摄像头网络命名前缀\n"
            "- 匹配安防/监控芯片 MAC OUI\n"
            "- 检测近距离贴身异常强发射源\n\n"
            "上下键: 滑动查看内容\n"
            "OK 键: 重新排查\n"
            "长按 OK: 返回主菜单");
        return;
    }

    spy_candidate_t cands[SPY_MAX_RESULTS];
    size_t count = tracker_wifi_spy_get_candidates(cands, SPY_MAX_RESULTS);
    size_t total_aps = tracker_wifi_spy_get_total_scanned();

    if (count == 0) {
        char clean_buf[256];
        snprintf(clean_buf, sizeof(clean_buf),
                 "排查完成!\n\n"
                 "周围共扫描到 %u 个热点\n"
                 "未发现微型摄像头或隐藏热点.\n\n"
                 "周围无线环境正常.\n\n"
                 "[短按 OK 键重新排查]\n"
                 "[长按 OK 键返回主菜单]", (unsigned)total_aps);
        set_safe_label_text(s_wifi_card_label, clean_buf);
        return;
    }

    static char buf[2048];
    size_t off = 0;
    off += snprintf(buf + off, sizeof(buf) - off,
                    "排查完成! 发现 %u 个可疑目标:\n(周围共扫描 %u 个 Wi-Fi)\n\n",
                    (unsigned)count, (unsigned)total_aps);

    for (size_t i = 0; i < count; i++) {
        const char *name = cands[i].is_hidden ? "<隐藏 SSID 热点>" : cands[i].ssid;
        const char *vtag = (cands[i].vendor_tag[0] != '\0') ? cands[i].vendor_tag : "通用芯片";
        off += snprintf(buf + off, sizeof(buf) - off,
                        "[%d] %s\n"
                        "  原因: %s\n"
                        "  芯片: %s\n"
                        "  MAC: %02X:%02X:%02X:..\n"
                        "  信号: %d dBm | 信道: %d\n\n",
                        (int)(i + 1),
                        name,
                        tracker_spy_reason_to_str(cands[i].reason),
                        vtag,
                        cands[i].bssid[0], cands[i].bssid[1], cands[i].bssid[2],
                        cands[i].rssi,
                        cands[i].channel);
        if (off >= sizeof(buf) - 220) break;
    }
    off += snprintf(buf + off, sizeof(buf) - off,
                    "----------------\n"
                    "上下键: 滑动浏览全部目标\n"
                    "短按 OK: 重新排查扫描\n"
                    "长按 OK: 返回安全主菜单");
    set_safe_label_text(s_wifi_card_label, buf);
}

static void on_wifi_timer(lv_timer_t *t)
{
    (void)t;
    s_wifi_dots++;
    if (tracker_wifi_spy_is_busy()) {
        tracker_power_set_inhibit(true);
    } else {
        tracker_power_set_inhibit(false);
    }
    update_wifi_spy_display();
}

void demo_wifi_spy_enter(void)
{
    s_wifi_dots = 0;
    s_wifi_scr = ui_pixel_screen_create("偷拍排查");
    add_header_battery(s_wifi_scr);

    s_wifi_panel = ui_pixel_panel_create(s_wifi_scr, 10, 48, 220, 230, UI_PAPER);
    lv_obj_add_flag(s_wifi_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_wifi_panel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_wifi_panel, LV_SCROLLBAR_MODE_AUTO);

    s_wifi_card_label = create_safe_label(s_wifi_panel, "", 200, UI_INK);
    lv_obj_align(s_wifi_card_label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *footer = create_safe_label(s_wifi_scr, "上下:滑动查看 | OK:重扫 | 长按OK:菜单", 232, UI_INK);
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(footer, 4, 292);

    // 进入页面后立即自动开始执行排查扫描，扫描期间抑制熄屏
    tracker_power_set_inhibit(true);
    tracker_wifi_spy_start_scan();

    update_wifi_spy_display();
    lv_screen_load(s_wifi_scr);
    s_wifi_timer = lv_timer_create(on_wifi_timer, 250, NULL);
}

void demo_wifi_spy_exit(void)
{
    tracker_power_set_inhibit(false);
    if (s_wifi_timer) {
        lv_timer_delete(s_wifi_timer);
        s_wifi_timer = NULL;
    }
    if (s_wifi_scr) {
        lv_obj_delete(s_wifi_scr);
        s_wifi_scr = NULL;
        s_wifi_panel = NULL;
        s_wifi_card_label = NULL;
    }
}

void demo_wifi_spy_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev == BSP_BTN_CLICK) {
        if (btn == BSP_BTN_UP) {
            if (s_wifi_panel) {
                lv_obj_scroll_by_bounded(s_wifi_panel, 0, 45, LV_ANIM_ON);
            }
        } else if (btn == BSP_BTN_DOWN) {
            if (s_wifi_panel) {
                lv_obj_scroll_by_bounded(s_wifi_panel, 0, -45, LV_ANIM_ON);
            }
        } else if (btn == BSP_BTN_OK) {
            if (s_wifi_panel) {
                lv_obj_scroll_to_y(s_wifi_panel, 0, LV_ANIM_OFF);
            }
            tracker_power_set_inhibit(true);
            tracker_wifi_spy_start_scan();
            tracker_alarm_beep(2200, 40);
            update_wifi_spy_display();
        }
    }
}

// ============================================================================
// 4. 【信任白名单】模块 (demo_whitelist)
// ============================================================================
static lv_obj_t *s_whitelist_scr = NULL;
static lv_obj_t *s_whitelist_panel = NULL;
static lv_obj_t *s_whitelist_card_label = NULL;

static void update_whitelist_display(void)
{
    if (!s_whitelist_card_label) return;

    size_t count = tracker_whitelist_count();
    if (count == 0) {
        set_safe_label_text(s_whitelist_card_label,
            "暂无信任白名单设备\n\n"
            "使用说明:\n"
            "在「防追踪雷达」中选中自己的\n"
            "AirTag 或随身设备后短按 OK 键,\n"
            "即可加入白名单, 永不误报.\n\n"
            "长按 OK 键返回主菜单");
        return;
    }

    static char buf[2048];
    size_t off = 0;
    off += snprintf(buf + off, sizeof(buf) - off, "已信任白名单 (%u 个):\n\n", (unsigned)count);
    for (size_t i = 0; i < count; i++) {
        const tracker_whitelist_item_t *item = tracker_whitelist_get(i);
        if (item) {
            off += snprintf(buf + off, sizeof(buf) - off,
                            "[%d] %s\n"
                            "    MAC: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
                            (int)(i + 1),
                            (item->name[0] != '\0') ? item->name : "信任信标",
                            item->mac[0], item->mac[1], item->mac[2],
                            item->mac[3], item->mac[4], item->mac[5]);
        }
        if (off >= sizeof(buf) - 220) break;
    }
    off += snprintf(buf + off, sizeof(buf) - off,
                    "----------------\n"
                    "上下键: 滑动浏览全部设备\n"
                    "[短按 OK 键清空白名单]\n"
                    "[长按 OK 键返回主菜单]");
    set_safe_label_text(s_whitelist_card_label, buf);
}

void demo_whitelist_enter(void)
{
    s_whitelist_scr = ui_pixel_screen_create("白名单");
    add_header_battery(s_whitelist_scr);

    s_whitelist_panel = ui_pixel_panel_create(s_whitelist_scr, 10, 48, 220, 230, UI_PAPER);
    lv_obj_add_flag(s_whitelist_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_whitelist_panel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_whitelist_panel, LV_SCROLLBAR_MODE_AUTO);

    s_whitelist_card_label = create_safe_label(s_whitelist_panel, "", 200, UI_INK);
    lv_obj_align(s_whitelist_card_label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *footer = create_safe_label(s_whitelist_scr, "上下:滑动查看 | OK:清空 | 长按OK:菜单", 232, UI_INK);
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(footer, 4, 292);

    update_whitelist_display();
    lv_screen_load(s_whitelist_scr);
}

void demo_whitelist_exit(void)
{
    if (s_whitelist_scr) {
        lv_obj_delete(s_whitelist_scr);
        s_whitelist_scr = NULL;
        s_whitelist_panel = NULL;
        s_whitelist_card_label = NULL;
    }
}

void demo_whitelist_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev == BSP_BTN_CLICK) {
        if (btn == BSP_BTN_UP) {
            if (s_whitelist_panel) {
                lv_obj_scroll_by_bounded(s_whitelist_panel, 0, 45, LV_ANIM_ON);
            }
        } else if (btn == BSP_BTN_DOWN) {
            if (s_whitelist_panel) {
                lv_obj_scroll_by_bounded(s_whitelist_panel, 0, -45, LV_ANIM_ON);
            }
        } else if (btn == BSP_BTN_OK) {
            // 短按 OK 清空白名单
            while (tracker_whitelist_count() > 0) {
                const tracker_whitelist_item_t *it = tracker_whitelist_get(0);
                if (!it) break;
                tracker_whitelist_remove(it->mac);
            }
            tracker_alarm_beep(1400, 80);
            if (s_whitelist_panel) {
                lv_obj_scroll_to_y(s_whitelist_panel, 0, LV_ANIM_OFF);
            }
            update_whitelist_display();
        }
    }
}
