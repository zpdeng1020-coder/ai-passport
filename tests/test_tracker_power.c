// tests/test_tracker_power.c —— 智能电源与熄屏状态机单元测试
#include "tracker_power.h"
#include <assert.h>
#include <stdio.h>

extern uint32_t s_mock_time_ms;
extern uint8_t s_mock_backlight;

static void test_power_state_machine(void)
{
    tracker_power_init();
    assert(tracker_power_get_state() == TRACKER_POWER_STATE_ON);
    assert(tracker_power_is_screen_on() == true);
    assert(!tracker_power_is_inhibited());

    // 1. 经过 20 秒：保持亮屏 (100%)
    tracker_power_tick(20 * 1000);
    assert(tracker_power_get_state() == TRACKER_POWER_STATE_ON);

    // 2. 经过 36 秒（> 35s）：进入微暗模式 (DIM, 20%)
    tracker_power_tick(36 * 1000);
    assert(tracker_power_get_state() == TRACKER_POWER_STATE_DIM);
    assert(tracker_power_is_screen_on() == true);

    // 3. 经过 46 秒（> 45s）：进入完全熄屏模式 (OFF, 0%)
    tracker_power_tick(46 * 1000);
    assert(tracker_power_get_state() == TRACKER_POWER_STATE_OFF);
    assert(tracker_power_is_screen_on() == false);

    // 4. 在完全熄屏状态下按下按键：
    // 应被拦截（返回 true），且仅唤醒屏幕至全亮，不透传给业务层
    bool consumed = tracker_power_handle_key_event(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(consumed == true);
    assert(tracker_power_get_state() == TRACKER_POWER_STATE_ON);
    assert(tracker_power_is_screen_on() == true);

    // 5. 屏幕点亮后再次按键：
    // 不拦截（返回 false），允许透传业务
    consumed = tracker_power_handle_key_event(BSP_BTN_OK, BSP_BTN_CLICK);
    assert(consumed == false);

    printf("PASS: test_power_state_machine\n");
}

static void test_power_inhibit(void)
{
    tracker_power_init();
    tracker_power_set_inhibit(true);
    assert(tracker_power_is_inhibited());

    // 抑制熄屏状态下，即使过去 120 秒，也绝不熄屏！
    tracker_power_tick(120 * 1000);
    assert(tracker_power_get_state() == TRACKER_POWER_STATE_ON);
    assert(tracker_power_is_screen_on() == true);

    // 解除抑制后，超时才正常熄屏
    tracker_power_set_inhibit(false);
    assert(!tracker_power_is_inhibited());

    tracker_power_tick(120 * 1000 + 46 * 1000);
    assert(tracker_power_get_state() == TRACKER_POWER_STATE_OFF);

    // 测试强制唤醒 (Alert Wake)
    tracker_power_wake();
    assert(tracker_power_get_state() == TRACKER_POWER_STATE_ON);

    printf("PASS: test_power_inhibit & wake\n");
}

int main(void)
{
    printf("--- Running Tracker Power Unit Tests ---\n");
    test_power_state_machine();
    test_power_inhibit();
    printf("ALL TRACKER POWER TESTS PASSED!\n");
    return 0;
}
