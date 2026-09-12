// main/tracker_types.h —— 随身反追踪与无线安全探测器类型定义
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define TRACKER_MAX_BEACONS       16   // 内存有界设计：最多维护 16 个近场信标
#define TRACKER_MAX_WHITELIST     8    // 白名单容量
#define TRACKER_STALE_TIMEOUT_SEC 180  // 3 分钟未见则视为离开/老化清除
#define TRACKER_NOTICE_TIME_SEC   60   // 出现超过 1 分钟标记为留意 (Notice)
#define TRACKER_ALERT_TIME_SEC    300  // 持续伴随超过 5 分钟且信号强标记为高危 (Alert)
#define TRACKER_ALERT_MIN_RSSI    (-78) // 触发高危告警的最小平滑信号强度 (近场伴随)

typedef enum {
    TRACKER_TYPE_UNKNOWN = 0,
    TRACKER_TYPE_AIRTAG,        // Apple Find My / AirTag (Company 0x004c, Type 0x12)
    TRACKER_TYPE_APPLE_DEV,     // Apple 随身/查找设备 (Company 0x004c, Type 0x10/0x07/0x05)
    TRACKER_TYPE_SMARTTAG,      // Samsung SmartTag (Company 0x0075)
    TRACKER_TYPE_TILE,          // Tile Tracker (UUID 0xFEED / Company 0x011A)
    TRACKER_TYPE_HUAWEI,        // 华为查找标签
} tracker_type_t;

typedef enum {
    THREAT_LEVEL_SAFE = 0,      // 安全：无未知信标或仅在白名单内
    THREAT_LEVEL_NOTICE,        // 提示(黄色)：周围发现陌生追踪器，但伴随时间较短
    THREAT_LEVEL_ALERT,         // 告警(红色)：检测到持续近场伴随跟踪！
} threat_level_t;

// 单个被跟踪的信标实体
typedef struct {
    uint8_t mac[6];
    uint8_t addr_type;
    tracker_type_t type;
    int8_t rssi;                // 最近一次瞬时 RSSI (dBm)
    int8_t smoothed_rssi;       // 平滑后 RSSI
    uint32_t first_seen_sec;    // 首次被捕获的时间戳(秒)
    uint32_t last_seen_sec;     // 最近一次捕获的时间戳(秒)
    uint32_t hits;              // 捕获次数
    bool is_whitelisted;        // 是否在白名单中
    threat_level_t threat;      // 单设备威胁等级
} tracker_beacon_t;

// 白名单实体
typedef struct {
    uint8_t mac[6];
    bool active;
    char name[16];              // 自定义昵称，如 "MyKeys", "MyAirTag"
} tracker_whitelist_item_t;

// 全局扫描与防御状态快照
typedef struct {
    uint32_t current_time_sec;
    size_t beacon_count;
    size_t active_count;
    size_t alert_count;
    threat_level_t max_threat;
    int selected_idx;           // 当前光标聚焦的信标索引 (-1 为未选择)
} tracker_summary_t;
