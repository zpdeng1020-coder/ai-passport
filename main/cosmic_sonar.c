// main/cosmic_sonar.c —— 宇宙射电声呐与能量瀑布流引擎实现
#include "cosmic_sonar.h"
#include <string.h>
#include <stdlib.h>

#ifdef ESP_PLATFORM
#include "esp_wifi.h"
#include "esp_log.h"
#include "bsp_audio.h"
static const char *TAG = "cosmic_sonar";
#endif

static cosmic_rf_snapshot_t s_snapshot;
static uint8_t s_waterfall[COSMIC_SPECTRUM_ROWS][COSMIC_SPECTRUM_BINS];
static bool    s_running = false;
static bool    s_audio_enabled = true;
static uint32_t s_window_packets = 0;
static int32_t  s_window_rssi_sum = 0;
static uint32_t s_tick_counter = 0;

#ifdef ESP_PLATFORM
static void wifi_sniffer_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
    (void)type;
    if (!s_running) return;
    const wifi_promiscuous_pkt_t *pkt = (const wifi_promiscuous_pkt_t *)buf;
    cosmic_sonar_feed_packet(pkt->rx_ctrl.rssi, pkt->rx_ctrl.channel);
}
#endif

void cosmic_sonar_init(void) {
    memset(&s_snapshot, 0, sizeof(s_snapshot));
    memset(s_waterfall, 0, sizeof(s_waterfall));
    s_snapshot.current_avg_rssi = -95;
    s_snapshot.peak_rssi = -95;
    s_snapshot.active_channel = 1;
    s_running = false;
    s_audio_enabled = true;
    s_window_packets = 0;
    s_window_rssi_sum = 0;
    s_tick_counter = 0;

    // 填充微弱初始星空底噪
    for (int r = 0; r < COSMIC_SPECTRUM_ROWS; r++) {
        for (int c = 0; c < COSMIC_SPECTRUM_BINS; c++) {
            s_waterfall[r][c] = (uint8_t)(r % 3);
        }
    }
}

void cosmic_sonar_start(void) {
    s_running = true;
#ifdef ESP_PLATFORM
    wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_ALL
    };
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(wifi_sniffer_cb);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    ESP_LOGI(TAG, "射电混杂侦听开启");
#endif
}

void cosmic_sonar_stop(void) {
    s_running = false;
#ifdef ESP_PLATFORM
    esp_wifi_set_promiscuous(false);
    ESP_LOGI(TAG, "射电混杂侦听关闭");
#endif
}

bool cosmic_sonar_is_running(void) {
    return s_running;
}

void cosmic_sonar_feed_packet(int8_t rssi, uint8_t channel) {
    if (!s_running) return;

    s_snapshot.total_packets++;
    s_window_packets++;
    s_window_rssi_sum += rssi;

    if (rssi > s_snapshot.peak_rssi) {
        s_snapshot.peak_rssi = rssi;
    }

    if (channel >= 1 && channel <= COSMIC_RF_CHANNELS) {
        if (s_snapshot.channel_activity[channel - 1] < 250) {
            s_snapshot.channel_activity[channel - 1] += 5;
        }
    }

    // 检测突发强射电耀斑 (PPS突增或极强信号)
    if (rssi >= -45) {
        if (s_snapshot.flare_count < COSMIC_MAX_FLARE_EVENTS) {
            cosmic_flare_event_t *ev = &s_snapshot.recent_flares[s_snapshot.flare_count++];
            ev->channel = channel;
            ev->peak_rssi = rssi;
            ev->packet_rate = s_snapshot.current_pps;
            ev->timestamp_sec = s_tick_counter / 10;
            strncpy(ev->description, "突发强高能脉冲", sizeof(ev->description) - 1);
        }
    }
}

void cosmic_sonar_tick(void) {
    s_tick_counter++;

    // 计算瞬时 PPS (每 10 个 100ms 周期或每 100ms 统计)
    s_snapshot.current_pps = s_window_packets * 10;
    if (s_window_packets > 0) {
        s_snapshot.current_avg_rssi = (int8_t)(s_window_rssi_sum / (int32_t)s_window_packets);
    } else {
        s_snapshot.current_avg_rssi = -95;
    }
    s_window_packets = 0;
    s_window_rssi_sum = 0;

    // 各信道活跃度自然衰减
    for (int i = 0; i < COSMIC_RF_CHANNELS; i++) {
        if (s_snapshot.channel_activity[i] > 2) {
            s_snapshot.channel_activity[i] -= 2;
        } else {
            s_snapshot.channel_activity[i] = 0;
        }
    }

#ifdef ESP_PLATFORM
    // 信道轮转: 每 200ms 切换一次 2.4GHz 监听信道 (1~13)
    if (s_tick_counter % 2 == 0) {
        s_snapshot.active_channel = (s_snapshot.active_channel % 13) + 1;
        esp_wifi_set_channel(s_snapshot.active_channel, WIFI_SECOND_CHAN_NONE);
    }
#endif

    // 瀑布流下滚 (第 0 行移至第 1 行，腾出第 0 行填入最新采样)
    for (int r = COSMIC_SPECTRUM_ROWS - 1; r > 0; r--) {
        memcpy(s_waterfall[r], s_waterfall[r - 1], COSMIC_SPECTRUM_BINS);
    }

    // 构建第 0 行能量谱
    for (int c = 0; c < COSMIC_SPECTRUM_BINS; c++) {
        int ch_idx = (c * COSMIC_RF_CHANNELS) / COSMIC_SPECTRUM_BINS;
        uint8_t base_val = s_snapshot.channel_activity[ch_idx];
        // 加上当前信道瞬时强度与微弱随机宇宙噪声
        if (ch_idx + 1 == s_snapshot.active_channel && s_snapshot.current_pps > 0) {
            uint16_t val = (uint16_t)base_val + (uint16_t)(s_snapshot.current_pps / 5);
            s_waterfall[0][c] = (val > 255) ? 255 : (uint8_t)val;
        } else {
            s_waterfall[0][c] = (base_val > 10) ? base_val : (uint8_t)((s_tick_counter + c) % 4);
        }
    }

#ifdef ESP_PLATFORM
    // 脉冲星声音合成: 根据瞬时 PPS 输出 ES8311 变频合成粒子脉冲
    if (s_audio_enabled && s_running) {
        static int16_t pcm_chunk[64];
        bool is_pulse = (s_snapshot.current_pps > 10 && (s_tick_counter % 3 == 0));
        for (int i = 0; i < 64; i++) {
            if (is_pulse) {
                // 脉冲星爆鸣 (方波/谐波)
                pcm_chunk[i] = (i % 8 < 4) ? 4000 : -4000;
            } else {
                // 宇宙背景微弱粉红噪声
                pcm_chunk[i] = (int16_t)((rand() % 400) - 200);
            }
        }
        bsp_audio_write(pcm_chunk, sizeof(pcm_chunk));
    }
#endif
}

void cosmic_sonar_get_snapshot(cosmic_rf_snapshot_t *snapshot) {
    if (!snapshot) return;
    *snapshot = s_snapshot;
}

const uint8_t *cosmic_sonar_get_waterfall_matrix(void) {
    return (const uint8_t *)s_waterfall;
}

bool cosmic_sonar_toggle_audio(void) {
    s_audio_enabled = !s_audio_enabled;
    return s_audio_enabled;
}

bool cosmic_sonar_is_audio_enabled(void) {
    return s_audio_enabled;
}
