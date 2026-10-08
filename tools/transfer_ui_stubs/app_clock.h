/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：主机测试替身。传书页只调 app_clock_start()，这里给个本 TU 的计数桩。
 * English: Host test shim. The transfer page only calls app_clock_start(); this is a per-TU counter stub.
 * 冻结：仅用于测试。/ Frozen: Tests only.
 */
#pragma once

/// 记录传书页请求了几次校时。/ Counts how often the transfer page asks for a sync.
static int test_clock_starts;

static inline void app_clock_start(void) { ++test_clock_starts; }
