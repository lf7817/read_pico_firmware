/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：加载期生成 4bpp 灰度平面，渲染期只做搬运。平面尺寸是目标显示尺寸，不是源尺寸。
 * English: Builds a 4bpp gray plane while loading so rendering only copies it. The plane is sized
 * to the display target, never to the source.
 *
 * 冻结：不放大（目标任一边大于源就按源截断），失败时 out->nib 为 NULL 且不影响正文阅读；
 *       nibble 打包与 epdiy 同基准——偶数列走低 4 位，奇数列走高 4 位。
 * Frozen: never enlarges (a target larger than the source is clamped to it); on failure out->nib
 * stays NULL and text reading is unaffected. Nibble packing matches epdiy: even columns in the low
 * half byte, odd columns in the high half.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "img_decode.h"

/// 平面里一格就是一个 nibble。/ One plane cell is one nibble.
#define IMG_PLANE_LEVELS 16

/// 栅格格式由调用方给出：它已经从文件头量过尺寸，解码器因此不必再偷读一次头部。
/// The caller supplies the raster format: it has already measured the header, so the decoders
/// never need to peek at it a second time.
typedef enum { IMG_FORMAT_JPEG, IMG_FORMAT_PNG } img_format_t;

/// 4bpp 平面。nib 为 NULL 表示空平面；width/height 是目标显示尺寸。
/// A 4bpp plane. A NULL nib means empty; width/height are display-target dimensions.
typedef struct {
    uint8_t* nib;      ///< 2 像素/字节，偶数列低 4 位 / two pixels per byte, even columns in the low half
    uint16_t width;    ///< 目标宽 / target width
    uint16_t height;   ///< 目标高 / target height
} img_plane_t;

/// 源图等比内接到 max 盒子里的结果尺寸；只缩小不放大，任一边至少 1。
/// Aspect-fit of the source into the max box; shrink only, never enlarge, each side at least 1.
void img_plane_target(int src_w, int src_h, int max_w, int max_h, int* out_w, int* out_h);

/// 平面字节数（含奇数宽的半个字节）。/ Plane bytes, rounding up an odd width.
size_t img_plane_bytes(int width, int height);

/// 从 reader 建平面：解码条带 -> 整数加权缩小 -> blue noise 抖动 -> 打包。
/// 失败返回 false 并把 *out 清空；源尺寸必须由调用方按同一份文件头量出来。
/// Build a plane from a reader: decode strips, box-downscale, blue-noise dither, pack. Returns
/// false and clears *out on failure. src_w/src_h must come from the same file header.
bool img_plane_build(img_reader_t* r, img_format_t format, int src_w, int src_h, int max_w,
                     int max_h, img_plane_t* out);

/// 已经有一整块 8-bit 灰度平面时，走同一套缩放与抖动打包。兜底路径用（渐进 JPEG）。
/// Pack an 8-bit gray plane that already exists through the same scale and dither; the fallback
/// path uses it for progressive JPEG.
bool img_plane_from_gray(const uint8_t* gray, int width, int height, int max_w, int max_h,
                         img_plane_t* out);

/// 释放并清空。/ Free and clear.
void img_plane_free(img_plane_t* p);

/// 取一格（0..15）。越界返回白（15），让调用方的裁剪不必处处设防。
/// One cell (0..15). Out of range reads white so callers need not guard every clip.
uint8_t img_plane_get(const img_plane_t* p, int x, int y);
/// 写一格（0..15）。越界忽略。/ Write one cell; out of range is ignored.
void img_plane_set(img_plane_t* p, int x, int y, uint8_t code);

/// 把平面画进 4bpp framebuffer 的 (x0,y0) 起。逐点走 epd_draw_pixel，因为它取的是 color 的高 nibble。
/// Paint the plane into a 4bpp framebuffer at (x0,y0), pixel by pixel through epd_draw_pixel,
/// which reads the level from the high nibble of color.
void img_plane_blit(const img_plane_t* p, uint8_t* fb, int x0, int y0);
