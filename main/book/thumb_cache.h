/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：4bpp 缩略图磁盘缓存。键含图书路径、字节数、mtime、目标尺寸与抖动版本；任何一项变了都 miss。
 * English: On-disk 4bpp thumbnail cache. The key carries the book path, byte size, mtime, target
 * dimensions and dither version; a change in any of them misses.
 *
 * 冻结：缓存永远不是正确性来源——读写失败一律当 miss，正文与图片预览不受影响；只写内置分区。
 * Frozen: the cache is never a source of truth; every read or write failure is a miss and leaves
 * text and illustration preview unaffected, and only the internal partition is written.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "img_plane.h"

/// 默认缓存目录（内置分区）。/ Default cache directory, on the internal partition.
#define THUMB_CACHE_ROOT "/flash/.thumbs"
/// 缓存路径上限：根目录 + 文件名（含抖动版本与尺寸）。/ Cache path bound: root plus one name.
#define THUMB_CACHE_PATH_MAX 352

/// 换缓存根目录。设备侧不调用，宿主机测试用它注入 /tmp 路径。
/// Override the cache root. The device uses the default; host tests point it at a /tmp path.
void thumb_cache_set_root(const char* root);

/// 当前缓存根目录。/ The active cache root.
const char* thumb_cache_root(void);

/// 由图书条目算缓存文件路径。返回 false 表示放不下或参数不合法。
/// Derive the cache path for one book entry. False means bad arguments or an overflowed buffer.
bool thumb_cache_key(const char* book_path, uint32_t size, uint32_t mtime, int width, int height,
                     char* out, size_t out_size);

/// 读回平面。调用方负责 img_plane_free。任何不符（缺文件、截断、尺寸或版本不符）都是 false。
/// Load a plane; the caller frees it with img_plane_free. Anything off—absent, truncated, or a
/// dimension or version mismatch—returns false.
bool thumb_cache_load(const char* path, img_plane_t* out);

/// 原子写入：先写 .part，再删目标、改名。失败静默（缓存不是正确性来源）。
/// Atomic store: write .part, then remove the target and rename. Failures are silent by design.
bool thumb_cache_store(const char* path, const img_plane_t* plane);
