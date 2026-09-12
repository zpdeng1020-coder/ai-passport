// main/cosmic_sos_beacon.c —— 荒野多模射电求救信标实现
#include "cosmic_sos_beacon.h"
#include <string.h>

static cosmic_sos_status_t s_sos;
static uint32_t            s_total_ticks = 0;
static uint16_t            s_pattern_tick = 0;

// 国际标准莫尔斯电码 "S O S" (点=200ms=2tick, 划=600ms=6tick, 字符内间隔=2tick, 字符间间隔=6tick, 单词间间隔=14tick)
typedef struct {
    uint8_t ticks;
    bool    on;
} morse_element_t;

static const morse_element_t SOS_PATTERN[] = {
    // S: . . .
    { 2, true },  { 2, false },
    { 2, true },  { 2, false },
    { 2, true },  { 6, false }, // 字母间隙

    // O: - - -
    { 6, true },  { 2, false },
    { 6, true },  { 2, false },
    { 6, true },  { 6, false }, // 字母间隙

    // S: . . .
    { 2, true },  { 2, false },
    { 2, true },  { 2, false },
    { 2, true },  { 14, false }, // 单词间隙 (长停顿)
};

#define SOS_PATTERN_LEN (sizeof(SOS_PATTERN) / sizeof(SOS_PATTERN[0]))

void cosmic_sos_init(void) {
    memset(&s_sos, 0, sizeof(s_sos));
    s_sos.is_active = false;
    s_sos.battery_soc = 100;
    s_total_ticks = 0;
    s_pattern_tick = 0;
}

void cosmic_sos_start(void) {
    s_sos.is_active = true;
    s_sos.elapsed_sec = 0;
    s_sos.distress_cycle_count = 0;
    s_sos.morse_state = 0;
    s_sos.morse_light_on = false;
    s_sos.ble_beacon_active = true;
    s_sos.wifi_beacon_active = true;
    s_total_ticks = 0;
    s_pattern_tick = 0;
}

void cosmic_sos_stop(void) {
    s_sos.is_active = false;
    s_sos.morse_light_on = false;
    s_sos.ble_beacon_active = false;
    s_sos.wifi_beacon_active = false;
    s_total_ticks = 0;
    s_pattern_tick = 0;
}

bool cosmic_sos_is_active(void) {
    return s_sos.is_active;
}

void cosmic_sos_tick_100ms(int battery_soc, bool *out_light_on, uint16_t *out_audio_hz) {
    if (!s_sos.is_active) {
        if (out_light_on) *out_light_on = false;
        if (out_audio_hz) *out_audio_hz = 0;
        return;
    }

    s_sos.battery_soc = battery_soc;
    s_total_ticks++;
    s_pattern_tick++;

    // 统计发射总秒数 (每 10 个 100ms tick 计 1 秒)
    if (s_total_ticks % 10 == 0) {
        s_sos.elapsed_sec++;
    }

    // 遍历当前莫尔斯序列
    uint16_t accumulated_ticks = 0;
    bool found = false;
    uint8_t current_state = 0;

    for (size_t i = 0; i < SOS_PATTERN_LEN; i++) {
        accumulated_ticks += SOS_PATTERN[i].ticks;
        if (s_pattern_tick <= accumulated_ticks) {
            current_state = (uint8_t)i;
            s_sos.morse_light_on = SOS_PATTERN[i].on;
            found = true;
            break;
        }
    }

    if (!found) {
        // 一个完整 SOS 周期结束，开启下一轮
        s_pattern_tick = 1;
        s_sos.distress_cycle_count++;
        current_state = 0;
        s_sos.morse_light_on = SOS_PATTERN[0].on;
    }

    s_sos.morse_state = current_state;

    if (out_light_on) {
        *out_light_on = s_sos.morse_light_on;
    }

    // 点亮时发出高穿透性 1500Hz 强音，熄灭时静音
    if (out_audio_hz) {
        *out_audio_hz = s_sos.morse_light_on ? 1500 : 0;
    }
}

void cosmic_sos_get_status(cosmic_sos_status_t *status) {
    if (!status) return;
    *status = s_sos;
}
