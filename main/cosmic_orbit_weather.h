// main/cosmic_orbit_weather.h —— 空间站过境天体推算与地磁太空天气
#pragma once

#include "cosmic_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化空间站与太空天气模块
void cosmic_orbit_init(void);

// 计算指定空间站与地理大区的下一次过境窗口 (纯数学天体计算，适合 Host 单元测试)
void cosmic_orbit_compute_pass(cosmic_sat_t sat, cosmic_region_t region, uint32_t epoch_sec, cosmic_orbit_pass_t *out);

// 切换到下一个空间站目标 (天宫 <-> ISS)
cosmic_sat_t cosmic_orbit_next_sat(void);

// 切换到下一个地理区域 (华北 -> 华东 -> 华南 -> 西南 -> 西北)
cosmic_region_t cosmic_orbit_next_region(void);

// 每秒更新过境倒计时与天体视界步进
void cosmic_orbit_tick_1s(void);

// 获取当前空间站过境推算详情
void cosmic_orbit_get_pass(cosmic_orbit_pass_t *pass);

// 获取当前地球地磁与太阳活动指数
void cosmic_orbit_get_weather(cosmic_space_weather_t *weather);

#ifdef __cplusplus
}
#endif
