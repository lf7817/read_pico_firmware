/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：有界 JPEG/PNG 解码与灰度缩放，调用方持有输出。
 * English: Bounded JPEG/PNG decoding and grayscale resizing; caller owns output.
 * 冻结：不访问文件和显示；失败交还空输出，不影响正文阅读。
 * Frozen: No file or display access; failure leaves empty output and must not block text reading.
 * 冻结：尺寸探测只认头，不校验整文件；坏头返回 false，不解码也不分配。
 * Frozen: size probing reads headers only; a bad header returns false without decoding or allocating.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/// 解码到不超过648×1000灰度图；输入至多2MiB；基线JPEG至多16M像素，其余至多1M像素、解码堆至多4MiB。
/// Decode at most 648x1000 grayscale pixels; input <=2MiB; baseline JPEG <=16M pixels, others <=1M pixels with a <=4MiB decoder heap.
bool book_image_decode(const uint8_t* data, size_t len, size_t budget,
                       uint8_t** pixels, uint16_t* width, uint16_t* height);

/// 源像素上限：基线 JPEG 16M，其余 1M；单边不超过 8192。这些是文档化的插图范围，
/// 两条解码入口（book_image_decode 与 img_plane_build 的调用方）共用同一组数字。
/// Source pixel ceilings: 16M for baseline JPEG, 1M otherwise, 8192 per side. These are the
/// documented illustration limits, shared by both decode entries.
#define BOOK_IMAGE_MAX_SIDE 8192u
#define BOOK_IMAGE_MAX_PIXELS_JPEG (16u * 1024u * 1024u)
#define BOOK_IMAGE_MAX_PIXELS_OTHER (1024u * 1024u)

/// 只读文件头拿源尺寸，不分配内存，可在排版期调用。
/// Source dimensions from the header only; allocates nothing, safe during layout.
bool book_image_size(const uint8_t* data, size_t len, uint16_t* width, uint16_t* height);
/// 与 book_image_size 同样的头解析，额外回报隔行位；`interlaced` 可为 NULL。
/// The same header parse reporting the interlace flag; `interlaced` may be NULL.
bool book_image_size_ex(const uint8_t* data, size_t len, uint16_t* width, uint16_t* height,
                        bool* interlaced);
