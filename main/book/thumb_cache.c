/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：缩略图缓存的键、原子写与严格读回。
 * English: Cache key, atomic write and a strict read-back.
 *
 * 冻结：读失败一律 miss，绝不"尽力而为"地半读；写失败静默，不留 .part 垃圾以外的影响。
 * Frozen: a failed read is always a miss and never a partial read; a failed write stays silent and
 * leaves nothing behind beyond the .part file.
 */
#include "thumb_cache.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "img_dither.h"

#define TAG "thumb_cache"
#define THUMB_HEADER_BYTES 12

static const char* s_root = THUMB_CACHE_ROOT;

void thumb_cache_set_root(const char* root) { s_root = root && root[0] ? root : THUMB_CACHE_ROOT; }
const char* thumb_cache_root(void) { return s_root; }

static uint32_t fnv1a(const char* s) {
    uint32_t h = 2166136261u;
    while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
    return h;
}

bool thumb_cache_key(const char* book_path, uint32_t size, uint32_t mtime, int width, int height,
                     char* out, size_t out_size) {
    if (!book_path || !out || !out_size || width <= 0 || height <= 0) return false;
    char name[96];
    // 抖动版本进文件名：换算法或换阈值表以后旧缓存自然全部 miss。
    // The dither version is part of the name, so changing the map or the algorithm ages every entry
    // out by itself.
    int written = snprintf(name, sizeof(name), "%08x%08x%08x-v%d-%dx%d.4bpp", (unsigned)fnv1a(book_path),
                           (unsigned)size, (unsigned)mtime, IMG_DITHER_VERSION, width, height);
    if (written < 0 || (size_t)written >= sizeof(name)) return false;
    int full = snprintf(out, out_size, "%s/%s", s_root, name);
    return full > 0 && (size_t)full < out_size;
}

static bool ensure_dir(void) {
    if (mkdir(s_root, 0777) == 0 || errno == EEXIST) return true;
    ESP_LOGW(TAG, "mkdir %s failed (%d)", s_root, errno);
    return false;
}

bool thumb_cache_load(const char* path, img_plane_t* out) {
    if (out) { out->nib = NULL; out->width = out->height = 0; }
    if (!path || !out) return false;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    uint8_t head[THUMB_HEADER_BYTES];
    bool ok = fread(head, 1, sizeof(head), f) == sizeof(head) && !memcmp(head, "RP1", 3);
    uint16_t width = 0, height = 0;
    uint32_t payload = 0;
    if (ok) {
        width = (uint16_t)(head[4] << 8 | head[5]);
        height = (uint16_t)(head[6] << 8 | head[7]);
        payload = (uint32_t)head[8] << 24 | (uint32_t)head[9] << 16 | (uint32_t)head[10] << 8 | head[11];
        // 头里的长度必须正好等于剩下的字节数：半截文件在这里就被拒掉。
        // The declared length must equal the remaining bytes exactly, which is where a truncated
        // file gets refused.
        ok = width && height && payload == (uint32_t)img_plane_bytes(width, height);
        if (ok) {
            ok = fseek(f, 0, SEEK_END) == 0;
            long size = ok ? ftell(f) : -1;
            ok = ok && size == (long)(THUMB_HEADER_BYTES + payload);
        }
    }
    uint8_t* nib = NULL;
    if (ok) {
        nib = malloc(payload);
        if (!nib) ok = false;
        else if (fseek(f, THUMB_HEADER_BYTES, SEEK_SET) != 0 || fread(nib, 1, payload, f) != payload) {
            free(nib);
            nib = NULL;
            ok = false;
        }
    }
    fclose(f);
    if (!ok) return false;
    out->nib = nib;
    out->width = width;
    out->height = height;
    return true;
}

bool thumb_cache_store(const char* path, const img_plane_t* plane) {
    if (!path || !plane || !plane->nib || !plane->width || !plane->height) return false;
    if (!ensure_dir()) return false;
    char part[THUMB_CACHE_PATH_MAX + 8];
    if ((size_t)snprintf(part, sizeof(part), "%s.part", path) >= sizeof(part)) return false;
    size_t payload = img_plane_bytes(plane->width, plane->height);
    uint8_t head[THUMB_HEADER_BYTES] = { 'R', 'P', '1', 0 };
    head[4] = (uint8_t)(plane->width >> 8);
    head[5] = (uint8_t)(plane->width & 0xFF);
    head[6] = (uint8_t)(plane->height >> 8);
    head[7] = (uint8_t)(plane->height & 0xFF);
    head[8] = (uint8_t)(payload >> 24);
    head[9] = (uint8_t)(payload >> 16);
    head[10] = (uint8_t)(payload >> 8);
    head[11] = (uint8_t)payload;
    bool ok = false;
    FILE* f = fopen(part, "wb");
    if (f) {
        ok = fwrite(head, 1, sizeof(head), f) == sizeof(head) &&
             fwrite(plane->nib, 1, payload, f) == payload;
        if (fclose(f) != 0) ok = false;
    }
    if (!ok) {
        remove(part);
        return false;
    }
    // FAT 的 rename 不覆盖同名目标，必须先删。/ FAT rename does not replace an existing name.
    remove(path);
    if (rename(part, path) != 0) {
        ESP_LOGW(TAG, "rename failed (%d)", errno);
        remove(part);
        return false;
    }
    return true;
}
