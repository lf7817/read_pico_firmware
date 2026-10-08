/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：流式解码入口。输入是 reader 回调，输出是按条带交付的 8-bit Y 行。
 * English: Streaming decode entry. Input is a reader callback; output is 8-bit Y rows in strips.
 *
 * 冻结：不碰文件、不碰显示、不缩放、不抖动、不申请内存；工作缓冲与条带缓冲都由调用方给。
 * Frozen: no file or display access, no scaling, no dithering, no allocation; the caller owns both
 * the decode work area and the strip buffer.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/// TJpgDec 工作区下限：A0 实测灰度图 3,584 B，彩色 4:2:0 要两张量化表与四张霍夫曼表，
/// 宿主机实测 400x640 彩图在 4,096 B 下可解。放大到 8 KiB 档留余量。
/// TJpgDec work-area floor: A0 measured 3,584 B for gray; a color 4:2:0 scan needs two quantizer
/// and four Huffman tables, and 4,096 B decodes a 400x640 color fixture on the host. Rounded up.
#define IMG_JPEG_WORK_MIN 4096
/// 一条条带的最大行数（16 = 一个 MCU 行）。/ Rows per strip (16 = one MCU row).
#define IMG_STRIP_ROWS 16

/// 顺序读回调：返回实际读到的字节数，0 表示流结束。A 阶段只有内存切片实现，
/// 子项目 C 会换成 ZIP 流式 inflate 而不动这里任何类型。
/// Sequential read returning bytes consumed, 0 at end of stream. A ships the memory slice only;
/// subproject C swaps in streaming ZIP inflate without touching these types.
typedef struct {
    size_t (*read)(void* ctx, uint8_t* dst, size_t want);
    void* ctx;
    /* 内存切片的内联状态：放在这里是为了不为 reader 再申请一次内存。
       Inline memory-slice state, kept here so a reader never allocates. */
    const uint8_t* mem_data;
    size_t mem_len;
    size_t mem_at;
} img_reader_t;

/// 用一段已在内存里的数据填好 reader；data 必须在解码期间保持有效。
/// Point a reader at an in-memory buffer that must outlive the decode.
bool img_reader_from_memory(img_reader_t* r, const uint8_t* data, size_t len);

/// 条带回调：gray_rows 是 width 列、row_count 行的 8-bit Y；first_row 是全局行号。
/// 返回 false 表示调用方不再要数据，解码以 JDR_INTR 中止，本模块也返回 false。
/// Strip callback: row_count rows of width 8-bit Y columns at global row first_row. Returning
/// false means the caller is done; the decode aborts with JDR_INTR and this module returns false.
typedef bool (*img_strip_cb)(void* ctx, const uint8_t* gray_rows, int row_count, int first_row, int width);

/// 解 JPEG 为 Y 条带；scale 0..3 = 1/1..1/8，超出直接拒。A0 实测只有 1/8 真省时间，
/// 1/2 与 1/4 反而更慢。strip_buffer 至少容纳 IMG_STRIP_ROWS * 缩小后的行宽。
/// Decode a JPEG into Y strips; scale 0..3 = 1/1..1/8 and anything else is rejected. A0 measured
/// that only 1/8 actually saves time. strip_buffer must hold IMG_STRIP_ROWS * the scaled width.
bool img_decode_jpeg_strips(img_reader_t* r, uint8_t* work, size_t work_size,
                            uint8_t* strip_buffer, size_t strip_buffer_size, uint8_t scale,
                            img_strip_cb cb, void* cb_ctx, int* out_width, int* out_height);

/// 解 PNG 为 Y 条带（libpng 实现在 img_png.c）。隔行图返回 false；不缩放，缩小由调用方做。
/// Decode a PNG into Y strips (implemented in img_png.c). Interlaced images return false; no
/// scaling happens here, the caller downsamples.
bool img_decode_png_rows(img_reader_t* r, img_strip_cb cb, void* cb_ctx, int* out_width,
                         int* out_height);
