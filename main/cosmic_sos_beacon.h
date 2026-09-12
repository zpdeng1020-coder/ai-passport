// main/cosmic_sos_beacon.h —— 荒野多模射电求救信标 (BLE + Wi-Fi + 莫尔斯声光)
#pragma once

#include "cosmic_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化求救信标模块
void cosmic_sos_init(void);

// 启动/激活荒野多模求救信标 (发射 BLE + Wi-Fi + 莫尔斯时序)
void cosmic_sos_start(void);

// 停止求救信标
void cosmic_sos_stop(void);

// 查询当前信标是否处于激活呼救状态
bool cosmic_sos_is_active(void);

// 每 100ms 驱动莫尔斯电码时序步进
// 返回当前是否需要点亮爆闪白光，以及当前蜂鸣音调 (Hz, 0 表示静音)
void cosmic_sos_tick_100ms(int battery_soc, bool *out_light_on, uint16_t *out_audio_hz);

// 获取最新求救信标运行状态
void cosmic_sos_get_status(cosmic_sos_status_t *status);

#ifdef __cplusplus
}
#endif
