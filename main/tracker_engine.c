// main/tracker_engine.c —— 反追踪核心检测引擎与判定状态机实现
#include "tracker_engine.h"
#include <string.h>
#include <stdio.h>

static tracker_beacon_t s_beacons[TRACKER_MAX_BEACONS];
static size_t s_beacon_count = 0;

static tracker_whitelist_item_t s_whitelist[TRACKER_MAX_WHITELIST];
static size_t s_whitelist_count = 0;

static uint32_t s_last_tick_sec = 0;
static int s_selected_idx = -1;

void tracker_engine_init(void)
{
    memset(s_beacons, 0, sizeof(s_beacons));
    s_beacon_count = 0;
    memset(s_whitelist, 0, sizeof(s_whitelist));
    s_whitelist_count = 0;
    s_last_tick_sec = 0;
    s_selected_idx = -1;
}

void tracker_engine_reset(void)
{
    memset(s_beacons, 0, sizeof(s_beacons));
    s_beacon_count = 0;
    s_selected_idx = -1;
}

tracker_type_t tracker_parse_adv(const uint8_t *payload, uint8_t payload_len)
{
    if (!payload || payload_len < 3) {
        return TRACKER_TYPE_UNKNOWN;
    }

    uint8_t offset = 0;
    while (offset + 1 < payload_len) {
        uint8_t len = payload[offset];
        if (len == 0) {
            break; // 结束或填充
        }
        if (offset + 1 + len > payload_len) {
            break; // 越界
        }

        uint8_t type = payload[offset + 1];
        const uint8_t *data = &payload[offset + 2];
        uint8_t data_len = len - 1;

        // 1. 检查 Manufacturer Specific Data (0xFF)
        if (type == 0xFF && data_len >= 2) {
            uint16_t company_id = (uint16_t)(data[0] | (data[1] << 8));

            // Apple Inc. (0x004c)
            if (company_id == 0x004C) {
                if (data_len >= 3) {
                    uint8_t apple_type = data[2];
                    // 0x12: Find My / AirTag 离线寻找或失联广播
                    if (apple_type == 0x12) {
                        return TRACKER_TYPE_AIRTAG;
                    }
                    // 0x10: Nearby Action / 0x07: 靠近配对 / 0x05: AirDrop
                    if (apple_type == 0x10 || apple_type == 0x07 || apple_type == 0x05) {
                        return TRACKER_TYPE_APPLE_DEV;
                    }
                }
            }
            // Samsung Electronics (0x0075) -> SmartTag
            else if (company_id == 0x0075) {
                return TRACKER_TYPE_SMARTTAG;
            }
            // Tile, Inc. (0x011A)
            else if (company_id == 0x011A) {
                return TRACKER_TYPE_TILE;
            }
            // Huawei Device (0x027D)
            else if (company_id == 0x027D) {
                return TRACKER_TYPE_HUAWEI;
            }
        }

        // 2. 检查 16-bit Service UUIDs (0x02 / 0x03) 或 Service Data (0x16)
        if ((type == 0x02 || type == 0x03 || type == 0x16) && data_len >= 2) {
            uint16_t uuid = (uint16_t)(data[0] | (data[1] << 8));
            // Tile 的固定服务 UUID 0xFEED
            if (uuid == 0xFEED) {
                return TRACKER_TYPE_TILE;
            }
        }

        offset += (1 + len);
    }

    return TRACKER_TYPE_UNKNOWN;
}

