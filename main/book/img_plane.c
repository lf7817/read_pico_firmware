/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：把解码条带、整数加权缩小、blue noise 抖动串成一次加载期的顺序装配。
 * English: Assembles decode strips, integer box downscaling and blue-noise dithering into one
 * load-time pass.
 *
 * 冻结：全程不申请第二份整图 8-bit 平面；抖动坐标用目标屏幕坐标，与条带边界无关。
 * Frozen: never allocates a second full 8-bit plane; the dither is keyed on target coordinates so
 * strip boundaries cannot shift it.
 */
#include "img_plane.h"
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "epdiy.h"
#include "img_dither.h"
#include "img_scale.h"

#define PSRAM_CAPS (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)

void img_plane_target(int src_w, int src_h, int max_w, int max_h, int* out_w, int* out_h) {
    int w = src_w, h = src_h;
    if (w > max_w) { h = (int)((int64_t)h * max_w / w); w = max_w; }
    if (h > max_h) { w = (int)((int64_t)w * max_h / h); h = max_h; }
    *out_w = w < 1 ? 1 : w;
    *out_h = h < 1 ? 1 : h;
}

size_t img_plane_bytes(int width, int height) {
    if (width <= 0 || height <= 0) return 0;
    return ((size_t)width + 1) / 2 * (size_t)height;
}

uint8_t img_plane_get(const img_plane_t* p, int x, int y) {
    if (!p || !p->nib || x < 0 || y < 0 || x >= p->width || y >= p->height) return 15;
    uint8_t byte = p->nib[(size_t)y * ((p->width + 1) / 2) + x / 2];
    return (uint8_t)((x & 1) ? (byte >> 4) : (byte & 0x0F));
}

void img_plane_set(img_plane_t* p, int x, int y, uint8_t code) {
    if (!p || !p->nib || x < 0 || y < 0 || x >= p->width || y >= p->height) return;
    uint8_t* byte = &p->nib[(size_t)y * ((p->width + 1) / 2) + x / 2];
    code &= 0x0F;
    *byte = (uint8_t)((x & 1) ? ((*byte & 0x0F) | (code << 4)) : ((*byte & 0xF0) | code));
}

void img_plane_free(img_plane_t* p) {
    if (!p) return;
    free(p->nib);
    p->nib = NULL;
    p->width = p->height = 0;
}

void img_plane_blit(const img_plane_t* p, uint8_t* fb, int x0, int y0, int width, int height) {
    if (!p || !p->nib || !fb) return;
    if (width > p->width) width = p->width;
    if (height > p->height) height = p->height;
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x)
        epd_draw_pixel(x0 + x, y0 + y, (uint8_t)(img_plane_get(p, x, y) << 4), fb);
}
