/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：不画屏的宿主测试共用的 epd_draw_pixel 空实现。
 * English: No-op epd_draw_pixel shared by host tests that never paint.
 * 冻结：仅用于宿主测试。要核对像素的测试自己定义记录版，别改这里。
 * Frozen: Host tests only. Tests that inspect pixels define their own recording version.
 */
#include <stdint.h>

void epd_draw_pixel(int x, int y, uint8_t color, uint8_t* framebuffer) {
    (void)x; (void)y; (void)color; (void)framebuffer;
}
