// main/cosmic_sonar.h —— 宇宙射电声呐与能量瀑布流引擎
#pragma once

#include "cosmic_types.h"

#ifdef __cplusplus
extern "C" {
#endif

// 初始化射电声呐模块与音频合成器
void cosmic_sonar_init(void);

// 启动射电侦听与声化
void cosmic_sonar_start(void);

// 停止射电侦听
void cosmic_sonar_stop(void);

// 查询声呐是否正在运行
bool cosmic_sonar_is_running(void);

// 射电空口数据包注入 (由 Wi-Fi 混杂或定时采样驱动)
void cosmic_sonar_feed_packet(int8_t rssi, uint8_t channel);

// 定期驱动状态机、信道轮转与瀑布流下滚 (建议每 100ms 调用一次)
void cosmic_sonar_tick(void);

// 获取最新射电声呐快照
void cosmic_sonar_get_snapshot(cosmic_rf_snapshot_t *snapshot);

// 获取瀑布流历史热力图矩阵数据 (指针指向 rows x cols 强度矩阵, 0~255)
// cols 固定为 24 个频段格，rows 固定为 60 行历史
#define COSMIC_SPECTRUM_BINS 24
#define COSMIC_SPECTRUM_ROWS 60
const uint8_t *cosmic_sonar_get_waterfall_matrix(void);

// 切换当前音频声化状态 (开启 / 静音)
bool cosmic_sonar_toggle_audio(void);
bool cosmic_sonar_is_audio_enabled(void);

#ifdef __cplusplus
}
#endif
