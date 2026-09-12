// main/tracker_wifi_spy.c —— 可疑无线偷拍摄像头与隐藏热点排查模块实现
#include "tracker_wifi_spy.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>

#if defined(ESP_PLATFORM)
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "demo_radio.h"
static const char *TAG = "wifi_spy";

static esp_netif_t *s_sta_netif = NULL;
static esp_event_handler_instance_t s_scan_handler = NULL;
static bool s_wifi_initialized = false;
static uint16_t s_total_scanned_aps = 0;
static bool s_has_scanned = false;
#endif

static const char *const CAMERA_PREFIXES[] = {
    "CAM-", "CAM_", "IPCAM", "IPC-", "IPC_",
    "HD-", "HD_", "TUYA", "SMARTLIFE", "MINICAM",
    "V380", "XVR", "DVR-", "DVR_", "CARECAM", "SPY", "CAMERA",
    "LOOKCAM", "IWFCAM", "HDWIFICAM", "HISILICON", "WIFICAM"
};
#define CAMERA_PREFIX_COUNT (sizeof(CAMERA_PREFIXES) / sizeof(CAMERA_PREFIXES[0]))

typedef struct {
    uint8_t oui[3];
    const char *vendor_name;
} vendor_oui_entry_t;

static const vendor_oui_entry_t VENDOR_OUIS[] = {
    {{0x10, 0x5A, 0xF7}, "涂鸦智能"},
    {{0x20, 0xF8, 0x5E}, "涂鸦智能"},
    {{0x7C, 0xF6, 0x66}, "涂鸦智能"},
    {{0x68, 0x57, 0x2D}, "涂鸦智能"},
    {{0x84, 0xF3, 0xEB}, "涂鸦智能"},
    {{0xA0, 0x92, 0x08}, "涂鸦智能"},
    {{0xD8, 0x1F, 0x12}, "涂鸦智能"},
    {{0x00, 0x12, 0x12}, "雄迈安防"},
    {{0x00, 0x12, 0x16}, "雄迈安防"},
    {{0x00, 0x12, 0x17}, "雄迈安防"},
    {{0x00, 0x12, 0x18}, "雄迈安防"},
    {{0x44, 0x19, 0xB6}, "海康威视"},
    {{0x70, 0xB3, 0xD5}, "海康威视"},
    {{0xBC, 0x54, 0x51}, "海康威视"},
    {{0x18, 0x68, 0xCB}, "海康威视"},
    {{0x38, 0xAF, 0x29}, "大华安防"},
    {{0xE0, 0x50, 0x8B}, "大华安防"},
    {{0x4C, 0x11, 0xBF}, "大华安防"},
    {{0x90, 0x02, 0xA9}, "大华安防"},
    {{0x24, 0x0A, 0xC4}, "乐鑫IoT"},
    {{0x30, 0xAE, 0xA4}, "乐鑫IoT"},
    {{0xA4, 0xCF, 0x12}, "乐鑫IoT"},
    {{0xDC, 0x4F, 0x22}, "乐鑫IoT"},
};
#define VENDOR_OUI_COUNT (sizeof(VENDOR_OUIS) / sizeof(VENDOR_OUIS[0]))

static const char *lookup_vendor(const uint8_t bssid[6])
{
    if (!bssid) return NULL;
    for (size_t i = 0; i < VENDOR_OUI_COUNT; i++) {
        if (memcmp(bssid, VENDOR_OUIS[i].oui, 3) == 0) {
            return VENDOR_OUIS[i].vendor_name;
        }
    }
    return NULL;
}

static spy_candidate_t s_results[SPY_MAX_RESULTS];
static size_t s_result_count = 0;
static bool s_scanning = false;

spy_reason_t tracker_wifi_eval_ap(const char *ssid, const uint8_t bssid[6],
                                  int8_t rssi, bool is_hidden,
                                  char *out_vendor, size_t vendor_cap)
{
    const char *vname = lookup_vendor(bssid);
    if (out_vendor && vendor_cap > 0) {
        if (vname) {
            snprintf(out_vendor, vendor_cap, "%s", vname);
        } else {
            out_vendor[0] = '\0';
        }
    }

    // 1. 检查摄像头关键词（忽略大小写前缀匹配）
    if (ssid && ssid[0] != '\0') {
        for (size_t i = 0; i < CAMERA_PREFIX_COUNT; i++) {
            if (strncasecmp(ssid, CAMERA_PREFIXES[i], strlen(CAMERA_PREFIXES[i])) == 0) {
                return SPY_REASON_CAMERA_PREFIX;
            }
        }
    }

    // 2. 隐藏 SSID 热点：关闭广播躲避发现且信号强 (>= -70 dBm)
    if (is_hidden || !ssid || ssid[0] == '\0') {
        if (rssi >= -70) {
            return SPY_REASON_HIDDEN_SSID;
        }
        return SPY_REASON_NONE;
    }

    // 3. 安防/监控芯片模组 MAC OUI 识别 (若在近距离且为安防模组)
    if (vname != NULL && rssi >= -75) {
        return SPY_REASON_VENDOR_OUI;
    }

    // 4. 贴身极强信号（例如在房间内 < -45dBm）未识别热点
    if (rssi >= -45) {
        return SPY_REASON_SUSPICIOUS_STRONG;
    }

    return SPY_REASON_NONE;
}

