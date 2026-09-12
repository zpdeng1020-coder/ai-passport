// main/cosmic_types.h —— 星原探针 (Cosmic & Earth Radio Explorer) 核心数据类型与常量
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 1. 宇宙射电声呐与能量特征 (Cosmic RF Sonar)
// ============================================================================
#define COSMIC_RF_CHANNELS          14      // 2.4GHz 1~14 信道
#define COSMIC_WATERFALL_COLS       240     // 水平瀑布流宽度 (对应屏宽 240)
#define COSMIC_WATERFALL_ROWS       120     // 垂直历史缓存行数
#define COSMIC_MAX_FLARE_EVENTS     10      // 记录最近突发强射电耀斑事件数

typedef struct {
    uint8_t  channel;               // 触发信道 (1~14)
    int8_t   peak_rssi;             // 峰值 RSSI (dBm)
    uint32_t packet_rate;           // 瞬间包密度 (包/秒)
    uint32_t timestamp_sec;         // 发生时间戳 (自开机秒数)
    char     description[24];       // 事件简述 (如 "强射电耀斑", "突发脉冲")
} cosmic_flare_event_t;

typedef struct {
    uint32_t total_packets;         // 累计捕获包数
    uint32_t current_pps;           // 当前瞬时包速率 (packets per second)
    int8_t   current_avg_rssi;      // 当前平均信号强度 (-100 ~ -30 dBm)
    int8_t   peak_rssi;             // 当前峰值信号强度
    uint8_t  active_channel;        // 当前监听信道 (1~14)
    uint8_t  channel_activity[COSMIC_RF_CHANNELS]; // 各信道活跃度 (0~100)
    uint8_t  flare_count;           // 捕获的电磁耀斑总数
    cosmic_flare_event_t recent_flares[COSMIC_MAX_FLARE_EVENTS];
} cosmic_rf_snapshot_t;

// ============================================================================
// 2. 户外暗夜与“电磁荒野”静区罗盘 (Radio Quiet Scout)
// ============================================================================
typedef enum {
    QUIET_LEVEL_STORM = 0,          // 0~29 分: 赛博风暴 (重度电磁污染)
    QUIET_LEVEL_SUBURBAN,           // 30~59分: 近郊村落 (中度背景干扰)
    QUIET_LEVEL_WILDERNESS,         // 60~89分: 自然原野 (良好自然场)
    QUIET_LEVEL_SANCTUARY,          // 90~100分: 纯净深空 (零辐射荒野避风港)
} cosmic_quiet_level_t;

typedef enum {
    ASTRO_VIEW_NORMAL = 0,          // 正常彩色全景星空 HUD
    ASTRO_VIEW_RED_NIGHT,           // 620nm 天文专用暗红光护眼模式 (保护视紫红质暗适应)
} cosmic_astro_view_mode_t;

typedef struct {
    uint8_t              rqi_score;         // 电磁荒野纯度评分 (0~100)
    cosmic_quiet_level_t level;             // 等级分类
    cosmic_astro_view_mode_t view_mode;     // 当前观星夜视模式
    uint32_t             packet_density;    // 采样窗口包密度
    int8_t               noise_floor_dbm;   // 环境底噪 (-100 ~ -40 dBm)
    const char          *level_name;        // 中文评级字样 (纯 const)
    const char          *advice;            // 观测与扎营建议
} cosmic_quiet_report_t;

// ============================================================================
// 3. 空间站过境推算与太空天气 (Space Weather & Orbit Tracker)
// ============================================================================
typedef enum {
    REGION_NORTH_CHINA = 0,         // 华北地区 (北京/天津/河北/山西/内蒙古)
    REGION_EAST_CHINA,              // 华东地区 (上海/江苏/浙江/山东/安徽)
    REGION_SOUTH_CHINA,             // 华南地区 (广东/广西/海南/福建/香港/澳门)
    REGION_SOUTHWEST,               // 西南地区 (四川/重庆/云南/贵州/西藏)
    REGION_NORTHWEST,               // 西北地区 (陕西/甘肃/青海/宁夏/新疆)
    REGION_COUNT
} cosmic_region_t;

typedef enum {
    SAT_TIANGONG_CSS = 0,           // 中国天宫空间站 (CSS)
    SAT_ISS,                        // 国际空间站 (ISS)
    SAT_COUNT
} cosmic_sat_t;

typedef struct {
    cosmic_sat_t sat_type;          // 目标空间站
    const char  *sat_name;          // 空间站中文名称
    cosmic_region_t region;         // 所在地理大区
    const char  *region_name;       // 大区中文名称
    uint32_t     countdown_sec;     // 距离下次最佳过境倒计时秒数
    uint16_t     duration_sec;      // 过境可见持续秒数
    uint8_t      max_elevation_deg; // 最大过境天顶仰角 (0~90°)
    uint16_t     azimuth_start_deg; // 入场方位角 (0~360°)
    uint16_t     azimuth_end_deg;   // 离场方位角 (0~360°)
    const char  *azimuth_desc;      // 方位描述 (如 "西北至东南")
    int8_t       apparent_mag;      // 视星等 (-2 等左右代表肉眼极亮可见)
    bool         is_transiting;     // 当前是否正在天空划过
} cosmic_orbit_pass_t;

typedef struct {
    uint8_t  kp_index;              // 地磁 Kp 指数 (0~9级, ≥5级为地磁暴)
    char     solar_flare_class;     // 太阳耀斑等级 ('A', 'B', 'C', 'M', 'X')
    float    solar_flux;            // 太阳射电通量指数 (sfu)
    uint8_t  aurora_chance_pct;     // 极光观测概率 (0~100%)
    const char *geomag_status;      // 地磁状态中文 (宁静 / 弱扰动 / 地磁暴)
} cosmic_space_weather_t;

// ============================================================================
// 4. 荒野多模射电求救信标 (Wilderness Radio SOS Beacon)
// ============================================================================
typedef struct {
    bool     is_active;             // 信标求救是否已激活
    uint32_t elapsed_sec;           // 求救信标持续发射总秒数
    uint32_t distress_cycle_count;  // 发射的求救周期轮数
    int      battery_soc;           // 当前剩余电量百分比 (0~100%)
    uint8_t  morse_state;           // 当前莫尔斯时序步进 (0~17)
    bool     morse_light_on;        // 当前目视白光爆闪是否点亮
    bool     ble_beacon_active;     // BLE 救援广播是否正在发射
    bool     wifi_beacon_active;    // Wi-Fi 救援 AP 是否正在广播
} cosmic_sos_status_t;

#ifdef __cplusplus
}
#endif
