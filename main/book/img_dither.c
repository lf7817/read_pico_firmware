/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：阈值在相邻两档之间决定取哪一档，纯整数、无状态。
 * English: The threshold picks between two adjacent levels; integer only and stateless.
 *
 * 冻结：0 与 255 一律走端点，不参与抖动；正好落在签字档位上的灰度也不抖动。
 * Frozen: 0 and 255 always take an endpoint and never dither, and neither does a gray that lands
 * exactly on a signed-off level.
 */
#include "img_dither.h"
#include "../assets/blue_noise_64.h"

const uint8_t img_dither_levels[IMG_DITHER_LEVELS] = {0x0, 0x3, 0x6, 0xB, 0xD, 0xF};

uint8_t img_dither_gray_to_4bpp(uint8_t gray, int x, int y) {
    if (gray == 0) return img_dither_levels[0];
    if (gray == 255) return img_dither_levels[IMG_DITHER_LEVELS - 1];
    int code = gray >> 4;  // 0..15，与 epdiy 的高 nibble 同基准 / matches epdiy's high nibble
    int lo = 0, hi = IMG_DITHER_LEVELS - 1;
    for (int i = 0; i + 1 < IMG_DITHER_LEVELS; ++i) {
        if (code >= img_dither_levels[i] && code <= img_dither_levels[i + 1]) {
            lo = i;
            hi = i + 1;
            break;
        }
    }
    // 正好命中档位就不抖：发丝线的最低档 0xB0 靠这条保住，否则同一根线会被阈值拆成两档。
    // A level hit is not dithered: the 0xB0 hairline floor depends on it, or one stroke splits in two.
    if (code == img_dither_levels[lo]) return img_dither_levels[lo];
    if (code == img_dither_levels[hi]) return img_dither_levels[hi];
    int span = img_dither_levels[hi] - img_dither_levels[lo];
    int pos = code - img_dither_levels[lo];
    int threshold = img_blue_noise[y & 63][x & 63];  // 0..255
    // 用整数比较代替浮点：pos/span 与 threshold/255 比 / integer compare of pos/span against threshold/255
    return (uint8_t)((pos * 255 > threshold * span) ? img_dither_levels[hi] : img_dither_levels[lo]);
}

void img_dither_row_to_4bpp(const uint8_t* gray, int x_start, int y, int count, uint8_t* out) {
    for (int i = 0; i < count; ++i) out[i] = img_dither_gray_to_4bpp(gray[i], x_start + i, y);
}
