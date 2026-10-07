/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * NVS 里的用户设置：睡眠档、字体路径、上次唤醒/开机原因、拿起唤醒开关。
 *
 * User settings in NVS: sleep mode, font path, last wake/boot reason,
 * pickup-wake switch.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/// 默认深睡。浅睡：按键回原页；拿起唤醒默认关。软睡：SOFT_SLEEP 拉低 EN，再短按开机。关机：EN=0，长按开机。
/// Default is deep. Light: key returns to the page; pickup-wake defaults off. Soft sleep: SOFT_SLEEP drops EN, then a short press boots. Off: EN=0, long-press to boot.
typedef enum {
    APP_SLEEP_LIGHT = 0,
    APP_SLEEP_DEEP = 1,
    APP_SLEEP_OFF = 2,
} app_sleep_mode_t;

void app_settings_init(void);
app_sleep_mode_t app_settings_sleep_mode(void);
void app_settings_set_sleep_mode(app_sleep_mode_t mode);
const char* app_sleep_mode_name(app_sleep_mode_t mode);
/// 空路径表示固件内建字体；非空为 SD 上的 TTF。/ Empty path is the built-in font; non-empty is a TTF on the SD card.
const char* app_settings_font_path(void);
void app_settings_set_font_path(const char* path);
/// 上次浅睡唤醒源（app_wake_source_t），掉电也保留。/ Last light-sleep wake source (app_wake_source_t); kept across power loss.
uint8_t app_settings_last_wake(void);
void app_settings_set_last_wake(uint8_t src);
/// 最近一次非 0 的 PMU wake_reason。STATUS 报 0 时用这个回显。/ Last non-zero PMU wake_reason. Used when STATUS reports 0.
uint8_t app_settings_last_boot(void);
void app_settings_set_last_boot(uint8_t reason);
/// 浅睡拿起唤醒。默认关；有加速度计也不会自动开。/ Light-sleep pickup wake. Defaults off; an accelerometer does not turn it on.
bool app_settings_pickup_wake(void);
void app_settings_set_pickup_wake(bool on);
/// 阅读默认字号，36..72、步长 4，默认 48。/ Default reading size, 36..72 in steps of 4, initially 48.
uint8_t app_settings_book_px(void);
/// 无效字号恢复 48。/ Invalid sizes fall back to 48.
void app_settings_set_book_px(uint8_t px);
/// 实验性晃动翻页，默认关闭。/ Experimental shake page turn, off by default.
bool app_settings_book_shake(void);
/// 保存实验性晃动翻页开关。/ Persist the experimental shake page-turn switch.
void app_settings_set_book_shake(bool on);
/// 阅读翻页方式。右手=现状；左手=镜像点击分区；按键=只认按键。
/// Reading page-turn mode. Right = the shipped layout, Left mirrors the tap
/// zones, Keys ignores taps and swipes for paging.
typedef enum {
    APP_TURN_RIGHT = 0,
    APP_TURN_LEFT = 1,
    APP_TURN_KEYS = 2,
} app_turn_mode_t;
/// 默认右手，即历史行为。/ Defaults to right-hand, the historical behaviour.
app_turn_mode_t app_settings_turn_mode(void);
/// 保存翻页方式，非法值回落右手。/ Persist the mode; invalid values fall back to right.
void app_settings_set_turn_mode(app_turn_mode_t mode);
/// 无操作自动锁屏分钟数：0=关，其余为 5/10/30。默认关，不改变现有行为。
/// Idle auto-lock in minutes: 0 = off, otherwise 5/10/30. Defaults off so the
/// existing behaviour does not change on its own.
uint8_t app_settings_auto_lock_min(void);
/// 非法值回落 0（关）。/ Invalid values fall back to 0 (off).
void app_settings_set_auto_lock_min(uint8_t min);
