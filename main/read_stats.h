/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：阅读时长统计（N6）。只统计翻页间隔，单次间隔封顶 5 分钟，所以「开着书不动」
 * 不计入阅读时间。日桶需要墙上时钟：没校时就不写日统计、也不落盘，宁可少记也不
 * 拿假日期凑数。数据落在 /flash/read_stats.bin，不占 NVS（NVS 只剩 20 KB，且逐书
 * 进度已经住在里面）。
 *
 * English: Reading-time stats (N6). Only page-turn gaps count, capped at 5 minutes
 * each, so leaving the book open without turning adds nothing. Day buckets need a
 * wall clock: before an NTP sync nothing is written at all rather than filing time
 * under a fake date. Storage is /flash/read_stats.bin, not NVS (the partition has
 * ~20 KB left and per-book progress already lives there).
 *
 * 冻结：不做「读完日期」，那要改逐书进度存储层，属子项目 D。
 * Frozen: no per-book finished date; that belongs to subproject D, which replaces
 * the per-book progress store.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// 载入统计文件（顺带触发内置 flash 挂载）。重复调用只生效一次。
/// / Load the stats file (which also mounts internal flash). Repeat-safe.
void read_stats_init(void);

/// 翻页时调用，累加与上一次翻页之间的间隔（封顶 5 分钟）。
/// / Call on a page turn; adds the gap since the previous turn, capped at 5 min.
void read_stats_note_turn(int64_t now_ms);

/// 把累计的间隔写进今天的日桶并落盘。未校时时直接返回，保留待写量。
/// / Move pending seconds into today's bucket and persist. No-op while the wall
/// clock is untrusted, keeping the pending amount for a later flush.
void read_stats_flush(int64_t now_ms);

/// 「今日 42 分 · 连续 5 天」。没有可显示数据时返回 false 且不写 out。
/// / "today 42 min · 5-day streak"; false without touching out when empty.
bool read_stats_format(char* out, size_t size);

#ifdef __cplusplus
}
#endif
