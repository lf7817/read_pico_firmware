/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：app_clock.h 的实现。esp_sntp 轮询一次池地址；同步完成即停，不常驻重试。
 *
 * English: Implementation of app_clock.h. esp_sntp polls the pool once; it stops
 * after a completed sync instead of retrying forever.
 */

#include "app_clock.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "esp_log.h"
#include "esp_sntp.h"

#define TAG "app_clock"

// 早于这个 unix 秒视为没校时（2024-01-01 00:00 UTC）。
// Anything before this unix second counts as unsynced (2024-01-01 00:00 UTC).
#define APP_CLOCK_MIN_VALID ((time_t)1704067200)

static bool s_started;
// 自己记同步标志：esp_sntp_stop() 会把 lwip 的 sync status 复位，
// 而回调里我们正是要停掉轮询，所以不能再拿那个状态位当判据。
// Own sync flag: esp_sntp_stop() resets the lwip sync status, and the callback
// stops polling on purpose, so the status bit cannot be the validity test.
static bool s_synced;

static void on_synced(struct timeval* tv) {
    (void)tv;
    s_synced = true;
    esp_sntp_stop();
    ESP_LOGI(TAG, "synced, epoch %ld", (long)time(NULL));
}

void app_clock_start(void) {
    if (s_started) return;
    s_started = true;
    // 固定 UTC+8：产品文案以中日英为主但时区选择是另一个决策，见文件头冻结段。
    // Fixed UTC+8: a timezone picker is a separate decision, see the frozen block.
    setenv("TZ", "CST-8", 1);
    tzset();
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(on_synced);
    esp_sntp_init();
    ESP_LOGI(TAG, "sntp started");
}

bool app_clock_valid(void) {
    if (!s_started || !s_synced) return false;
    return time(NULL) > APP_CLOCK_MIN_VALID;
}

bool app_clock_hm(char* out, size_t size) {
    if (!app_clock_valid() || !out || size < 6) return false;
    struct tm tm;
    const time_t now = time(NULL);
    localtime_r(&now, &tm);
    snprintf(out, size, "%02d:%02d", tm.tm_hour, tm.tm_min);
    return true;
}

bool app_clock_md(char* out, size_t size) {
    if (!app_clock_valid() || !out || size < 10) return false;
    static const char* const weekday[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    struct tm tm;
    const time_t now = time(NULL);
    localtime_r(&now, &tm);
    snprintf(out, size, "%02d-%02d %s", tm.tm_mon + 1, tm.tm_mday, weekday[tm.tm_wday % 7]);
    return true;
}
