/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：封面来源查找——同名不同后缀的图片文件。找不到就返回 false，由界面画占位。
 * English: Cover source lookup — an image file with the same stem and a different extension. False
 * when there is none, and the UI draws the placeholder.
 *
 * 冻结：不解析 OPF 的 meta name=cover（那要等子项目 C 的 ZIP 流式读），也不在这里画图。
 * Frozen: no OPF meta name=cover parsing (that waits for subproject C's streaming ZIP reads), and no
 * drawing happens here.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>

/// 在图书路径旁边找 .jpg/.jpeg/.png；命中则写出绝对路径。
/// Look for .jpg/.jpeg/.png next to the book path and write the winner out on a hit.
bool book_cover_source(const char* book_path, char* out, size_t out_size);
