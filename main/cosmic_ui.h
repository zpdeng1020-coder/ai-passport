// main/cosmic_ui.h —— 星原探针四大核心模块 UI 视图控制器声明
#pragma once

#include "bsp_button.h"

#ifdef __cplusplus
extern "C" {
#endif

// 1. 宇宙射电声呐视图
void demo_cosmic_sonar_enter(void);
void demo_cosmic_sonar_exit(void);
void demo_cosmic_sonar_key(bsp_btn_t btn, bsp_btn_ev_t ev);

// 2. 户外暗夜与“电磁荒野”静区罗盘视图
void demo_quiet_scout_enter(void);
void demo_quiet_scout_exit(void);
void demo_quiet_scout_key(bsp_btn_t btn, bsp_btn_ev_t ev);

// 3. 空间站过境推算与太空天气视图
void demo_orbit_tracker_enter(void);
void demo_orbit_tracker_exit(void);
void demo_orbit_tracker_key(bsp_btn_t btn, bsp_btn_ev_t ev);

// 4. 荒野多模射电求救信标视图
void demo_sos_beacon_enter(void);
void demo_sos_beacon_exit(void);
void demo_sos_beacon_key(bsp_btn_t btn, bsp_btn_ev_t ev);

#ifdef __cplusplus
}
#endif