const char *tracker_spy_reason_to_str(spy_reason_t reason)
{
    switch (reason) {
        case SPY_REASON_HIDDEN_SSID:      return "隐藏广播热点";
        case SPY_REASON_CAMERA_PREFIX:    return "摄像头热点";
        case SPY_REASON_VENDOR_OUI:       return "安防模组芯片";
        case SPY_REASON_SUSPICIOUS_STRONG:return "贴身强信号源";
        default:                          return "正常设备";
    }
}

#if defined(ESP_PLATFORM)
static void wifi_scan_done_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id; (void)data;
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    s_total_scanned_aps = ap_count;
    s_has_scanned = true;
    s_result_count = 0;

    if (ap_count > 0) {
        wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
        if (ap_list) {
            if (esp_wifi_scan_get_ap_records(&ap_count, ap_list) == ESP_OK) {
                for (uint16_t i = 0; i < ap_count && s_result_count < SPY_MAX_RESULTS; i++) {
                    bool hidden = (ap_list[i].ssid[0] == '\0');
                    char vendor[24] = {0};
                    spy_reason_t reason = tracker_wifi_eval_ap((const char *)ap_list[i].ssid,
                                                               ap_list[i].bssid,
                                                               ap_list[i].rssi,
                                                               hidden,
                                                               vendor,
                                                               sizeof(vendor));
                    if (reason != SPY_REASON_NONE) {
                        spy_candidate_t *cand = &s_results[s_result_count++];
                        strncpy(cand->ssid, (const char *)ap_list[i].ssid, sizeof(cand->ssid) - 1);
                        cand->ssid[sizeof(cand->ssid) - 1] = '\0';
                        memcpy(cand->bssid, ap_list[i].bssid, 6);
                        cand->rssi = ap_list[i].rssi;
                        cand->channel = ap_list[i].primary;
                        cand->is_hidden = hidden;
                        cand->reason = reason;
                        strncpy(cand->vendor_tag, vendor, sizeof(cand->vendor_tag) - 1);
                        cand->vendor_tag[sizeof(cand->vendor_tag) - 1] = '\0';
                    }
                }
            }
            free(ap_list);
        }
    }
    s_scanning = false;
    ESP_LOGI(TAG, "Wi-Fi spy scan complete, total APs: %u, suspicious count: %u",
             (unsigned)s_total_scanned_aps, (unsigned)s_result_count);
}
#endif

void tracker_wifi_spy_init(void)
{
    memset(s_results, 0, sizeof(s_results));
    s_result_count = 0;
    s_scanning = false;

#if defined(ESP_PLATFORM)
    s_total_scanned_aps = 0;
    s_has_scanned = false;
    demo_radio_nvs_prepare();
    demo_radio_network_prepare();
    if (!s_sta_netif) {
        s_sta_netif = esp_netif_create_default_wifi_sta();
    }
    if (!s_wifi_initialized) {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&cfg);
        esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE,
                                            wifi_scan_done_handler, NULL, &s_scan_handler);
        esp_wifi_set_storage(WIFI_STORAGE_RAM);
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_start();
        s_wifi_initialized = true;
        ESP_LOGI(TAG, "Wi-Fi stack initialized for spy detection");
    }
#endif
}

bool tracker_wifi_spy_is_busy(void)
{
    return s_scanning;
}

size_t tracker_wifi_spy_get_candidates(spy_candidate_t *out_list, size_t max_count)
{
    if (!out_list || max_count == 0) return 0;
    size_t copy_cnt = (s_result_count < max_count) ? s_result_count : max_count;
    memcpy(out_list, s_results, copy_cnt * sizeof(spy_candidate_t));
    return copy_cnt;
}

size_t tracker_wifi_spy_get_total_scanned(void)
{
#if defined(ESP_PLATFORM)
    return s_total_scanned_aps;
#else
    return 0;
#endif
}

bool tracker_wifi_spy_has_scanned(void)
{
#if defined(ESP_PLATFORM)
    return s_has_scanned;
#else
    return false;
#endif
}

bool tracker_wifi_spy_start_scan(void)
{
#if defined(ESP_PLATFORM)
    if (s_scanning) return false;
    if (!s_wifi_initialized) {
        tracker_wifi_spy_init();
    }
    s_scanning = true;
    wifi_scan_config_t config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true, // 关键：抓取隐藏 SSID
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };
    esp_err_t err = esp_wifi_scan_start(&config, false);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_scan_start failed: %s", esp_err_to_name(err));
        s_scanning = false;
        return false;
    }
    return true;
#else
    // Host 仿真模式
    return true;
#endif
}
