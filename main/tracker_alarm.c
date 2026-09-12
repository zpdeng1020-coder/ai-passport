// main/tracker_alarm.c —— 声学告警与寻物蜂鸣器模块实现
#include "tracker_alarm.h"
#include <math.h>
#include <string.h>

#if defined(ESP_PLATFORM)
#include "bsp_audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "tracker_alarm";
static bool s_muted = false;
static QueueHandle_t s_tone_queue = NULL;

typedef struct {
    uint16_t freq_hz;
    uint16_t duration_ms;
} tone_cmd_t;

#define SAMPLE_RATE 16000
#define BUFFER_SAMPLES 256

static void alarm_task(void *arg)
{
    (void)arg;
    int16_t pcm_buf[BUFFER_SAMPLES];
    bsp_audio_set_format(SAMPLE_RATE, 16, 1);
    bsp_audio_set_volume(80);

    tone_cmd_t cmd;
    while (1) {
        if (xQueueReceive(s_tone_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            if (s_muted || cmd.duration_ms == 0 || cmd.freq_hz == 0) {
                continue;
            }

            size_t total_samples = (SAMPLE_RATE * cmd.duration_ms) / 1000;
            size_t generated = 0;
            double phase = 0.0;
            double phase_inc = (2.0 * M_PI * cmd.freq_hz) / SAMPLE_RATE;

            while (generated < total_samples) {
                size_t chunk = total_samples - generated;
                if (chunk > BUFFER_SAMPLES) chunk = BUFFER_SAMPLES;

                for (size_t i = 0; i < chunk; i++) {
                    // 正弦波，加轻微衰减包络防破音
                    double env = 1.0;
                    if (generated + i < 80) env = (double)(generated + i) / 80.0;
                    else if (total_samples - (generated + i) < 80) env = (double)(total_samples - (generated + i)) / 80.0;

                    pcm_buf[i] = (int16_t)(sin(phase) * 16000.0 * env);
                    phase += phase_inc;
                    if (phase >= 2.0 * M_PI) phase -= 2.0 * M_PI;
                }

                bsp_audio_write(pcm_buf, chunk * sizeof(int16_t));
                generated += chunk;
            }
        }
    }
}

void tracker_alarm_init(void)
{
    if (s_tone_queue == NULL) {
        s_tone_queue = xQueueCreate(4, sizeof(tone_cmd_t));
        xTaskCreate(alarm_task, "alarm_task", 3072, NULL, 5, NULL);
        ESP_LOGI(TAG, "Alarm worker task started");
    }
}

void tracker_alarm_beep(uint16_t freq_hz, uint16_t duration_ms)
{
    if (s_muted || !s_tone_queue) return;
    tone_cmd_t cmd = { .freq_hz = freq_hz, .duration_ms = duration_ms };
    xQueueSend(s_tone_queue, &cmd, 0);
}

void tracker_alarm_trigger_alert(void)
{
    if (s_muted) return;
    // 刺耳急促两声报警
    tracker_alarm_beep(2400, 120);
}

void tracker_alarm_tick_hotcold(int proximity_pct)
{
    if (s_muted) return;
    if (proximity_pct < 10) return;

    // 越靠近目标，音调越高 (1000Hz -> 3000Hz)，短促 30ms 滴答
    uint16_t freq = 1000 + (uint16_t)(proximity_pct * 20);
    tracker_alarm_beep(freq, 35);
}

bool tracker_alarm_toggle_mute(void)
{
    s_muted = !s_muted;
    return s_muted;
}

bool tracker_alarm_is_muted(void)
{
    return s_muted;
}

#else
// Host 模式
void tracker_alarm_init(void) {}
void tracker_alarm_beep(uint16_t freq_hz, uint16_t duration_ms) { (void)freq_hz; (void)duration_ms; }
void tracker_alarm_trigger_alert(void) {}
void tracker_alarm_tick_hotcold(int proximity_pct) { (void)proximity_pct; }
static bool s_host_muted = false;
bool tracker_alarm_toggle_mute(void) { s_host_muted = !s_host_muted; return s_host_muted; }
bool tracker_alarm_is_muted(void) { return s_host_muted; }
#endif
