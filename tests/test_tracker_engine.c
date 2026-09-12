// tests/test_tracker_engine.c —— 反追踪核心检测引擎单元测试
#include "tracker_engine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_parse_airtag(void)
{
    // 典型的 Apple Find My 广播 Payload
    // [0]=0x1e (len 30), [1]=0xff (Manufacturer), [2]=0x4c, [3]=0x00 (Apple), [4]=0x12 (Find My), ...
    uint8_t airtag_adv[] = {
        0x02, 0x01, 0x1a,                               // Flags
        0x1b, 0xff, 0x4c, 0x00, 0x12, 0x19, 0x10,       // Apple Find My header
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
        0x0d, 0x0e, 0x0f
    };
    tracker_type_t t = tracker_parse_adv(airtag_adv, sizeof(airtag_adv));
    assert(t == TRACKER_TYPE_AIRTAG);

    // 随机普通广播包
    uint8_t normal_adv[] = {
        0x02, 0x01, 0x06,
        0x05, 0x09, 'T', 'e', 's', 't'
    };
    t = tracker_parse_adv(normal_adv, sizeof(normal_adv));
    assert(t == TRACKER_TYPE_UNKNOWN);

    // Samsung SmartTag 广播 (len = 1 byte type + 6 bytes data = 7)
    uint8_t samsung_adv[] = {
        0x02, 0x01, 0x06,
        0x07, 0xff, 0x75, 0x00, 0x01, 0x02, 0x03, 0x04
    };
    t = tracker_parse_adv(samsung_adv, sizeof(samsung_adv));
    assert(t == TRACKER_TYPE_SMARTTAG);

    // Tile 广播 (Service UUID 0xFEED: len = 1 byte type + 2 bytes UUID = 3)
    uint8_t tile_adv[] = {
        0x02, 0x01, 0x06,
        0x03, 0x03, 0xed, 0xfe
    };
    t = tracker_parse_adv(tile_adv, sizeof(tile_adv));
    assert(t == TRACKER_TYPE_TILE);

    printf("PASS: test_parse_airtag & beacons\n");
}

static void test_threat_timeline(void)
{
    tracker_engine_init();

    uint8_t mac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    uint32_t now = 1000;

    // 第一次捕获：安全
    bool ok = tracker_feed_beacon(mac, 0, TRACKER_TYPE_AIRTAG, -70, now);
    assert(ok);
    tracker_engine_tick(now);

    tracker_summary_t sum;
    tracker_get_summary(&sum);
    assert(sum.beacon_count == 1);
    assert(sum.alert_count == 0);
    assert(sum.max_threat == THREAT_LEVEL_SAFE);

    const tracker_beacon_t *b = tracker_get_beacon(0);
    assert(b != NULL);
    assert(b->threat == THREAT_LEVEL_SAFE);

    // 经过 70 秒（> 60秒）：黄色 NOTICE 提示
    now += 70;
    tracker_feed_beacon(mac, 0, TRACKER_TYPE_AIRTAG, -68, now);
    tracker_engine_tick(now);

    tracker_get_summary(&sum);
    assert(sum.max_threat == THREAT_LEVEL_NOTICE);
    assert(sum.alert_count == 0);

    // 模拟持续近场伴随：每隔 60 秒捕获一次，直到累计伴随超过 300 秒 (5分钟)
    for (int i = 0; i < 5; i++) {
        now += 60;
        tracker_feed_beacon(mac, 0, TRACKER_TYPE_AIRTAG, -65, now);
        tracker_engine_tick(now);
    }

    // 此时持续时间已达 70 + 300 = 370 秒，hits >= 7，近场强信号 -> 触发 ALERT!
    tracker_get_summary(&sum);
    assert(sum.max_threat == THREAT_LEVEL_ALERT);
    assert(sum.alert_count == 1);

    b = tracker_get_beacon(0);
    assert(b->threat == THREAT_LEVEL_ALERT);
    assert(b->smoothed_rssi >= -70);

    printf("PASS: test_threat_timeline\n");
}

static void test_whitelist(void)
{
    tracker_engine_init();

    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint32_t now = 1000;

    // 先模拟让其变成高危设备
    for (int i = 0; i < 6; i++) {
        now += 65;
        tracker_feed_beacon(mac, 0, TRACKER_TYPE_AIRTAG, -60, now);
        tracker_engine_tick(now);
    }
    tracker_summary_t sum;
    tracker_get_summary(&sum);
    assert(sum.max_threat == THREAT_LEVEL_ALERT);

    // 将其加入白名单
    bool added = tracker_whitelist_add(mac, "MyOwnAirTag");
    assert(added);
    assert(tracker_whitelist_is_present(mac));

    // 验证立即降级为安全
    tracker_get_summary(&sum);
    assert(sum.max_threat == THREAT_LEVEL_SAFE);
    assert(sum.alert_count == 0);

    const tracker_beacon_t *b = tracker_get_beacon(0);
    assert(b->is_whitelisted == true);
    assert(b->threat == THREAT_LEVEL_SAFE);

    // 移出白名单
    bool removed = tracker_whitelist_remove(mac);
    assert(removed);
    assert(!tracker_whitelist_is_present(mac));

    // 移出后重新评估恢复告警
    tracker_get_summary(&sum);
    assert(sum.max_threat == THREAT_LEVEL_ALERT);
    assert(sum.alert_count == 1);

    printf("PASS: test_whitelist\n");
}

