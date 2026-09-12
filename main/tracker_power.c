// main/tracker_power.c —— AI Passport 随身安全哨兵智能电源与熄屏管理器实现
#include "tracker_power.h"
#include <stdio.h>

#if defined(ESP_PLATFORM)
#include "bsp_display.h"
#include "tracker_alarm.h"
#include "esp_timer.h"
#include "esp_log.h"
static const char *TAG = "tracker_power";

static void hw_set_backlight(uint8_t pct)
{
    bsp_display_backlight(pct);
}

static uint32_t hw_get_time_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void hw_play_wake_sound(void)
{
    tracker_alarm_beep(2000, 20);
}
#else
static uint8_t s_mock_backlight = 100;
static uint32_t s_mock_time_ms = 0;

static void hw_set_backlight(uint8_t pct)
{
    s_mock_backlight = pct;
}

static uint32_t hw_get_time_ms(void)
{
    return s_mock_time_ms;
}

static void hw_play_wake_sound(void)
{
}
#endif

#define TRACKER_POWER_DIM_TIMEOUT_MS  (35 * 1000)
#define TRACKER_POWER_OFF_TIMEOUT_MS  (45 * 1000)

static tracker_power_state_t s_power_state = TRACKER_POWER_STATE_ON;
static uint32_t s_last_activity_ms = 0;
static bool s_inhibited = false;

void tracker_power_init(void)
{
    s_power_state = TRACKER_POWER_STATE_ON;
    s_last_activity_ms = hw_get_time_ms();
    s_inhibited = false;
    hw_set_backlight(100);
#if defined(ESP_PLATFORM)
    ESP_LOGI(TAG, "智能熄屏省电模块就绪: 35s微暗/45s灭屏");
#endif
}

void tracker_power_feed_activity(void)
{
    s_last_activity_ms = hw_get_time_ms();
    if (s_power_state != TRACKER_POWER_STATE_ON) {
        s_power_state = TRACKER_POWER_STATE_ON;
        hw_set_backlight(100);
#if defined(ESP_PLATFORM)
        ESP_LOGD(TAG, "活动恢复: 屏幕背光点亮 100%%");
#endif
    }
}

void tracker_power_set_inhibit(bool inhibit)
{
    s_inhibited = inhibit;
    if (inhibit) {
        tracker_power_feed_activity();
    }
}

bool tracker_power_is_inhibited(void)
{
    return s_inhibited;
}

void tracker_power_wake(void)
{
    tracker_power_feed_activity();
}

bool tracker_power_is_screen_on(void)
{
    return (s_power_state != TRACKER_POWER_STATE_OFF);
}

tracker_power_state_t tracker_power_get_state(void)
{
    return s_power_state;
}

bool tracker_power_handle_key_event(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    (void)btn;
    // 仅针对按键抬起/点击或按下瞬间触发唤醒处理
    if (ev != BSP_BTN_CLICK && ev != BSP_BTN_PRESS && ev != BSP_BTN_LONG) {
        return false;
    }

    if (s_power_state == TRACKER_POWER_STATE_OFF) {
        // 处于完全熄屏态：首按仅负责唤醒点亮屏幕，消费本事件，不透传给业务层
        tracker_power_wake();
        hw_play_wake_sound();
#if defined(ESP_PLATFORM)
        ESP_LOGI(TAG, "按键唤醒屏幕: 拦截并消费本事件，防止口袋误触");
#endif
        return true;
    }

    // 若处于微暗状态，恢复全亮并允许事件透传
    if (s_power_state == TRACKER_POWER_STATE_DIM) {
        tracker_power_feed_activity();
    } else {
        s_last_activity_ms = hw_get_time_ms();
    }

    return false;
}

void tracker_power_tick(uint32_t now_ms)
{
    if (now_ms == 0) {
        now_ms = hw_get_time_ms();
    }

    if (s_inhibited) {
        s_last_activity_ms = now_ms;
        if (s_power_state != TRACKER_POWER_STATE_ON) {
            s_power_state = TRACKER_POWER_STATE_ON;
            hw_set_backlight(100);
        }
        return;
    }

    uint32_t elapsed = (now_ms >= s_last_activity_ms) ? (now_ms - s_last_activity_ms) : 0;

    if (s_power_state == TRACKER_POWER_STATE_ON) {
        if (elapsed >= TRACKER_POWER_OFF_TIMEOUT_MS) {
            s_power_state = TRACKER_POWER_STATE_OFF;
            hw_set_backlight(0);
#if defined(ESP_PLATFORM)
            ESP_LOGI(TAG, "无操作达到 %u ms, 执行熄屏省电 (背光 0%%)", (unsigned)elapsed);
#endif
        } else if (elapsed >= TRACKER_POWER_DIM_TIMEOUT_MS) {
            s_power_state = TRACKER_POWER_STATE_DIM;
            hw_set_backlight(20);
#if defined(ESP_PLATFORM)
            ESP_LOGD(TAG, "空闲预警: 降低背光至 20%%");
#endif
        }
    } else if (s_power_state == TRACKER_POWER_STATE_DIM) {
        if (elapsed >= TRACKER_POWER_OFF_TIMEOUT_MS) {
            s_power_state = TRACKER_POWER_STATE_OFF;
            hw_set_backlight(0);
#if defined(ESP_PLATFORM)
            ESP_LOGI(TAG, "两段式休眠: 完全熄屏 (背光 0%%)");
#endif
        }
    }
}
