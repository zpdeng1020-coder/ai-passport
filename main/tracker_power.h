// main/tracker_power.h —— AI Passport 随身安全哨兵智能电源与熄屏管理器
#pragma once

#if defined(ESP_PLATFORM)
#include "bsp_button.h"
#else
typedef enum {
    BSP_BTN_UP = 0,
    BSP_BTN_DOWN,
    BSP_BTN_OK,
} bsp_btn_t;

typedef enum {
    BSP_BTN_PRESS = 0,
    BSP_BTN_CLICK,
    BSP_BTN_DOUBLE,
    BSP_BTN_LONG,
} bsp_btn_ev_t;
#endif
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 电源状态
typedef enum {
    TRACKER_POWER_STATE_ON = 0,   // 亮屏全亮 (100%)
    TRACKER_POWER_STATE_DIM,      // 预备微暗 (20%)
    TRACKER_POWER_STATE_OFF       // 完全熄屏 (0%)
} tracker_power_state_t;

// 初始化电源管理模块
void tracker_power_init(void);

// 喂狗/刷新活动时间（按键或业务事件触发）
void tracker_power_feed_activity(void);

// 设置熄屏抑制（true: 禁止自动熄屏，例如冷热寻物、Wi-Fi扫描中、高危报警态）
void tracker_power_set_inhibit(bool inhibit);

// 查询当前是否处于熄屏抑制状态
bool tracker_power_is_inhibited(void);

// 强制唤醒/点亮屏幕至 100%（例如报警触发时调用）
void tracker_power_wake(void);

// 查询当前屏幕是否点亮（非 OFF）
bool tracker_power_is_screen_on(void);

// 当前电源状态
tracker_power_state_t tracker_power_get_state(void);

// 全局按键拦截处理器：
// 若屏幕当前已熄灭，此函数自动唤醒屏幕并返回 true（消费该事件，不透传给业务，防止口袋误触）；
// 若屏幕当前为点亮状态，此函数刷新活跃时间并返回 false（允许正常处理业务）。
bool tracker_power_handle_key_event(bsp_btn_t btn, bsp_btn_ev_t ev);

// 周期性时钟调用（驱动微暗与熄屏状态机，推荐每 100ms ~ 500ms 调用一次）
void tracker_power_tick(uint32_t now_ms);

#ifdef __cplusplus
}
#endif
