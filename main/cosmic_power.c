// main/cosmic_power.c —— 星原探针智能电源管理与口袋防误触实现
#include "cosmic_power.h"

#ifdef ESP_PLATFORM
#include "bsp_display.h"
#include "esp_log.h"
static const char *TAG = "cosmic_power";
#endif

#define DIM_TIMEOUT_SEC    35
#define SLEEP_TIMEOUT_SEC  45

static cosmic_power_state_t s_state = COSMIC_POWER_ON;
static uint32_t             s_idle_sec = 0;
static bool                 s_keep_alive = false;

void cosmic_power_init(void) {
    s_state = COSMIC_POWER_ON;
    s_idle_sec = 0;
    s_keep_alive = false;
#ifdef ESP_PLATFORM
    bsp_display_backlight(100);
#endif
}

void cosmic_power_tick(uint32_t delta_ms) {
    (void)delta_ms;
    if (s_keep_alive) {
        s_idle_sec = 0;
        return;
    }

    s_idle_sec++;

    if (s_state == COSMIC_POWER_ON && s_idle_sec >= DIM_TIMEOUT_SEC) {
        s_state = COSMIC_POWER_DIM;
#ifdef ESP_PLATFORM
        ESP_LOGI(TAG, "闲置 35 秒: 背光微暗 20%%");
        bsp_display_backlight(20);
#endif
    } else if (s_state == COSMIC_POWER_DIM && s_idle_sec >= SLEEP_TIMEOUT_SEC) {
        s_state = COSMIC_POWER_SLEEP;
#ifdef ESP_PLATFORM
        ESP_LOGI(TAG, "闲置 45 秒: 屏幕完全熄灭 0%% (后台持续保持侦听)");
        bsp_display_backlight(0);
#endif
    }
}

void cosmic_power_wake(void) {
    s_idle_sec = 0;
    s_state = COSMIC_POWER_ON;
#ifdef ESP_PLATFORM
    bsp_display_backlight(100);
#endif
}

void cosmic_power_set_keep_alive(bool enable) {
    s_keep_alive = enable;
    if (enable) {
        s_idle_sec = 0;
        s_state = COSMIC_POWER_ON;
    }
}

bool cosmic_power_is_keep_alive(void) {
    return s_keep_alive;
}

bool cosmic_power_handle_key_event(bsp_btn_t btn, bsp_btn_ev_t ev) {
    (void)btn;
    (void)ev;

    // 若当前处于完全熄屏待机状态:
    // 从口袋/背包掏出按下的第一下按键，仅负责唤醒屏幕，事件被电源层消费，不透传给底层业务
    if (s_state == COSMIC_POWER_SLEEP) {
        cosmic_power_wake();
        return true; // 拦截消费
    }

    // 若处于微暗状态，按键唤醒至全亮并允许透传
    if (s_state == COSMIC_POWER_DIM) {
        cosmic_power_wake();
    }

    s_idle_sec = 0;
    return false; // 正常透传业务
}

cosmic_power_state_t cosmic_power_get_state(void) {
    return s_state;
}
