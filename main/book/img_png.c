/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：libpng 的逐行解码桥：内存与读流都归我们管，输出 8-bit Y 条带。
 * English: libpng row-decode bridge: memory and the input stream are ours, output is 8-bit Y strips.
 *
 * 冻结：libpng 不得用默认 malloc——`espressif/libpng` 组件没有 PSRAM 开关，装桥失败就等于吃内部 RAM。
 *       隔行（Adam7）图一律拒绝，不解也不猜。
 * Frozen: libpng must never use the default malloc — the espressif/libpng component ships no PSRAM
 * switch, so skipping the bridge silently spends internal RAM. Interlaced (Adam7) images are
 * rejected outright, never decoded and never guessed at.
 */
#include "img_decode.h"
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "png.h"

#define TAG "img_png"
#define PNG_MAX_SIDE 4096u

#define PSRAM_CAPS (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)

// 桥在读结构建好之后立刻装上：之后的每一块缓冲都落在 PSRAM。结构本身（png_struct 与 png_info）
// 仍由默认分配器给出，那是 A0 量到的固定量，不是随行数增长的量。
// The bridge goes on as soon as the read struct exists, so every buffer after that lands in PSRAM.
// The struct and info themselves still come from the default allocator: that is the fixed amount A0
// measured, not something that grows with the row count.
static void* pool_alloc(png_structp png, png_alloc_size_t n) {
    void* p = heap_caps_malloc((size_t)n, PSRAM_CAPS);
    if (!p) png_error(png, "psram exhausted");
    return p;
}

static void pool_free(png_structp png, void* p) { (void)png; free(p); }

static void png_read_fn(png_structp png, png_bytep dst, png_size_t want) {
    img_reader_t* r = (img_reader_t*)png_get_io_ptr(png);
    if (r->read(r->ctx, dst, want) != want) png_error(png, "short read");
}

bool img_decode_png_rows(img_reader_t* r, img_strip_cb cb, void* cb_ctx, int* out_width,
                         int* out_height) {
    if (!r || !r->read || !cb || !out_width || !out_height) return false;
    *out_width = *out_height = 0;
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) return false;
    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_read_struct(&png, NULL, NULL); return false; }
    bool ok = false;
    png_bytep row = NULL;
    uint8_t* strip = NULL;
    if (setjmp(png_jmpbuf(png))) goto done;           // libpng 的所有失败出口 / every libpng failure lands here
    png_set_mem_fn(png, NULL, pool_alloc, pool_free);
    png_set_read_fn(png, r, png_read_fn);
    png_set_user_limits(png, PNG_MAX_SIDE, PNG_MAX_SIDE);
    png_read_info(png, info);
    png_uint_32 width = 0, height = 0;
    int depth = 0, color = 0, interlace = 0;
    // png_get_IHDR 成功时返回块位掩码、失败时返回 0；libpng 1.6 没有公开的 IHDR 常量可比。
    // png_get_IHDR returns a chunk mask on success and 0 on failure; 1.6 exposes no IHDR constant.
    if (!png_get_IHDR(png, info, &width, &height, &depth, &color, &interlace, NULL, NULL)) goto done;
    if (!width || !height || width > PNG_MAX_SIDE || height > PNG_MAX_SIDE) goto done;
    if (interlace != PNG_INTERLACE_NONE) {
        // Adam7 的行长逐 pass 变化，条带接口拿不到"第 n 行的宽度"，所以直接拒。
        // Adam7 row bytes change per pass, which the strip contract cannot express, so refuse.
        ESP_LOGW(TAG, "interlaced png rejected (%ux%u)", (unsigned)width, (unsigned)height);
        goto done;
    }
    if (depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (depth == 16) png_set_strip_16(png);
    if (color == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color == PNG_COLOR_TYPE_PALETTE || color == PNG_COLOR_TYPE_GRAY) png_set_tRNS_to_alpha(png);
    // 19595/38469 是 16.16 定点的 Rec.601 系数，避免在这里引入浮点。
    // 19595/38469 are Rec.601 in 16.16 fixed point, keeping floating point out of this path.
    if (color & PNG_COLOR_MASK_COLOR) png_set_rgb_to_gray_fixed(png, PNG_ERROR_ACTION_NONE, 19595, 38469);
    png_set_add_alpha(png, 0xFF, PNG_FILLER_AFTER);
    png_read_update_info(png, info);
    // 下面按"每像素灰度 + alpha 两字节"取值，变换结果必须正好是这个形状。
    // The loop below reads two bytes per pixel, so the transform result must be exactly that shape.
    if (png_get_color_type(png, info) != PNG_COLOR_TYPE_GRAY_ALPHA ||
        png_get_bit_depth(png, info) != 8 || png_get_rowbytes(png, info) < (png_size_t)width * 2) {
        ESP_LOGW(TAG, "png shape %d/%d not gray+alpha 8", png_get_color_type(png, info),
                 png_get_bit_depth(png, info));
        goto done;
    }
    row = png_malloc(png, png_get_rowbytes(png, info));
    strip = png_malloc(png, (size_t)width * IMG_STRIP_ROWS);
    if (!row || !strip) goto done;
    int filled = 0, first_row = 0;
    for (png_uint_32 y = 0; y < height; ++y) {
        png_read_row(png, row, NULL);
        // 透明像素合成到白底，与出货路径同一式子，免得两条路径各挑一个白点。
        // Composite transparent pixels onto white with the same expression the shipped path uses,
        // so the two paths cannot disagree about what "white" means.
        uint8_t* out = strip + (size_t)filled * width;
        for (png_uint_32 x = 0; x < width; ++x) {
            unsigned gray = row[(size_t)x * 2];
            unsigned alpha = row[(size_t)x * 2 + 1];
            out[x] = (uint8_t)((gray * alpha + 255u * (255u - alpha) + 127u) / 255u);
        }
        ++filled;
        if (filled < IMG_STRIP_ROWS && y + 1 < height) continue;
        if (!cb(cb_ctx, strip, filled, first_row, (int)width)) goto done;
        first_row += filled;
        filled = 0;
    }
    *out_width = (int)width;
    *out_height = (int)height;
    ok = true;

done:
    if (row) png_free(png, row);
    if (strip) png_free(png, strip);
    png_destroy_read_struct(&png, &info, NULL);
    if (!ok) { *out_width = *out_height = 0; }
    return ok;
}
