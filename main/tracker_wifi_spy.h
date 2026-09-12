// main/tracker_wifi_spy.h —— 可疑无线偷拍摄像头与隐藏热点排查模块
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SPY_MAX_RESULTS 16

typedef enum {
    SPY_REASON_NONE = 0,
    SPY_REASON_HIDDEN_SSID,      // 隐藏广播热点 (关闭SSID广播常用于躲避排查)
    SPY_REASON_CAMERA_PREFIX,    // 典型偷拍/监控网络命名前缀匹配 (如 V380, LookCam, IPCAM)
    SPY_REASON_VENDOR_OUI,       // 常见安防/偷拍模组芯片 OUI (涂鸦, 雄迈, 乐鑫IoT)
    SPY_REASON_SUSPICIOUS_STRONG // 近场强信号未识别设备 (> -48 dBm, 疑似同屋藏匿)
} spy_reason_t;

typedef struct {
    char ssid[33];
    uint8_t bssid[6];
    int8_t rssi;
    uint8_t channel;
    bool is_hidden;
    spy_reason_t reason;
    char vendor_tag[24];         // 厂商模组标签
} spy_candidate_t;

#ifdef __cplusplus
extern "C" {
#endif

void tracker_wifi_spy_init(void);
bool tracker_wifi_spy_start_scan(void);
bool tracker_wifi_spy_is_busy(void);
size_t tracker_wifi_spy_get_candidates(spy_candidate_t *out_list, size_t max_count);
size_t tracker_wifi_spy_get_total_scanned(void);
bool tracker_wifi_spy_has_scanned(void);

// 纯逻辑评估函数 (供 host tests 及驱动层共用)
spy_reason_t tracker_wifi_eval_ap(const char *ssid, const uint8_t bssid[6],
                                  int8_t rssi, bool is_hidden,
                                  char *out_vendor, size_t vendor_cap);
const char *tracker_spy_reason_to_str(spy_reason_t reason);

#ifdef __cplusplus
}
#endif
