// main/cosmic_quiet_scout.c —— 户外暗夜与“电磁荒野”静区罗盘实现
#include "cosmic_quiet_scout.h"
#include <string.h>
#include <math.h>

static cosmic_quiet_report_t s_report;

// 预置静态级别字样与建议 (严格使用 lv_font_cn_16 常用汉字)
static const char *LEVEL_NAMES[] = {
    "赛博风暴", // QUIET_LEVEL_STORM
    "近郊村落", // QUIET_LEVEL_SUBURBAN
    "自然原野", // QUIET_LEVEL_WILDERNESS
    "纯净深空", // QUIET_LEVEL_SANCTUARY
};

static const char *LEVEL_ADVICE[] = {
    "严重电子干扰，无法进行弱光观星与射电感知",
    "存在人造背景辐射，建议向更深处旷野扎营",
    "微弱自然电磁场，适合野外露营与静心扎营",
    "电磁环境极优，极适合肉眼观星与深空摄影",
};

uint8_t cosmic_quiet_calculate_rqi(uint32_t pps, int8_t avg_rssi) {
    // 基础评分从 100 分开始
    float score = 100.0f;

    // 1. 包速率扣分: 使用对数能级衰减 (1pps 扣约 4 分, 10pps 扣 15 分, 100pps 扣 30 分, 1000pps 扣 45 分)
    if (pps > 0) {
        float log_pps = log10f((float)(pps + 1));
        score -= log_pps * 15.0f;
    }

    // 2. 信号强度扣分: 以 -95dBm 为纯净参考底噪
    if (avg_rssi > -95) {
        float rssi_delta = (float)(avg_rssi - (-95));
        // 每升高 1dBm 扣约 0.7 分，强信号 (-40dBm) 额外扣约 38 分
        score -= rssi_delta * 0.7f;
    }

    // 钳位在 0 ~ 100 分
    if (score < 0.0f) score = 0.0f;
    if (score > 100.0f) score = 100.0f;

    return (uint8_t)(score + 0.5f);
}

void cosmic_quiet_init(void) {
    memset(&s_report, 0, sizeof(s_report));
    s_report.rqi_score = 95;
    s_report.level = QUIET_LEVEL_SANCTUARY;
    s_report.view_mode = ASTRO_VIEW_NORMAL;
    s_report.noise_floor_dbm = -92;
    s_report.packet_density = 0;
    s_report.level_name = LEVEL_NAMES[QUIET_LEVEL_SANCTUARY];
    s_report.advice = LEVEL_ADVICE[QUIET_LEVEL_SANCTUARY];
}

void cosmic_quiet_update(uint32_t pps, int8_t avg_rssi) {
    s_report.packet_density = pps;
    s_report.noise_floor_dbm = avg_rssi;
    s_report.rqi_score = cosmic_quiet_calculate_rqi(pps, avg_rssi);

    if (s_report.rqi_score >= 90) {
        s_report.level = QUIET_LEVEL_SANCTUARY;
    } else if (s_report.rqi_score >= 60) {
        s_report.level = QUIET_LEVEL_WILDERNESS;
    } else if (s_report.rqi_score >= 30) {
        s_report.level = QUIET_LEVEL_SUBURBAN;
    } else {
        s_report.level = QUIET_LEVEL_STORM;
    }

    s_report.level_name = LEVEL_NAMES[s_report.level];
    s_report.advice = LEVEL_ADVICE[s_report.level];
}

void cosmic_quiet_get_report(cosmic_quiet_report_t *report) {
    if (!report) return;
    *report = s_report;
}

cosmic_astro_view_mode_t cosmic_quiet_toggle_view_mode(void) {
    if (s_report.view_mode == ASTRO_VIEW_NORMAL) {
        s_report.view_mode = ASTRO_VIEW_RED_NIGHT;
    } else {
        s_report.view_mode = ASTRO_VIEW_NORMAL;
    }
    return s_report.view_mode;
}

cosmic_astro_view_mode_t cosmic_quiet_get_view_mode(void) {
    return s_report.view_mode;
}
