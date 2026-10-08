/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：TJpgDec 的薄包装；块回调把每块的 Y 行拼进条带缓冲，攒满一整条就交出去。
 * English: Thin TJpgDec wrapper; the block callback stitches per-MCU Y rows into the strip buffer
 * and hands the strip over once it is full.
 *
 * 冻结：解码期零分配；条带缓冲溢出或块顺序对不上都必须中止，而不是静默写歪。
 * Frozen: zero allocation while decoding; a full strip buffer or an out-of-order block aborts
 * instead of silently writing crooked.
 */
#include "img_decode.h"
#include <string.h>
#include "esp_log.h"
#include "vendor/tjpgd.h"

#define TAG "img_decode"

static size_t mem_read(void* ctx, uint8_t* dst, size_t want) {
    img_reader_t* r = (img_reader_t*)ctx;
    size_t n = r->mem_len - r->mem_at;
    if (n > want) n = want;
    if (dst && n) memcpy(dst, r->mem_data + r->mem_at, n);
    r->mem_at += n;
    return n;
}

bool img_reader_from_memory(img_reader_t* r, const uint8_t* data, size_t len) {
    if (!r || !data || !len) return false;
    r->read = mem_read;
    r->ctx = r;
    r->mem_data = data;
    r->mem_len = len;
    r->mem_at = 0;
    return true;
}

typedef struct {
    img_reader_t* reader;
    uint8_t* strip;
    size_t strip_size;
    int width;       // 按 scale 缩小后的行宽 / row width after scale
    int filled;      // 条带里已攒的行数 / rows currently buffered
    int first_row;   // 条带第一行的全局行号 / global row index of strip[0]
    img_strip_cb cb;
    void* cb_ctx;
    bool stopped;
} strip_out_t;

static size_t tj_in(JDEC* jd, uint8_t* dst, size_t want) {
    strip_out_t* out = (strip_out_t*)jd->device;
    return out->reader->read(out->reader->ctx, dst, want);
}

static bool strip_flush(strip_out_t* out) {
    if (!out->filled) return true;
    bool keep = out->cb(out->cb_ctx, out->strip, out->filled, out->first_row, out->width);
    out->first_row += out->filled;
    out->filled = 0;
    if (!keep) out->stopped = true;
    return keep;
}

static int tj_out(JDEC* jd, void* bitmap, JRECT* rect) {
    strip_out_t* out = (strip_out_t*)jd->device;
    if (out->stopped) return 0;
    const uint8_t* src = (const uint8_t*)bitmap;
    unsigned wide = (unsigned)(rect->right - rect->left + 1);
    unsigned tall = (unsigned)(rect->bottom - rect->top + 1);
    // 每行第一个块必须正好接在已攒行后面；对不上就是顺序假设被破坏，宁可不写。
    // The first block of a row must land right after the buffered rows; otherwise the ordering
    // assumption is broken, so abort rather than write it somewhere plausible.
    if (rect->left == 0) {
        if ((unsigned)(out->first_row + out->filled) != (unsigned)rect->top) {
            ESP_LOGE(TAG, "strip desync: block top %u, expected %d", (unsigned)rect->top,
                     out->first_row + out->filled);
            out->stopped = true;
            return 0;
        }
        // 条带只能在整行 MCU 收尾时交出去：一块只覆盖 16 列，此时这一行的其余块还没写进来。
        // A strip may only be delivered at the end of a full MCU row: one block covers 16 columns,
        // and flushing here would strand every block to its right behind the new base row.
        if (out->filled >= IMG_STRIP_ROWS && !strip_flush(out)) return 0;
    }
    // 目标行由块的绝对坐标决定，不能跟着块内循环走：一块 16 行高，落在条带里的第 0..15 行。
    // The destination row comes from the block's absolute rectangle. A block is up to 16 rows tall
    // and lands on rows 0..15 of the strip; keying it on the buffered row count writes row 0 sixteen
    // times and leaves the rest of the strip untouched.
    for (unsigned y = 0; y < tall; ++y) {
        int row = (int)(rect->top + y);
        if (row < out->first_row) continue;
        if (row - out->first_row >= IMG_STRIP_ROWS) {
            ESP_LOGE(TAG, "strip overruns %d rows at %d", IMG_STRIP_ROWS, row);
            out->stopped = true;
            return 0;
        }
        int rel = row - out->first_row;
        size_t at = (size_t)rel * out->width + (size_t)rect->left;
        if (at + wide > out->strip_size) {
            ESP_LOGE(TAG, "strip overflow at row %d (width %d)", row, out->width);
            out->stopped = true;
            return 0;
        }
        memcpy(out->strip + at, src + (size_t)y * wide, wide);
        if (rel + 1 > out->filled) out->filled = rel + 1;
    }
    return 1;
}

bool img_decode_jpeg_strips(img_reader_t* r, uint8_t* work, size_t work_size,
                            uint8_t* strip_buffer, size_t strip_buffer_size, uint8_t scale,
                            img_strip_cb cb, void* cb_ctx, int* out_width, int* out_height) {
    if (!r || !r->read || !work || !strip_buffer || !cb || !out_width || !out_height) return false;
    if (work_size < IMG_JPEG_WORK_MIN) {
        ESP_LOGW(TAG, "work area %u B below the %d B floor", (unsigned)work_size, IMG_JPEG_WORK_MIN);
        return false;
    }
    if (scale > 3) return false;
    JDEC jd;
    strip_out_t out = {0};
    out.reader = r;
    out.strip = strip_buffer;
    out.strip_size = strip_buffer_size;
    out.cb = cb;
    out.cb_ctx = cb_ctx;
    if (jd_prepare(&jd, tj_in, work, work_size, &out) != JDR_OK) return false;
    int width = (int)jd.width >> scale, height = (int)jd.height >> scale;
    if (!width || (size_t)width * IMG_STRIP_ROWS > strip_buffer_size) {
        ESP_LOGW(TAG, "strip buffer too small for %d-wide rows", width);
        return false;
    }
    out.width = width;
    *out_width = width;
    *out_height = height;
    if (jd_decomp(&jd, tj_out, scale) != JDR_OK || out.stopped) return false;
    return strip_flush(&out);
}
