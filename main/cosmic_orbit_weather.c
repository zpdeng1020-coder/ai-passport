// main/cosmic_orbit_weather.c —— 空间站过境天体推算与太空天气实现
#include "cosmic_orbit_weather.h"
#include <string.h>

static cosmic_orbit_pass_t    s_current_pass;
static cosmic_space_weather_t s_weather;
static uint32_t               s_uptime_sec = 0;

static const char *SAT_NAMES[] = {
    "中国天宫空间站", // SAT_TIANGONG_CSS
    "国际空间站",     // SAT_ISS
};

static const char *REGION_NAMES[] = {
    "华北地区", // REGION_NORTH_CHINA
    "华东地区", // REGION_EAST_CHINA
    "华南地区", // REGION_SOUTH_CHINA
    "西南地区", // REGION_SOUTHWEST
    "西北地区", // REGION_NORTHWEST
};

static const char *AZIMUTH_DESCS[] = {
    "西北向东南",
    "西南向东北",
    "西向东偏北",
    "西向东偏南",
};

static const char *GEOMAG_STATES[] = {
    "地磁宁静 (适宜射电观测)",
    "弱地磁扰动",
    "地磁暴预警 (强极光/电离层扰动)",
};

// 预设各大区与空间站的轨道基准偏移参数 (秒与仰角)
// 天宫轨道倾角 41.5°, 覆盖华北、华中、华南极佳
// 国际空间站轨道倾角 51.6°
typedef struct {
    uint32_t base_countdown_sec;
    uint16_t duration_sec;
    uint8_t  max_elevation_deg;
    uint8_t  azimuth_idx;
    int8_t   apparent_mag;
} pass_param_t;

static const pass_param_t TIANGONG_PARAMS[REGION_COUNT] = {
    {  1840, 360, 78, 0, -2 }, // 华北: 高仰角近天顶, 视星等 -2 等极亮
    {  3210, 330, 65, 0, -1 }, // 华东: 65°
    {  5400, 310, 52, 2, -1 }, // 华南: 52°
    {  7200, 340, 70, 1, -2 }, // 西南: 70°
    {  8900, 380, 82, 0, -2 }, // 西北: 82°
};

static const pass_param_t ISS_PARAMS[REGION_COUNT] = {
    {  2700, 370, 85, 0, -3 }, // 华北
    {  4500, 350, 72, 0, -2 }, // 华东
    {  6300, 300, 48, 2, -1 }, // 华南
    {  8100, 340, 62, 1, -2 }, // 西南
    {  1200, 390, 88, 0, -3 }, // 西北: 近天顶
};

void cosmic_orbit_compute_pass(cosmic_sat_t sat, cosmic_region_t region, uint32_t epoch_sec, cosmic_orbit_pass_t *out) {
    if (!out) return;
    if (sat >= SAT_COUNT) sat = SAT_TIANGONG_CSS;
    if (region >= REGION_COUNT) region = REGION_NORTH_CHINA;

    const pass_param_t *params = (sat == SAT_TIANGONG_CSS) ? 
        &TIANGONG_PARAMS[region] : &ISS_PARAMS[region];

    out->sat_type = sat;
    out->sat_name = SAT_NAMES[sat];
    out->region = region;
    out->region_name = REGION_NAMES[region];
    out->duration_sec = params->duration_sec;
    out->max_elevation_deg = params->max_elevation_deg;
    out->azimuth_desc = AZIMUTH_DESCS[params->azimuth_idx];
    out->apparent_mag = params->apparent_mag;

    // 根据系统运行时间推进倒计时 (以轨道周期 5520 秒约 92 分钟循环)
    uint32_t period = 5520;
    uint32_t offset = (params->base_countdown_sec > (epoch_sec % period)) ?
                      (params->base_countdown_sec - (epoch_sec % period)) :
                      (period - ((epoch_sec % period) - params->base_countdown_sec));

    out->countdown_sec = offset;
    out->is_transiting = (offset < params->duration_sec);
}

void cosmic_orbit_init(void) {
    s_uptime_sec = 0;
    cosmic_orbit_compute_pass(SAT_TIANGONG_CSS, REGION_NORTH_CHINA, 0, &s_current_pass);

    // 初始化太阳与地磁活动指数
    s_weather.kp_index = 2; // Kp 2 为平静期
    s_weather.solar_flare_class = 'C';
    s_weather.solar_flux = 120.5f;
    s_weather.aurora_chance_pct = 5;
    s_weather.geomag_status = GEOMAG_STATES[0];
}

cosmic_sat_t cosmic_orbit_next_sat(void) {
    cosmic_sat_t next = (s_current_pass.sat_type == SAT_TIANGONG_CSS) ? SAT_ISS : SAT_TIANGONG_CSS;
    cosmic_orbit_compute_pass(next, s_current_pass.region, s_uptime_sec, &s_current_pass);
    return next;
}

cosmic_region_t cosmic_orbit_next_region(void) {
    cosmic_region_t next = (cosmic_region_t)((s_current_pass.region + 1) % REGION_COUNT);
    cosmic_orbit_compute_pass(s_current_pass.sat_type, next, s_uptime_sec, &s_current_pass);
    return next;
}

void cosmic_orbit_tick_1s(void) {
    s_uptime_sec++;
    if (s_current_pass.countdown_sec > 0) {
        s_current_pass.countdown_sec--;
    } else {
        // 过境结束，重新计算下一次过境
        cosmic_orbit_compute_pass(s_current_pass.sat_type, s_current_pass.region, s_uptime_sec, &s_current_pass);
    }
    s_current_pass.is_transiting = (s_current_pass.countdown_sec < s_current_pass.duration_sec);

    // 地磁小幅慢速周期性变化模拟 (周期约 1 小时)
    uint32_t cycle = (s_uptime_sec / 60) % 20;
    if (cycle < 12) {
        s_weather.kp_index = 2;
        s_weather.geomag_status = GEOMAG_STATES[0];
        s_weather.aurora_chance_pct = 5;
    } else if (cycle < 17) {
        s_weather.kp_index = 4;
        s_weather.geomag_status = GEOMAG_STATES[1];
        s_weather.aurora_chance_pct = 25;
    } else {
        s_weather.kp_index = 6;
        s_weather.geomag_status = GEOMAG_STATES[2];
        s_weather.aurora_chance_pct = 68;
    }
}

void cosmic_orbit_get_pass(cosmic_orbit_pass_t *pass) {
    if (!pass) return;
    *pass = s_current_pass;
}

void cosmic_orbit_get_weather(cosmic_space_weather_t *weather) {
    if (!weather) return;
    *weather = s_weather;
}
