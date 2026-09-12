// main/tracker_alarm.h —— 声学告警与寻物蜂鸣器模块
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 初始化告警音频任务
void tracker_alarm_init(void);

// 触发短促提示音 (beep)
void tracker_alarm_beep(uint16_t freq_hz, uint16_t duration_ms);

// 触发连续高危警报 (两声短促急促蜂鸣)
void tracker_alarm_trigger_alert(void);

// 触发冷热测距寻物脉冲（根据 proximity 0~100% 决定滴答频率）
void tracker_alarm_tick_hotcold(int proximity_pct);

// 切换静音状态
bool tracker_alarm_toggle_mute(void);
bool tracker_alarm_is_muted(void);

#ifdef __cplusplus
}
#endif
