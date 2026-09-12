// main/tracker_ble_scanner.c —— NimBLE 连续被动监听与抓包器实现
#include "tracker_ble_scanner.h"
#include "tracker_engine.h"

#include <stdio.h>
#include <string.h>

#if defined(ESP_PLATFORM)
#include "esp_log.h"
#include "esp_timer.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"

static const char *TAG = "ble_scanner";
static bool s_ble_started = false;
static bool s_scanning = false;
static uint32_t s_packet_count = 0;

static int ble_gap_event_handler(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    if (event->type == BLE_GAP_EVENT_DISC) {
        s_packet_count++;
        struct ble_gap_disc_desc *disc = &event->disc;
        if (disc->length_data > 0 && disc->data != NULL) {
            tracker_type_t type = tracker_parse_adv(disc->data, disc->length_data);
            if (type != TRACKER_TYPE_UNKNOWN) {
                uint32_t now_sec = (uint32_t)(esp_timer_get_time() / 1000000ULL);
                tracker_feed_beacon(disc->addr.val, disc->addr.type, type, disc->rssi, now_sec);
            }
        }
        return 0;
    }

    if (event->type == BLE_GAP_EVENT_DISC_COMPLETE) {
        // 扫描完成或超时后，若仍处于开启状态则自动重新启动连续扫描
        if (s_scanning) {
            struct ble_gap_disc_params params = {
                .filter_duplicates = 0, // 不过滤重复包，确保实时 RSSI 更新
                .passive = 1,           // 被动扫描：静默侦听，不发送 scan request，隐蔽性极佳
                .itvl = 0x0040,         // 扫描间隔 40ms
                .window = 0x0030,       // 扫描窗口 30ms (75% 占空比)
                .filter_policy = BLE_HCI_SCAN_FILT_NO_WL,
                .limited = 0,
            };
            ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &params, ble_gap_event_handler, NULL);
        }
        return 0;
    }

    return 0;
}

static void on_ble_sync(void)
{
    struct ble_gap_disc_params params = {
        .filter_duplicates = 0,
        .passive = 1,
        .itvl = 0x0040,
        .window = 0x0030,
        .filter_policy = BLE_HCI_SCAN_FILT_NO_WL,
        .limited = 0,
    };
    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER, &params, ble_gap_event_handler, NULL);
    if (rc == 0) {
        s_scanning = true;
        ESP_LOGI(TAG, "BLE passive anti-tracker scanning started");
    } else {
        ESP_LOGE(TAG, "Failed to start BLE discovery, rc=%d", rc);
    }
}

static void ble_host_task(void *param)
{
    (void)param;
    ESP_LOGI(TAG, "BLE host task running");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

bool tracker_ble_scanner_start(void)
{
    if (s_scanning) return true;

    if (!s_ble_started) {
        int rc = nimble_port_init();
        if (rc != 0) {
            ESP_LOGE(TAG, "nimble_port_init failed, rc=%d", rc);
            return false;
        }

        ble_hs_cfg.sync_cb = on_ble_sync;
        nimble_port_freertos_init(ble_host_task);
        s_ble_started = true;
    } else {
        on_ble_sync();
    }

    return true;
}

void tracker_ble_scanner_stop(void)
{
    if (!s_scanning) return;
    s_scanning = false;
    ble_gap_disc_cancel();
    ESP_LOGI(TAG, "BLE scanner stopped");
}

bool tracker_ble_scanner_is_active(void)
{
    return s_scanning;
}

uint32_t tracker_ble_scanner_get_packet_count(void)
{
    return s_packet_count;
}

#else
// Host 仿真模式
bool tracker_ble_scanner_start(void) { return true; }
void tracker_ble_scanner_stop(void) {}
bool tracker_ble_scanner_is_active(void) { return true; }
uint32_t tracker_ble_scanner_get_packet_count(void) { return 0; }
#endif