static void test_stale_purge(void)
{
    tracker_engine_init();

    uint8_t mac[6] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    uint32_t now = 1000;

    // 仅短暂出现一次的普通陌生设备
    tracker_feed_beacon(mac, 0, TRACKER_TYPE_AIRTAG, -85, now);
    tracker_summary_t sum;
    tracker_get_summary(&sum);
    assert(sum.beacon_count == 1);

    // 经过 190 秒（> 180 秒超时）未再收到信号
    now += 190;
    tracker_engine_tick(now);

    tracker_get_summary(&sum);
    assert(sum.beacon_count == 0); // 应该已被自动清理

    printf("PASS: test_stale_purge\n");
}

#include "tracker_wifi_spy.h"

static void test_wifi_spy_eval(void)
{
    char vbuf[32] = {0};
    uint8_t zero_bssid[6] = {0};
    uint8_t tuya_bssid[6] = {0x10, 0x5A, 0xF7, 0x11, 0x22, 0x33};
    uint8_t xm_bssid[6]   = {0x00, 0x12, 0x12, 0x44, 0x55, 0x66};
    uint8_t normal_bssid[6] = {0xAC, 0x85, 0x3D, 0x11, 0x22, 0x33};

    // 1. 隐藏且信号强 (>= -70dBm) -> SPY_REASON_HIDDEN_SSID
    assert(tracker_wifi_eval_ap("", zero_bssid, -65, true, vbuf, sizeof(vbuf)) == SPY_REASON_HIDDEN_SSID);
    assert(tracker_wifi_eval_ap(NULL, zero_bssid, -50, false, vbuf, sizeof(vbuf)) == SPY_REASON_HIDDEN_SSID);
    // 隐藏但极弱信号 (例如隔了很远的路人) -> SPY_REASON_NONE
    assert(tracker_wifi_eval_ap("", zero_bssid, -85, true, vbuf, sizeof(vbuf)) == SPY_REASON_NONE);

    // 2. 摄像头特征 SSID
    assert(tracker_wifi_eval_ap("CAM-3901A", zero_bssid, -75, false, vbuf, sizeof(vbuf)) == SPY_REASON_CAMERA_PREFIX);
    assert(tracker_wifi_eval_ap("ipcam_livingroom", zero_bssid, -80, false, vbuf, sizeof(vbuf)) == SPY_REASON_CAMERA_PREFIX);
    assert(tracker_wifi_eval_ap("Tuya_Plug_01", zero_bssid, -70, false, vbuf, sizeof(vbuf)) == SPY_REASON_CAMERA_PREFIX);
    assert(tracker_wifi_eval_ap("LOOKCAM_99B", zero_bssid, -60, false, vbuf, sizeof(vbuf)) == SPY_REASON_CAMERA_PREFIX);
    assert(tracker_wifi_eval_ap("V380_Pro_88", zero_bssid, -60, false, vbuf, sizeof(vbuf)) == SPY_REASON_CAMERA_PREFIX);

    // 3. 安防模组 OUI 匹配
    assert(tracker_wifi_eval_ap("Office-Router", tuya_bssid, -65, false, vbuf, sizeof(vbuf)) == SPY_REASON_VENDOR_OUI);
    assert(strcmp(vbuf, "涂鸦智能") == 0);

    assert(tracker_wifi_eval_ap("Unknown-AP", xm_bssid, -60, false, vbuf, sizeof(vbuf)) == SPY_REASON_VENDOR_OUI);
    assert(strcmp(vbuf, "雄迈安防") == 0);

    // 4. 普通合法 SSID & 普通 MAC
    assert(tracker_wifi_eval_ap("ChinaNet-Home", normal_bssid, -65, false, vbuf, sizeof(vbuf)) == SPY_REASON_NONE);
    assert(tracker_wifi_eval_ap("Starbucks-Free", normal_bssid, -50, false, vbuf, sizeof(vbuf)) == SPY_REASON_NONE);

    printf("PASS: test_wifi_spy_eval (with OUI vendor matching)\n");
}

int main(void)
{
    printf("--- Running Tracker Engine Unit Tests ---\n");
    test_parse_airtag();
    test_threat_timeline();
    test_whitelist();
    test_stale_purge();
    test_wifi_spy_eval();
    printf("ALL TRACKER ENGINE TESTS PASSED!\n");
    return 0;
}
