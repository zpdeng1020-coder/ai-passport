// main/tracker_ble_scanner.h —— NimBLE 连续被动监听与抓包器
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 初始化并启动 BLE 连续被动监听扫描器
bool tracker_ble_scanner_start(void);

// 停止扫描
void tracker_ble_scanner_stop(void);

// 检查是否正在扫描
bool tracker_ble_scanner_is_active(void);

// 获取累计捕获的 BLE 广播包总数
uint32_t tracker_ble_scanner_get_packet_count(void);

#ifdef __cplusplus
}
#endif
