/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：单实例 TXT/EPUB 书源与章节进度接口。
 * English: Singleton TXT/EPUB source and chapter progress interface.
 * 冻结：正文由调用方释放；不依赖 UI。
 * Frozen: The caller frees loaded text; no UI dependencies.
 */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "html_text.h"
#include "img_plane.h"
typedef enum {
    BOOK_KIND_TXT, ///< 文本 / Plain text
    BOOK_KIND_EPUB, ///< EPUB 容器 / EPUB container
} book_kind_t;
/// 打开并替换当前书源。/ Open and replace the current source.
esp_err_t book_open(const char *path);
/// 关闭书源并释放目录。/ Close the source and free its index.
void book_close(void);
/// 返回章节数。/ Return the chapter count.
size_t book_chapter_count(void);
/// 复制 UTF-8 标题，空间不足返回错误。/ Copy a UTF-8 title, failing if capacity is insufficient.
esp_err_t book_chapter_title(size_t i, char *buf, size_t cap);
/// 加载 NUL 结尾的 PSRAM UTF-8；调用方 free。/ Load NUL-terminated PSRAM UTF-8; caller frees it.
esp_err_t book_chapter_load(size_t i, char **utf8, size_t *len);
/// 保留 EPUB 标题/段落及图片块；TXT 的 blocks 为 NULL，调用方 html_text_free。
/// Preserve EPUB heading/paragraph and image blocks; TXT has NULL blocks; caller uses html_text_free.
esp_err_t book_chapter_load_blocks(size_t i, html_text_t *out);
/// 按需把指定章节的单幅本地图片解成不超过 max 的 4bpp 平面；失败平面为空，TXT 不支持。
/// Decode one local chapter image on demand into a 4bpp plane no larger than max; a failure leaves
/// an empty plane. Unsupported for TXT.
esp_err_t book_chapter_load_image(size_t i, const char *reference, img_plane_t *out, int max_width,
                                  int max_height);
/// 分页之前只读每张图的文件头，把源尺寸写回块里，让分页能按真实比例留位。
/// 只 inflate 不解码，返回拿到尺寸的图数。TXT 没有图片，返回 0。
/// Read only each image's header before pagination so page breaks reserve the real aspect. This
/// inflates without decoding, and returns how many sizes were read. TXT has no images and is 0.
size_t book_chapter_probe_images(size_t chapter, html_text_t* text);
/// 把偏移落在 [from,to) 的图解成不超过 max 的 4bpp 平面，最多用 budget 字节。
/// 返回已占用的平面字节数。已经解好的图直接计入预算，不重复解码。
/// Decode images whose offset falls in [from,to) into planes no larger than max, up to a byte
/// budget, and return the plane bytes used. Already-loaded images count towards the budget and are
/// not decoded twice.
size_t book_chapter_prepare_images(size_t chapter, html_text_t* text, size_t from, size_t to,
                                   int max_width, int max_height, size_t budget);
/// TXT 为源文件字节；EPUB 为 spine 原始 HTML 未压缩累计字节，不是 ZIP 文件大小。
/// TXT uses source-file bytes; EPUB uses cumulative uncompressed spine HTML bytes, not ZIP file size.
uint32_t book_total_bytes(void);
/// TXT 为原文件章起点；EPUB 为前序 spine 原始 HTML 字节总和。
/// TXT uses original chapter offsets; EPUB uses the sum of preceding spine source HTML bytes.
uint32_t book_chapter_byte_offset(size_t i);
/// 当前类型；关闭时默认 TXT。/ Current kind, defaulting to TXT when closed.
book_kind_t book_kind(void);
