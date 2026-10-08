/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：大比例缩小的分离式加权 box（面积平均），按条带喂入，跨条带累加。
 * English: Separable weighted box (area average) for large downscaling; fed by strips and
 * accumulating across strip boundaries.
 *
 * 冻结：只做缩小；放大一律拒。双线性在 8x 比例会丢掉约 7/8 的源像素，半调网点和细字会断。
 * Frozen: downscale only; upscaling is rejected outright — bilinear drops ~7/8 of the source
 * pixels at an 8x ratio, which breaks halftone dots and hairlines.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/// 状态本身放得进栈（几十字节），像素存储仍由调用方给，先问 img_box_scaler_memory。
/// The handle is small enough to live on the stack; the pixel storage stays with the caller,
/// sized by img_box_scaler_memory.
typedef struct img_box_scaler {
    int src_width, src_height;      ///< 源尺寸 / source size
    int dst_width, dst_height;      ///< 目标尺寸，两边都不得大于源 / destination size, never larger than source
    uint32_t* acc;                  ///< 纵向加权和 / weighted sum per destination column
    uint32_t* weight;               ///< 纵向权重和 / accumulated weight per destination column
    uint8_t* hrow;                  ///< 一条源行的横向结果 / one horizontally scaled source row
    int next_row;                   ///< 下一条待消费的源行 / next source row to consume
    int band;                       ///< 正在累加的输出行 / destination row being accumulated
    int emitted;                    ///< 已发射的输出行数 / output rows emitted
} img_box_scaler_t;

/// 需要的存储字节数：两轴累加器 + 一行的横向结果，都与源尺寸无关。
/// Bytes required: two accumulators plus one horizontally scaled row; independent of source size.
size_t img_box_scaler_memory(int dst_width);

/// 初始化；dst 任一边大于 src 时返回 false，存储不足也返回 false。
/// Initialise; returns false if either destination side exceeds its source side, or storage is short.
bool img_box_scaler_init(img_box_scaler_t* s, uint8_t* store, size_t store_size,
                         int src_width, int src_height, int dst_width, int dst_height);

/// 已发射的输出行数。/ Output rows emitted so far.
int img_box_emitted_rows(const img_box_scaler_t* s);

/// 下一条该喂的源行号，也就是本次没消费完的位置。
/// Next source row to feed, i.e. where an unfinished push stopped.
int img_box_source_row(const img_box_scaler_t* s);

/// 推入若干源行，尽可能多地发射输出行；返回写进 dst_rows 的行数，dst_capacity 是 dst_rows 的
/// 字节数。目标缓冲放不下某个输出带时会在那一源行被消费之前停下，调用方按 img_box_source_row
/// 从没消费的源行重推。
/// Push source rows, emitting every output row the push completes; returns rows written and takes
/// dst_capacity in bytes. A row that would complete a band with no room left is not consumed;
/// resume from img_box_source_row.
int img_box_push(img_box_scaler_t* s, const uint8_t* gray_rows, int row_count,
                 uint8_t* dst_rows, int dst_capacity);
