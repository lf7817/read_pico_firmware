/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：墙上时钟。传书页 STA 取得地址后做一次 SNTP 校时（N2），阅读页 HUD（N1）
 * 与后续锁屏从这里取时间。PMU 的 RTC 走 LSI 无晶振（±1~2%/天），本模块不读不写它；
 * 校时结果只活在 ESP32 本次会话里，断电即失效，所以无效时调用方不得显示占位时间。
 * 格式化只产出 ASCII 与数字，不新增中文字形，内建子集字体无需重生成。
 *
 * English: Wall clock. One-shot SNTP sync after the transfer page's STA gets an
 * address (N2); the reader HUD (N1) and later the lock face read time from here.
 * The PMU RTC runs on an untrimmed LSI (±1~2%/day); this module neither reads nor
 * writes it. The sync lives only in this ESP32 session and dies with power, so
 * callers must not show a placeholder when invalid. Formatters emit ASCII and
 * digits only, keeping the built-in font subset untouched.
 *
 * 冻结：时区固定 UTC+8；多时区与 PMU RTC 回写是后续单独决策，不在本模块里偷偷做。
 * Frozen: timezone fixed at UTC+8; multi-timezone and writing the PMU RTC are
 * separate later decisions, not to be smuggled into this module.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/// 启动一次性 SNTP 校时，幂等。只在 STA 取得地址后调；AP 模式没有上游。
/// / Start the one-shot SNTP sync; idempotent. Call only after STA has an
/// address; AP mode has no upstream.
void app_clock_start(void);

/// 本次会话已校时且墙上时间可信（不早于 2024-01-01）。
/// / Synced this session and the wall time is plausible (not before 2024-01-01).
bool app_clock_valid(void);

/// "21:40"。无效返回 false 且不写 out。/ "21:40"; false without touching out when invalid.
bool app_clock_hm(char* out, size_t size);

/// "10-07 Wed"。无效返回 false 且不写 out。/ "10-07 Wed"; false without touching out when invalid.
bool app_clock_md(char* out, size_t size);

#ifdef __cplusplus
}
#endif
