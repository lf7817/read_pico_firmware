/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：解码日志替身。/ English: Logging shim for the decode host tests.
 * 冻结：仅用于宿主测试。/ Frozen: Host tests only.
 */
#pragma once
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGW(tag, ...) ((void)(tag))
#define ESP_LOGE(tag, ...) ((void)(tag))
