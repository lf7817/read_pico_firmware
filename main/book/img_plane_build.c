/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：把解码条带、整数加权缩小、blue noise 抖动串成一次加载期的顺序装配。
 * English: Assembles decode strips, integer box downscaling and blue-noise dithering into one
 * load-time pass.
 *
 * 冻结：不申请第二份整图 8-bit 平面；抖动坐标用平面坐标，与条带边界无关。放在单独一个文件里是为了
 * 让 img_plane.c（存储与搬运）不牵扯解码器与 libpng。
 * Frozen: never allocates a second full 8-bit plane; the dither is keyed on plane coordinates so
 * strip boundaries cannot shift it. It lives in its own file so img_plane.c (storage and blitting)
 * stays free of the decoders and libpng.
 */
#include "img_plane.h"
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "img_dither.h"
#include "img_scale.h"

#define PSRAM_CAPS (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)

typedef struct {
    img_box_scaler_t scaler;
    img_plane_t* plane;
    uint8_t* box_out;      // 缩放后的 8-bit 行 / scaled 8-bit rows
    size_t box_out_bytes;
    int target_w, target_h;
    int rows;              // 已写进平面的行数 / rows already written into the plane
} build_t;

static bool collect_rows(void* ctx, const uint8_t* gray_rows, int row_count, int first_row, int width) {
    (void)first_row;
    build_t* b = (build_t*)ctx;
    if (width != b->scaler.src_width) return false;
    int emitted = img_box_push(&b->scaler, gray_rows, row_count, b->box_out, b->box_out_bytes);
    if (img_box_emitted_rows(&b->scaler) < b->rows + emitted) return false;
    for (int i = 0; i < emitted; ++i) {
        int y = b->rows + i;
        if (y >= b->target_h) return false;
        const uint8_t* src = b->box_out + (size_t)i * b->target_w;
        for (int x = 0; x < b->target_w; ++x)
            img_plane_set(b->plane, x, y, img_dither_gray_to_4bpp(src[x], x, y));
    }
    b->rows += emitted;
    return true;
}

bool img_plane_build(img_reader_t* r, img_format_t format, int src_w, int src_h, int max_w,
                     int max_h, img_plane_t* out) {
    if (out) { out->nib = NULL; out->width = out->height = 0; }
    if (!r || !out || src_w <= 0 || src_h <= 0 || max_w <= 0 || max_h <= 0) return false;
    int tw = 0, th = 0;
    img_plane_target(src_w, src_h, max_w, max_h, &tw, &th);
    // A0 实测 TJpgDec 只有 1/8 档真省时间，1/2 与 1/4 反而更慢，所以要么 1/1 要么 1/8。
    // A0 measured that only 1/8 actually saves decode time, so the choice is 1/1 or 1/8.
    int scale = format == IMG_FORMAT_JPEG && (src_w >> 3) >= tw && (src_h >> 3) >= th ? 3 : 0;
    int dec_w = src_w >> scale, dec_h = src_h >> scale;
    if (dec_w < tw || dec_h < th) return false;

    build_t b = {0};
    b.target_w = tw;
    b.target_h = th;
    size_t store_size = img_box_scaler_memory(tw);
    uint8_t* store = heap_caps_malloc(store_size, PSRAM_CAPS);
    uint8_t* strip = heap_caps_malloc((size_t)IMG_STRIP_ROWS * dec_w, PSRAM_CAPS);
    uint8_t* work = format == IMG_FORMAT_JPEG ? heap_caps_malloc(IMG_JPEG_WORK_MIN, PSRAM_CAPS) : NULL;
    b.plane = out;
    b.box_out_bytes = (size_t)(IMG_STRIP_ROWS + 1) * tw;
    b.box_out = heap_caps_malloc(b.box_out_bytes, PSRAM_CAPS);
    out->nib = heap_caps_malloc(img_plane_bytes(tw, th), PSRAM_CAPS);
    out->width = (uint16_t)tw;
    out->height = (uint16_t)th;
    bool ok = store && strip && b.box_out && out->nib && (format != IMG_FORMAT_JPEG || work) &&
        img_box_scaler_init(&b.scaler, store, store_size, dec_w, dec_h, tw, th);
    if (ok) {
        int gw = 0, gh = 0;
        if (format == IMG_FORMAT_JPEG) {
            ok = img_decode_jpeg_strips(r, work, IMG_JPEG_WORK_MIN, strip,
                                        (size_t)IMG_STRIP_ROWS * dec_w, (uint8_t)scale, collect_rows,
                                        &b, &gw, &gh);
        } else {
            ok = img_decode_png_rows(r, collect_rows, &b, &gw, &gh);
        }
        // 解码器报的源尺寸必须和调用方量到的一致，否则缩放器是按错的形状累加的。
        // The decoder must agree with the header the caller measured, or the scaler accumulated
        // against the wrong shape.
        ok = ok && gw == dec_w && gh == dec_h && b.rows == th && img_box_emitted_rows(&b.scaler) == th;
    }
    free(work);
    free(strip);
    free(b.box_out);
    free(store);
    if (!ok) img_plane_free(out);
    return ok;
}

bool img_plane_from_gray(const uint8_t* gray, int width, int height, int max_w, int max_h,
                         img_plane_t* out) {
    if (out) { out->nib = NULL; out->width = out->height = 0; }
    if (!gray || !out || width <= 0 || height <= 0 || max_w <= 0 || max_h <= 0) return false;
    int tw = 0, th = 0;
    img_plane_target(width, height, max_w, max_h, &tw, &th);
    size_t store_size = img_box_scaler_memory(tw);
    uint8_t* store = heap_caps_malloc(store_size, PSRAM_CAPS);
    img_box_scaler_t scaler;
    out->nib = heap_caps_malloc(img_plane_bytes(tw, th), PSRAM_CAPS);
    out->width = (uint16_t)tw;
    out->height = (uint16_t)th;
    uint8_t* rows = heap_caps_malloc((size_t)tw * th, PSRAM_CAPS);
    bool ok = store && rows && out->nib &&
              img_box_scaler_init(&scaler, store, store_size, width, height, tw, th) &&
              img_box_push(&scaler, gray, height, rows, (size_t)tw * th) == th;
    for (int y = 0; ok && y < th; ++y) for (int x = 0; x < tw; ++x)
        img_plane_set(out, x, y, img_dither_gray_to_4bpp(rows[(size_t)y * tw + x], x, y));
    free(rows);
    free(store);
    if (!ok) img_plane_free(out);
    return ok;
}