static int find_beacon_index(const uint8_t mac[6])
{
    for (size_t i = 0; i < s_beacon_count; i++) {
        if (memcmp(s_beacons[i].mac, mac, 6) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static void evaluate_single_beacon(tracker_beacon_t *b)
{
    if (b->is_whitelisted) {
        b->threat = THREAT_LEVEL_SAFE;
        return;
    }

    uint32_t duration = (b->last_seen_sec >= b->first_seen_sec)
                      ? (b->last_seen_sec - b->first_seen_sec)
                      : 0;

    // 威胁判定阶梯：
    // 持续伴随超过 5 分钟 (300秒) + 平滑 RSSI 处于近场 (>= -78 dBm) + 命中至少 5 次
    if (duration >= TRACKER_ALERT_TIME_SEC &&
        b->smoothed_rssi >= TRACKER_ALERT_MIN_RSSI &&
        b->hits >= 5) {
        b->threat = THREAT_LEVEL_ALERT;
    }
    // 伴随超过 1 分钟 (60秒) + 命中至少 2 次
    else if (duration >= TRACKER_NOTICE_TIME_SEC && b->hits >= 2) {
        b->threat = THREAT_LEVEL_NOTICE;
    } else {
        b->threat = THREAT_LEVEL_SAFE;
    }
}

bool tracker_feed_beacon(const uint8_t mac[6], uint8_t addr_type,
                         tracker_type_t type, int8_t rssi,
                         uint32_t now_sec)
{
    if (!mac || type == TRACKER_TYPE_UNKNOWN) {
        return false;
    }

    int idx = find_beacon_index(mac);
    if (idx >= 0) {
        // 更新已有信标
        tracker_beacon_t *b = &s_beacons[idx];
        b->type = type;
        b->addr_type = addr_type;
        b->rssi = rssi;
        // 指数移动平均平滑滤波 (EMA: 75% 历史 + 25% 新值)
        b->smoothed_rssi = (int8_t)((b->smoothed_rssi * 3 + rssi * 1) / 4);
        b->last_seen_sec = now_sec;
        b->hits++;
        b->is_whitelisted = tracker_whitelist_is_present(mac);
        evaluate_single_beacon(b);
        return true;
    }

    // 新增信标
    if (s_beacon_count < TRACKER_MAX_BEACONS) {
        idx = (int)s_beacon_count++;
    } else {
        // 队列已满：寻找最久未更新且非高危告警的条目进行替换 (LRU)
        int oldest_idx = -1;
        uint32_t oldest_time = 0xFFFFFFFF;
        for (size_t i = 0; i < TRACKER_MAX_BEACONS; i++) {
            if (s_beacons[i].threat != THREAT_LEVEL_ALERT && s_beacons[i].last_seen_sec < oldest_time) {
                oldest_time = s_beacons[i].last_seen_sec;
                oldest_idx = (int)i;
            }
        }
        if (oldest_idx < 0) {
            oldest_idx = 0; // 全是高危告警时保守替换首个
        }
        idx = oldest_idx;
    }

    tracker_beacon_t *new_b = &s_beacons[idx];
    memcpy(new_b->mac, mac, 6);
    new_b->addr_type = addr_type;
    new_b->type = type;
    new_b->rssi = rssi;
    new_b->smoothed_rssi = rssi;
    new_b->first_seen_sec = now_sec;
    new_b->last_seen_sec = now_sec;
    new_b->hits = 1;
    new_b->is_whitelisted = tracker_whitelist_is_present(mac);
    evaluate_single_beacon(new_b);

    return true;
}

void tracker_engine_tick(uint32_t now_sec)
{
    s_last_tick_sec = now_sec;

    // 清理老化设备 (Stale Purge)
    size_t i = 0;
    while (i < s_beacon_count) {
        tracker_beacon_t *b = &s_beacons[i];
        uint32_t inactive_sec = (now_sec >= b->last_seen_sec) ? (now_sec - b->last_seen_sec) : 0;

        // 高危设备保留更久 (600秒)，普通设备超过 180秒 移除
        uint32_t timeout = (b->threat == THREAT_LEVEL_ALERT) ? 600 : TRACKER_STALE_TIMEOUT_SEC;
        if (inactive_sec > timeout) {
            // 将末尾条目移过来填充
            if (i + 1 < s_beacon_count) {
                s_beacons[i] = s_beacons[s_beacon_count - 1];
            }
            s_beacon_count--;
            // 不递增 i，继续检查当前位置被替换进来的元素
        } else {
            // 重新评估威胁（伴随时长增长）
            evaluate_single_beacon(b);
            i++;
        }
    }
}

void tracker_get_summary(tracker_summary_t *out_summary)
{
    if (!out_summary) return;

    out_summary->current_time_sec = s_last_tick_sec;
    out_summary->beacon_count = s_beacon_count;
    out_summary->active_count = 0;
    out_summary->alert_count = 0;
    out_summary->max_threat = THREAT_LEVEL_SAFE;
    out_summary->selected_idx = s_selected_idx;

    for (size_t i = 0; i < s_beacon_count; i++) {
        const tracker_beacon_t *b = &s_beacons[i];
        uint32_t inactive_sec = (s_last_tick_sec >= b->last_seen_sec)
                              ? (s_last_tick_sec - b->last_seen_sec)
                              : 0;
        if (inactive_sec <= 60) {
            out_summary->active_count++;
        }
        if (b->threat == THREAT_LEVEL_ALERT) {
            out_summary->alert_count++;
        }
        if (b->threat > out_summary->max_threat) {
            out_summary->max_threat = b->threat;
        }
    }
}

const tracker_beacon_t *tracker_get_beacon(size_t index)
{
    if (index >= s_beacon_count) {
        return NULL;
    }
    return &s_beacons[index];
}

bool tracker_whitelist_add(const uint8_t mac[6], const char *name)
{
    if (!mac) return false;
    if (tracker_whitelist_is_present(mac)) return true;
    if (s_whitelist_count >= TRACKER_MAX_WHITELIST) return false;

    tracker_whitelist_item_t *item = &s_whitelist[s_whitelist_count++];
    memcpy(item->mac, mac, 6);
    item->active = true;
    if (name) {
        snprintf(item->name, sizeof(item->name), "%s", name);
    } else {
        snprintf(item->name, sizeof(item->name), "%02X%02X%02X", mac[3], mac[4], mac[5]);
    }

    // 同步更新信标列表状态
    int idx = find_beacon_index(mac);
    if (idx >= 0) {
        s_beacons[idx].is_whitelisted = true;
        evaluate_single_beacon(&s_beacons[idx]);
    }
    return true;
}

bool tracker_whitelist_remove(const uint8_t mac[6])
{
    if (!mac) return false;
    for (size_t i = 0; i < s_whitelist_count; i++) {
        if (memcmp(s_whitelist[i].mac, mac, 6) == 0) {
            if (i + 1 < s_whitelist_count) {
                s_whitelist[i] = s_whitelist[s_whitelist_count - 1];
            }
            s_whitelist_count--;
            int b_idx = find_beacon_index(mac);
            if (b_idx >= 0) {
                s_beacons[b_idx].is_whitelisted = false;
                evaluate_single_beacon(&s_beacons[b_idx]);
            }
            return true;
        }
    }
    return false;
}

bool tracker_whitelist_is_present(const uint8_t mac[6])
{
    if (!mac) return false;
    for (size_t i = 0; i < s_whitelist_count; i++) {
        if (memcmp(s_whitelist[i].mac, mac, 6) == 0 && s_whitelist[i].active) {
            return true;
        }
    }
    return false;
}

bool tracker_whitelist_toggle(const uint8_t mac[6])
{
    if (tracker_whitelist_is_present(mac)) {
        return tracker_whitelist_remove(mac);
    } else {
        return tracker_whitelist_add(mac, NULL);
    }
}

size_t tracker_whitelist_count(void)
{
    return s_whitelist_count;
}

const tracker_whitelist_item_t *tracker_whitelist_get(size_t index)
{
    if (index >= s_whitelist_count) return NULL;
    return &s_whitelist[index];
}

int tracker_rssi_to_proximity_pct(int8_t rssi)
{
    // 典型 BLE RSSI 范围：-100 dBm (边缘) ~ -40 dBm (贴身)
    if (rssi <= -100) return 0;
    if (rssi >= -40)  return 100;
    return (int)((rssi - (-100)) * 100 / 60);
}

const char *tracker_type_to_str(tracker_type_t type)
{
    switch (type) {
        case TRACKER_TYPE_AIRTAG:    return "Apple AirTag";
        case TRACKER_TYPE_APPLE_DEV: return "Apple 随身设备";
        case TRACKER_TYPE_SMARTTAG:  return "三星 SmartTag";
        case TRACKER_TYPE_TILE:      return "Tile 寻物瓷贴";
        case TRACKER_TYPE_HUAWEI:    return "华为 寻物标签";
        default:                     return "未知信标";
    }
}

const char *threat_level_to_str(threat_level_t threat)
{
    switch (threat) {
        case THREAT_LEVEL_SAFE:   return "安全";
        case THREAT_LEVEL_NOTICE: return "未知提醒";
        case THREAT_LEVEL_ALERT:  return "高危伴随!";
        default:                  return "无";
    }
}
