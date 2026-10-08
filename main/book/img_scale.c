/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：横向把一条源行按分数重叠加权折成 dst_width 个 8-bit 值，纵向按同样的分数跨条带累加。
 * English: The horizontal pass folds one source row into dst_width 8-bit values by fractional
 * overlap; the vertical pass accumulates the same fractions across strip boundaries.
 *
 * 冻结：全程整数定点，禁止浮点——缩略图缓存要和分页一样可逐字节复现。
 * Frozen: integer fixed point throughout, no floating point — thumbnail output must stay
 * byte-reproducible the same way pagination does.
 */
#include "img_scale.h"
#include <string.h>

size_t img_box_scaler_memory(int dst_width) {
    if (dst_width <= 0) return 0;
    // 两个 uint32 数组加一条 uint8 行，再留四字节对齐余量。
    // Two uint32 arrays plus one uint8 row, with four bytes of alignment slack.
    return (size_t)dst_width * (2 * sizeof(uint32_t) + 1) + sizeof(uint32_t);
}

static uintptr_t align4(uintptr_t at) { return (at + 3u) & ~(uintptr_t)3u; }

bool img_box_scaler_init(img_box_scaler_t* s, uint8_t* store, size_t store_size,
                         int src_width, int src_height, int dst_width, int dst_height) {
    if (!s || !store || src_width <= 0 || src_height <= 0 || dst_width <= 0 || dst_height <= 0) return false;
    if (dst_width > src_width || dst_height > src_height) return false;
    if (store_size < img_box_scaler_memory(dst_width)) return false;
    uintptr_t at = (uintptr_t)store;
    s->acc = (uint32_t*)align4(at);        at = (uintptr_t)s->acc + (size_t)dst_width * sizeof(uint32_t);
    s->weight = (uint32_t*)align4(at);     at = (uintptr_t)s->weight + (size_t)dst_width * sizeof(uint32_t);
    s->hrow = (uint8_t*)align4(at);
    memset(s->acc, 0, (size_t)dst_width * sizeof(uint32_t));
    memset(s->weight, 0, (size_t)dst_width * sizeof(uint32_t));
    s->src_width = src_width; s->src_height = src_height;
    s->dst_width = dst_width; s->dst_height = dst_height;
    s->next_row = 0; s->band = 0; s->emitted = 0;
    return true;
}

int img_box_emitted_rows(const img_box_scaler_t* s) { return s ? s->emitted : 0; }
int img_box_source_row(const img_box_scaler_t* s) { return s ? s->next_row : 0; }

// 目标格覆盖的源区间用"乘对边"的整数尺度表示：dx 覆盖 [dx*src, (dx+1)*src)，源列 sx 覆盖
// [sx*dst, (sx+1)*dst)。两边同尺度相减就是精确重叠长度，不引入取整漂移——条带喂入和整图喂入
// 才会逐字节一致。
// A destination cell covers source columns [dx*src, (dx+1)*src) and a source column covers
// [sx*dst, (sx+1)*dst) in the same multiply-by-the-other-side scale, so the overlap is exact and
// free of rounding drift, which is what makes strip input byte-identical to whole-image input.
static void scale_row(img_box_scaler_t* s, const uint8_t* src) {
    for (int dx = 0; dx < s->dst_width; ++dx) {
        unsigned long long xa = (unsigned long long)dx * (unsigned)s->src_width;
        unsigned long long xb = xa + (unsigned)s->src_width;
        unsigned first = (unsigned)(xa / (unsigned)s->dst_width);
        unsigned last = (unsigned)((xb - 1) / (unsigned)s->dst_width);
        unsigned long long sum = 0, span = 0;
        for (unsigned sx = first; sx <= last; ++sx) {
            unsigned long long ca = (unsigned long long)sx * (unsigned)s->dst_width;
            unsigned long long cb = ca + (unsigned)s->dst_width;
            unsigned long long from = xa > ca ? xa : ca;
            unsigned long long to = xb < cb ? xb : cb;
            sum += (unsigned long long)src[sx] * (to - from);
            span += to - from;
        }
        s->hrow[dx] = (uint8_t)((sum + span / 2) / span);
    }
}

int img_box_push(img_box_scaler_t* s, const uint8_t* gray_rows, int row_count,
                 uint8_t* dst_rows, int dst_capacity) {
    if (!s || !gray_rows || !dst_rows || row_count <= 0) return 0;
    int written = 0, room = dst_capacity / s->dst_width;   // 容量按行算，入参是字节 / capacity in rows, argument in bytes

    for (int i = 0; i < row_count && s->next_row < s->src_height && s->band < s->dst_height; ++i) {
        int sy = s->next_row;
        unsigned long long ra = (unsigned long long)sy * (unsigned)s->dst_height;
        unsigned long long rb = ra + (unsigned)s->dst_height;
        unsigned long long bb = (unsigned long long)(s->band + 1) * (unsigned)s->src_height;
        // 只做缩小，所以一个带至少覆盖一条源行，一条源行至多补全一个带。
        // Downscale only, so a band spans at least one source row and a row closes at most one band.
        if (rb >= bb && written >= room) break;
        scale_row(s, gray_rows + (size_t)i * (size_t)s->src_width);
        ++s->next_row;
        unsigned long long ba = bb - (unsigned long long)s->src_height;
        unsigned long long from = ra > ba ? ra : ba;
        unsigned long long to = rb < bb ? rb : bb;
        if (to > from) {
            uint32_t w = (uint32_t)(to - from);
            for (int dx = 0; dx < s->dst_width; ++dx) {
                s->acc[dx] += (uint32_t)s->hrow[dx] * w;
                s->weight[dx] += w;
            }
        }
        if (rb < bb) continue;
        for (int dx = 0; dx < s->dst_width; ++dx) {
            uint32_t w = s->weight[dx] ? s->weight[dx] : 1u;
            dst_rows[(size_t)written * s->dst_width + dx] = (uint8_t)((s->acc[dx] + w / 2) / w);
            s->acc[dx] = 0;
            s->weight[dx] = 0;
        }
        ++s->emitted;
        ++written;
        ++s->band;
    }
    return written;
}
