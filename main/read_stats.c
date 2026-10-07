/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：read_stats.h 的实现。文件是小端定长记录，读写都逐字段序列化，不直接
 * fwrite 结构体，避免对齐与编译器差异把格式钉死在主机上。
 *
 * English: Implementation of read_stats.h. The file is little-endian fixed-size
 * records, serialised field by field so struct padding cannot leak into it.
 */

#include "read_stats.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app_clock.h"
#include "book_store.h"
#include "esp_log.h"

#define TAG "read_stats"

#define STATS_PATH "/flash/read_stats.bin"
#define STATS_PART "/flash/read_stats.part"
#define STATS_MAGIC 0x52505331u   /* RPS1 */
#define STATS_VERSION 1
#define STATS_DAYS 366
// 天序号从 2024-01-01 起算，u16 还能用一百多年。
// Day index counts from 2024-01-01; u16 lasts another century.
#define STATS_DAY_ZERO 1704067200
// 单次翻页间隔封顶 5 分钟：超过就当人不在读。
// Cap one turn gap at 5 minutes; beyond that the reader is away.
#define STATS_GAP_CAP_MS 300000

static uint16_t s_day[STATS_DAYS];
static uint32_t s_sec[STATS_DAYS];
static int s_count;
static uint32_t s_total;
static int64_t s_last_turn_ms;
static int64_t s_pending_ms;
static bool s_loaded;

static uint32_t day_index(void) {
    return (uint32_t)((time(NULL) - STATS_DAY_ZERO) / 86400);
}

static int find_bucket(uint16_t day) {
    for (int i = 0; i < s_count; ++i) if (s_day[i] == day) return i;
    return -1;
}

static void add_seconds(uint16_t day, uint32_t sec) {
    const int i = find_bucket(day);
    if (i >= 0) {
        s_sec[i] += sec;
        return;
    }
    if (s_count == STATS_DAYS) {
        // 满了丢最旧的一天，保持按天升序，连续天数计算才简单。
        // Drop the oldest day to keep the array ascending and the streak logic cheap.
        memmove(s_day, s_day + 1, (STATS_DAYS - 1) * sizeof(s_day[0]));
        memmove(s_sec, s_sec + 1, (STATS_DAYS - 1) * sizeof(s_sec[0]));
        s_count = STATS_DAYS - 1;
    }
    s_day[s_count] = day;
    s_sec[s_count] = sec;
    ++s_count;
}

static void put16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)(v >> 24);
}

static uint16_t get16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static uint32_t get32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void load_stats(void) {
    FILE* f = fopen(STATS_PATH, "rb");
    if (f == NULL) return;
    uint8_t hdr[12];
    const size_t got = fread(hdr, 1, sizeof(hdr), f);
    if (got != sizeof(hdr) || get32(hdr) != STATS_MAGIC || get16(hdr + 4) != STATS_VERSION) {
        fclose(f);
        return;
    }
    const uint32_t total = get32(hdr + 6);
    const uint32_t count = get16(hdr + 10);
    if (count > STATS_DAYS) {
        fclose(f);
        return;
    }
    uint8_t rec[6];
    int n = 0;
    uint16_t days[STATS_DAYS];
    uint32_t secs[STATS_DAYS];
    while (n < (int)count && fread(rec, 1, sizeof(rec), f) == sizeof(rec)) {
        days[n] = get16(rec);
        secs[n] = get32(rec + 2);
        ++n;
    }
    fclose(f);
    if (n != (int)count) return;
    memcpy(s_day, days, sizeof(days));
    memcpy(s_sec, secs, sizeof(secs));
    s_count = n;
    s_total = total;
    ESP_LOGI(TAG, "loaded %d days, total %lu s", n, (unsigned long)total);
}

