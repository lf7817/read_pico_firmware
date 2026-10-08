/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：换后缀探测同名图片文件。
 * English: Probe the same stem with image extensions swapped in.
 *
 * 冻结：只认三种位图后缀；SVG 与外链一律不算封面。
 * Frozen: three bitmap extensions only; SVG and remote references are never a cover.
 */
#include "book_cover.h"
#include <string.h>
#include <sys/stat.h>

static const char* const s_extensions[] = { ".jpg", ".jpeg", ".png" };

bool book_cover_source(const char* book_path, char* out, size_t out_size) {
    if (!book_path || !out || !out_size) return false;
    const char* dot = strrchr(book_path, '.');
    // 没有后缀、后缀在目录名里（".hidden"）或者长到放不下，都不去猜。
    // No extension, an extension that is really a hidden name, or no room: refuse instead of guessing.
    if (!dot || dot == book_path || strchr(dot, '/')) return false;
    size_t stem = (size_t)(dot - book_path);
    for (size_t i = 0; i < sizeof(s_extensions) / sizeof(*s_extensions); ++i) {
        size_t ext = strlen(s_extensions[i]);
        if (stem + ext + 1 > out_size) return false;
        memcpy(out, book_path, stem);
        memcpy(out + stem, s_extensions[i], ext + 1);
        struct stat st;
        if (stat(out, &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0) return true;
    }
    out[0] = 0;
    return false;
}
