// main/cosmic_quiet_scout.h —— 户外暗夜与“电磁荒野”静区罗盘算法与状态管理
#pragma once

#include "cosmic_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化电磁荒野罗盘模块
void cosmic_quiet_init(void);

// 计算电磁荒野纯度评分 (纯函数，适合 Host 单元测试)
uint8_t cosmic_quiet_calculate_rqi(uint32_t pps, int8_t avg_rssi);

// 根据瞬时空口数据更新荒野评估状态
void cosmic_quiet_update(uint32_t pps, int8_t avg_rssi);

// 获取最新电磁荒野评估报告
void cosmic_quiet_get_report(cosmic_quiet_report_t *report);

// 切换天文观星暗红光模式 (Normal <-> Red Night)
cosmic_astro_view_mode_t cosmic_quiet_toggle_view_mode(void);

// 获取当前观星视界模式
cosmic_astro_view_mode_t cosmic_quiet_get_view_mode(void);

#ifdef __cplusplus
}
#endif