static bool store_stats(void) {
    uint8_t buf[12 + STATS_DAYS * 6];
    size_t o = 0;
    put32(buf + o, STATS_MAGIC); o += 4;
    put16(buf + o, STATS_VERSION); o += 2;
    put32(buf + o, s_total); o += 4;
    put16(buf + o, (uint16_t)s_count); o += 2;
    for (int i = 0; i < s_count; ++i) {
        put16(buf + o, s_day[i]); o += 2;
        put32(buf + o, s_sec[i]); o += 4;
    }
    FILE* f = fopen(STATS_PART, "wb");
    if (f == NULL) {
        ESP_LOGW(TAG, "open %s failed", STATS_PART);
        return false;
    }
    const bool wrote = fwrite(buf, 1, o, f) == o;
    fclose(f);
    if (!wrote) {
        remove(STATS_PART);
        ESP_LOGW(TAG, "write failed");
        return false;
    }
    // FAT 上 rename 不覆盖同名文件，先删目标再改名。
    // FAT rename does not replace an existing name, so unlink the target first.
    remove(STATS_PATH);
    if (rename(STATS_PART, STATS_PATH) != 0) {
        ESP_LOGW(TAG, "rename failed");
        return false;
    }
    return true;
}

void read_stats_init(void) {
    if (s_loaded) return;
    s_loaded = true;
    book_store_root_t roots[2];
    int n = 0;
    // 只为了让内置分区挂上；结果由后面的 fopen 说话。
    // Only to mount the internal partition; the fopen below reports the truth.
    book_store_roots(roots, &n);
    load_stats();
}

void read_stats_note_turn(int64_t now_ms) {
    if (s_last_turn_ms > 0 && now_ms > s_last_turn_ms) {
        int64_t gap = now_ms - s_last_turn_ms;
        if (gap > STATS_GAP_CAP_MS) gap = STATS_GAP_CAP_MS;
        s_pending_ms += gap;
    }
    s_last_turn_ms = now_ms;
}

void read_stats_flush(int64_t now_ms) {
    (void)now_ms;
    if (s_pending_ms <= 0) return;
    if (!app_clock_valid()) {
        // 没校时就不写日桶：留着待下一次，也不清累计量。
        // Unsynced: keep the pending seconds instead of filing them under a fake day.
        return;
    }
    const uint32_t sec = (uint32_t)((s_pending_ms + 500) / 1000);
    s_pending_ms = 0;
    if (sec == 0) return;
    add_seconds((uint16_t)day_index(), sec);
    s_total += sec;
    if (!store_stats()) {
        s_pending_ms = (int64_t)sec * 1000;
        return;
    }
    const int i = find_bucket((uint16_t)day_index());
    ESP_LOGI(
        TAG, "flushed +%lu s, today %lu s, days %d, total %lu s",
        (unsigned long)sec, (unsigned long)(i >= 0 ? s_sec[i] : 0), s_count,
        (unsigned long)s_total
    );
}

bool read_stats_format(char* out, size_t size) {
    if (out == NULL || size == 0) return false;
    if (!app_clock_valid()) {
        snprintf(out, size, "未校时，暂停日统计");
        return true;
    }
    const uint32_t today = day_index();
    const int i = find_bucket((uint16_t)today);
    const uint32_t today_sec = i >= 0 ? s_sec[i] : 0;
    // 今天还没读就从昨天往前数，免得刚开机就显示「连续 0 天」。
    // Count back from yesterday when today is empty so the streak does not read 0.
    uint32_t cursor = today;
    if (today_sec == 0 && cursor > 0) --cursor;
    int streak = 0;
    for (;;) {
        const int k = find_bucket((uint16_t)cursor);
        if (k < 0 || s_sec[k] == 0) break;
        ++streak;
        if (cursor == 0) break;
        --cursor;
    }
    if (today_sec == 0 && streak == 0 && s_total == 0) return false;
    snprintf(out, size, "今日 %u 分 · 连续 %d 天", (unsigned)(today_sec / 60), streak);
    return true;
}
