// tests/test_cosmic_engine.c —— 星原探针算法与状态机 Host 单元测试
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "cosmic_types.h"
#include "cosmic_quiet_scout.h"
#include "cosmic_orbit_weather.h"
#include "cosmic_sos_beacon.h"
#include "cosmic_sonar.h"

static void test_quiet_scout_rqi(void) {
    printf("[TEST] Running Radio Quiet Index (RQI) scoring tests...\n");

    // 1. 绝对纯净环境 (0 pps, -95dBm 底噪) => 应该满分 100 分
    uint8_t score_pure = cosmic_quiet_calculate_rqi(0, -95);
    printf("  Pure Sanctuary Score: %d\n", score_pure);
    assert(score_pure >= 95 && score_pure <= 100);

    // 2. 荒野原野 (5 pps, -85dBm) => 应在 60~89 分之间
    uint8_t score_wild = cosmic_quiet_calculate_rqi(5, -85);
    printf("  Wilderness Score: %d\n", score_wild);
    assert(score_wild >= 60 && score_wild <= 89);

    // 3. 近郊村落 (50 pps, -70dBm) => 应在 30~59 分之间
    uint8_t score_suburb = cosmic_quiet_calculate_rqi(50, -70);
    printf("  Suburban Score: %d\n", score_suburb);
    assert(score_suburb >= 30 && score_suburb <= 59);

    // 4. 赛博风暴 (800 pps, -45dBm 极强干扰) => 应在 0~29 分之间
    uint8_t score_storm = cosmic_quiet_calculate_rqi(800, -45);
    printf("  Cyber Storm Score: %d\n", score_storm);
    assert(score_storm <= 29);

    // 5. 状态报告与暗红光切换验证
    cosmic_quiet_init();
    cosmic_quiet_update(0, -96);
    cosmic_quiet_report_t rep;
    cosmic_quiet_get_report(&rep);
    assert(rep.level == QUIET_LEVEL_SANCTUARY);
    assert(strcmp(rep.level_name, "纯净深空") == 0);

    // 切换至红光模式
    assert(cosmic_quiet_get_view_mode() == ASTRO_VIEW_NORMAL);
    cosmic_astro_view_mode_t m = cosmic_quiet_toggle_view_mode();
    assert(m == ASTRO_VIEW_RED_NIGHT);
    assert(cosmic_quiet_get_view_mode() == ASTRO_VIEW_RED_NIGHT);

    printf("  -> RQI & Quiet Scout Tests Passed!\n\n");
}

static void test_orbit_weather(void) {
    printf("[TEST] Running Orbit Tracker & Space Weather tests...\n");

    cosmic_orbit_init();
    cosmic_orbit_pass_t pass;
    cosmic_orbit_get_pass(&pass);
    assert(pass.sat_type == SAT_TIANGONG_CSS);
    assert(pass.region == REGION_NORTH_CHINA);
    assert(pass.max_elevation_deg >= 70); // 华北天宫近天顶
    assert(pass.countdown_sec > 0);

    // 切换到 ISS
    cosmic_sat_t sat = cosmic_orbit_next_sat();
    assert(sat == SAT_ISS);
    cosmic_orbit_get_pass(&pass);
    assert(pass.sat_type == SAT_ISS);
    assert(strcmp(pass.sat_name, "国际空间站") == 0);

    // 切换区域
    cosmic_region_t reg = cosmic_orbit_next_region();
    assert(reg == REGION_EAST_CHINA);
    cosmic_orbit_get_pass(&pass);
    assert(pass.region == REGION_EAST_CHINA);
    assert(strcmp(pass.region_name, "华东地区") == 0);

    // 步进测试
    uint32_t init_cd = pass.countdown_sec;
    cosmic_orbit_tick_1s();
    cosmic_orbit_get_pass(&pass);
    assert(pass.countdown_sec == init_cd - 1);

    // 太空天气
    cosmic_space_weather_t w;
    cosmic_orbit_get_weather(&w);
    assert(w.kp_index >= 0 && w.kp_index <= 9);

    printf("  -> Orbit Tracker & Space Weather Tests Passed!\n\n");
}

static void test_sos_beacon(void) {
    printf("[TEST] Running SOS Beacon Morse & State Machine tests...\n");

    cosmic_sos_init();
    assert(!cosmic_sos_is_active());

    cosmic_sos_start();
    assert(cosmic_sos_is_active());

    bool light_on = false;
    uint16_t audio_hz = 0;

    // 前 2 个 tick (0~200ms) 是 'S' 的第一个点 (Dot) => 灯亮，声音 1500Hz
    cosmic_sos_tick_100ms(90, &light_on, &audio_hz);
    assert(light_on == true);
    assert(audio_hz == 1500);

    cosmic_sos_tick_100ms(90, &light_on, &audio_hz);
    assert(light_on == true);
    assert(audio_hz == 1500);

    // 第 3 个 tick (200~300ms) 是点内间隔 => 灯灭，静音
    cosmic_sos_tick_100ms(90, &light_on, &audio_hz);
    assert(light_on == false);
    assert(audio_hz == 0);

    // 运行 100 个 tick (10秒)，验证周期推进与总秒数记录
    for (int i = 0; i < 97; i++) {
        cosmic_sos_tick_100ms(85, &light_on, &audio_hz);
    }

    cosmic_sos_status_t status;
    cosmic_sos_get_status(&status);
    assert(status.is_active == true);
    assert(status.elapsed_sec == 10);
    assert(status.distress_cycle_count >= 1); // 至少完成了一轮完整 SOS

    cosmic_sos_stop();
    assert(!cosmic_sos_is_active());
    cosmic_sos_tick_100ms(85, &light_on, &audio_hz);
    assert(light_on == false);
    assert(audio_hz == 0);

    printf("  -> SOS Beacon Morse & State Machine Tests Passed!\n\n");
}

int main(void) {
    printf("====================================================\n");
    printf("  Running Cosmic & Earth Radio Explorer Unit Tests  \n");
    printf("====================================================\n");

    test_quiet_scout_rqi();
    test_orbit_weather();
    test_sos_beacon();

    printf("[TEST] Running Cosmic RF Sonar engine tests...\n");
    cosmic_sonar_init();
    assert(!cosmic_sonar_is_running());
    cosmic_sonar_start();
    assert(cosmic_sonar_is_running());

    // 模拟注入数据包
    cosmic_sonar_feed_packet(-50, 6);
    cosmic_sonar_feed_packet(-42, 6); // 触发耀斑
    cosmic_sonar_tick();

    cosmic_rf_snapshot_t snap;
    cosmic_sonar_get_snapshot(&snap);
    assert(snap.total_packets == 2);
    assert(snap.flare_count >= 1);
    assert(snap.active_channel >= 1 && snap.active_channel <= 14);

    const uint8_t *matrix = cosmic_sonar_get_waterfall_matrix();
    assert(matrix != NULL);

    cosmic_sonar_stop();
    assert(!cosmic_sonar_is_running());
    printf("  -> Cosmic RF Sonar Tests Passed!\n\n");

    printf(">>> ALL HOST TESTS PASSED SUCCESSFULLY (100%%) <<<\n");
    return 0;
}
