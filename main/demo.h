// main/demo.h —— 每个演示页实现的统一接口。
// 新增一个演示页 = 实现这三个函数 + 在 main.c 的 DEMOS[] 里加一行。
#pragma once

#include "bsp_button.h"

typedef struct {
    const char *name;
    void (*enter)(void);                          // 建自己的屏并载入
    void (*exit)(void);                           // 删屏、停定时器、释放资源
    void (*key)(bsp_btn_t btn, bsp_btn_ev_t ev);  // 收按键(长按确定已被 main 拦截)
} demo_entry_t;

// 各演示页(定义在各自的 .c 里)
void demo_display_enter(void); void demo_display_exit(void);
void demo_display_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void demo_button_enter(void);  void demo_button_exit(void);
void demo_button_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void demo_audio_enter(void);   void demo_audio_exit(void);
void demo_audio_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void demo_battery_enter(void); void demo_battery_exit(void);
void demo_battery_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void demo_wifi_enter(void);    void demo_wifi_exit(void);
void demo_wifi_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void demo_ble_enter(void);     void demo_ble_exit(void);
void demo_ble_key(bsp_btn_t btn, bsp_btn_ev_t ev);

void demo_low_power_enter(void); void demo_low_power_exit(void);
void demo_low_power_key(bsp_btn_t btn, bsp_btn_ev_t ev);

// AI Passport 随身安全哨兵四大核心功能
void demo_radar_enter(void);     void demo_radar_exit(void);     void demo_radar_key(bsp_btn_t btn, bsp_btn_ev_t ev);
void demo_hotcold_enter(void);   void demo_hotcold_exit(void);   void demo_hotcold_key(bsp_btn_t btn, bsp_btn_ev_t ev);
void demo_wifi_spy_enter(void);  void demo_wifi_spy_exit(void);  void demo_wifi_spy_key(bsp_btn_t btn, bsp_btn_ev_t ev);
void demo_whitelist_enter(void); void demo_whitelist_exit(void); void demo_whitelist_key(bsp_btn_t btn, bsp_btn_ev_t ev);

// 星原探针：宇宙与地球射电漫步者四大核心功能
void demo_cosmic_sonar_enter(void);  void demo_cosmic_sonar_exit(void);  void demo_cosmic_sonar_key(bsp_btn_t btn, bsp_btn_ev_t ev);
void demo_quiet_scout_enter(void);   void demo_quiet_scout_exit(void);   void demo_quiet_scout_key(bsp_btn_t btn, bsp_btn_ev_t ev);
void demo_orbit_tracker_enter(void); void demo_orbit_tracker_exit(void); void demo_orbit_tracker_key(bsp_btn_t btn, bsp_btn_ev_t ev);
void demo_sos_beacon_enter(void);    void demo_sos_beacon_exit(void);    void demo_sos_beacon_key(bsp_btn_t btn, bsp_btn_ev_t ev);

