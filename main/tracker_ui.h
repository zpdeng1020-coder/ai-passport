// main/tracker_ui.h —— AI Passport 随身安全哨兵 各功能页面接口
#pragma once

#include "bsp_button.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 1. 防追踪雷达
void demo_radar_enter(void);
void demo_radar_exit(void);
void demo_radar_key(bsp_btn_t btn, bsp_btn_ev_t ev);

// 2. 冷热寻物测距
void demo_hotcold_enter(void);
void demo_hotcold_exit(void);
void demo_hotcold_key(bsp_btn_t btn, bsp_btn_ev_t ev);

// 3. 无线偷拍排查
void demo_wifi_spy_enter(void);
void demo_wifi_spy_exit(void);
void demo_wifi_spy_key(bsp_btn_t btn, bsp_btn_ev_t ev);

// 4. 信任白名单
void demo_whitelist_enter(void);
void demo_whitelist_exit(void);
void demo_whitelist_key(bsp_btn_t btn, bsp_btn_ev_t ev);

#ifdef __cplusplus
}
#endif
