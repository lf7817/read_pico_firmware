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

/// 源像素上限，按"哪条路径在解"分档，不是按文件类型拍脑袋：
/// 条带路径（基线 JPEG 走 TJpgDec、PNG 走 libpng 逐行）内存与源宽成正比、与源高无关，
/// 所以两者都用 16M 像素 / 单边 8192；只有整图兜底（渐进 JPEG 走 stb）需要一次装下全图，
/// 那一条才是 1M 像素。
/// Source ceilings split by which path decodes, not by file extension guesswork. The strip paths
/// (baseline JPEG through TJpgDec, PNG through libpng row-by-row) scale with source width only,
/// so both take 16M pixels and 8192 per side. Only the whole-image fallback (progressive JPEG via
/// stb) has to hold the entire raster, and that one stays at 1M pixels.
#define BOOK_IMAGE_MAX_SIDE 8192u
#define BOOK_IMAGE_MAX_PIXELS_STRIP (16u * 1024u * 1024u)
#define BOOK_IMAGE_MAX_PIXELS_OTHER (1024u * 1024u)

/// 只读文件头拿源尺寸，不分配内存，可在排版期调用。
/// Source dimensions from the header only; allocates nothing, safe during layout.
bool book_image_size(const uint8_t* data, size_t len, uint16_t* width, uint16_t* height);
/// 与 book_image_size 同样的头解析，额外回报隔行位；`interlaced` 可为 NULL。
/// The same header parse reporting the interlace flag; `interlaced` may be NULL.
bool book_image_size_ex(const uint8_t* data, size_t len, uint16_t* width, uint16_t* height,
                        bool* interlaced);
