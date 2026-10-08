/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：八位灰度到四 bpp 的 blue noise 抖动，位置确定的纯函数。
 * English: Blue-noise dithering from 8-bit gray to 4bpp; a position-deterministic pure function.
 *
 * 冻结：不得依赖处理顺序，也不得用误差扩散——差分刷新会重画局部，结果必须逐字节可复现。
 * Frozen: no processing-order dependence and no error diffusion; differential updates repaint
 * regions, so the result must be byte-reproducible.
 */
#pragma once
#include <stdint.h>

/// 抖动算法版本，进缩略图缓存键；换阈值表或算法要 +1。/ Dither version, part of the thumbnail cache key.
#define IMG_DITHER_VERSION 1
/// 真机屏摄定的可分辨档数。/ The number of panel-distinct steps signed off from screen capture.
#define IMG_DITHER_LEVELS 6
/// 4bpp 码（0 = 黑、15 = 白），升序。/ 4bpp codes (0 = black, 15 = white), ascending.
extern const uint8_t img_dither_levels[IMG_DITHER_LEVELS];

/// 单像素：返回一个 0..15 的 nibble。x/y 是目标屏幕坐标，不是源坐标。
/// Per pixel: returns a 0..15 nibble. x/y are destination coordinates, not source ones.
uint8_t img_dither_gray_to_4bpp(uint8_t gray, int x, int y);
/// 一行：out[i] 是目标坐标 (x_start+i, y) 的 nibble。/ One row of destination nibbles.
void img_dither_row_to_4bpp(const uint8_t* gray, int x_start, int y, int count, uint8_t* out);
