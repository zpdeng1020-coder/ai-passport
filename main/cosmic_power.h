// main/cosmic_power.h —— 星原探针智能电源管理与口袋防误触状态机
#pragma once

#include "bsp_button.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COSMIC_POWER_ON = 0,    // 正常全亮 (100% 亮度)
    COSMIC_POWER_DIM,       // 无操作 35 秒微暗 (20% 亮度)
    COSMIC_POWER_SLEEP,     // 无操作 45 秒完全熄屏 (0% 亮度，后台持续工作)
} cosmic_power_state_t;

// 初始化电源管理器
void cosmic_power_init(void);

// 定时器心跳驱动 (建议每 1000ms 调用一次)
void cosmic_power_tick(uint32_t delta_ms);

// 唤醒屏幕至全亮 (重置闲置计时器)
void cosmic_power_wake(void);

// 设置常亮抑制 (如观星红光模式或 SOS 求救模式时强制常亮)
void cosmic_power_set_keep_alive(bool enable);

// 查询当前是否处于常亮抑制状态
bool cosmic_power_is_keep_alive(void);

// 口袋防误触按键拦截器
// 返回 true 表示该事件已被电源管理器作为唤醒消费，底层业务绝不透传；返回 false 表示屏幕已亮，正常透传业务
bool cosmic_power_handle_key_event(bsp_btn_t btn, bsp_btn_ev_t ev);

// 获取当前电源状态
cosmic_power_state_t cosmic_power_get_state(void);

#ifdef __cplusplus
}
#endif
