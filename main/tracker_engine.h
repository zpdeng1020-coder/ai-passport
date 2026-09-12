// main/tracker_engine.h —— 反追踪核心检测引擎与判定状态机接口
#pragma once

#include "tracker_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化检测引擎
void tracker_engine_init(void);

// 重置引擎所有状态（清空设备列表，保留白名单）
void tracker_engine_reset(void);

// 解析 BLE 原始广播 Payload，识别是否为追踪信标特征
tracker_type_t tracker_parse_adv(const uint8_t *payload, uint8_t payload_len);

// 将捕获的信标数据更新入状态机
bool tracker_feed_beacon(const uint8_t mac[6], uint8_t addr_type,
                         tracker_type_t type, int8_t rssi,
                         uint32_t now_sec);

// 定时调用：执行老化淘汰、威胁等级综合评估
void tracker_engine_tick(uint32_t now_sec);

// 获取全局汇总信息
void tracker_get_summary(tracker_summary_t *out_summary);

// 获取指定索引的信标条目（0 <= index < beacon_count）
const tracker_beacon_t *tracker_get_beacon(size_t index);

// 白名单操作
bool tracker_whitelist_add(const uint8_t mac[6], const char *name);
bool tracker_whitelist_remove(const uint8_t mac[6]);
bool tracker_whitelist_is_present(const uint8_t mac[6]);
bool tracker_whitelist_toggle(const uint8_t mac[6]);
size_t tracker_whitelist_count(void);
const tracker_whitelist_item_t *tracker_whitelist_get(size_t index);

// 辅助函数：根据 RSSI 估算距离与信号描述
// 返回 0~100 的信号强度百分比，越靠近 100 越近 (HOT)
int tracker_rssi_to_proximity_pct(int8_t rssi);
const char *tracker_type_to_str(tracker_type_t type);
const char *threat_level_to_str(threat_level_t threat);

#ifdef __cplusplus
}
#endif
