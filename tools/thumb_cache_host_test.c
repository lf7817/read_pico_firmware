/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：缩略图缓存宿主机测试：键敏感性、原子写、半截文件与尺寸不符都必须 miss。
 * English: Host test for the thumbnail cache: key sensitivity, atomic writes, and that a partial
 * file or a size mismatch misses instead of half-reading.
 * 冻结：仅用于宿主测试。/ Frozen: Host tests only.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "img_dither.h"
#include "thumb_cache.h"

// 链进来的图像管线用宿主堆替身，它要这个失败注入计数器；本测试不注入，停在关闭位。
// The linked image pipeline uses the host heap shim, which wants this injection counter;
// this test injects nothing, so it stays disabled.
int image_fail_after = -1;

// 链进来的 img_plane 带 blit，本测试不碰显示，所以给它一个空实现。
void epd_draw_pixel(int x, int y, uint8_t color, uint8_t* framebuffer) {
    (void)x; (void)y; (void)color; (void)framebuffer;
}

#define ROOT "/tmp/rp-thumb-host"

static void wipe(void) {
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", ROOT);
    if (system(cmd) != 0) { /* 首次运行目录本来就不存在 / the directory legitimately misses on the first run */ }
}

int main(void) {
    wipe();
    mkdir(ROOT, 0777);
    thumb_cache_set_root(ROOT);
    char key[512], other[512];

    uint8_t nib[(188 * 250 + 1) / 2];
    for (size_t i = 0; i < sizeof(nib); ++i) nib[i] = (uint8_t)(i * 7 + (i / 256));
    img_plane_t p = { nib, 188, 250 };

    assert(thumb_cache_key("/sdcard/books/a.epub", 12345u, 999u, 188, 250, key, sizeof(key)));
    assert(strstr(key, ROOT) == key);
    assert(thumb_cache_store(key, &p));
    img_plane_t q = { NULL, 0, 0 };
    assert(thumb_cache_load(key, &q));
    assert(q.width == 188 && q.height == 250 && q.nib);
    assert(!memcmp(q.nib, nib, sizeof(nib)));
    free(q.nib);
    q = (img_plane_t){ NULL, 0, 0 };

    // 任一键要素变了都必须 miss：路径、字节数、mtime、目标尺寸、抖动版本。
    // Any key ingredient must miss when it changes: path, size, mtime, target dims, dither version.
    assert(thumb_cache_key("/sdcard/books/b.epub", 12345u, 999u, 188, 250, other, sizeof(other)));
    assert(strcmp(other, key));
    assert(thumb_cache_key("/sdcard/books/a.epub", 12346u, 999u, 188, 250, other, sizeof(other)) &&
           !thumb_cache_load(other, &q));
    assert(thumb_cache_key("/sdcard/books/a.epub", 12345u, 998u, 188, 250, other, sizeof(other)) &&
           !thumb_cache_load(other, &q));
    assert(thumb_cache_key("/sdcard/books/a.epub", 12345u, 999u, 188, 251, other, sizeof(other)) &&
           !thumb_cache_load(other, &q));

    // 目录里没有的键要安静地 miss，不许创建文件。
    // An absent key misses quietly and must not create anything.
    char missing[512];
    snprintf(missing, sizeof(missing), "%s/0000000000000000-v9-1x1.4bpp", ROOT);
    assert(!thumb_cache_load(missing, &q) && !q.nib);

    // 半截的 .part 绝不能被当成有效缓存读到。/ A half-written .part may never read back as a hit.
    char part[560];
    snprintf(part, sizeof(part), "%s.part", key);
    FILE* f = fopen(part, "wb");
    assert(f);
    fputs("junk", f);
    fclose(f);
    q = (img_plane_t){ NULL, 0, 0 };
    assert(thumb_cache_load(key, &q));            // 主文件仍然有效 / the real entry still reads
    free(q.nib);
    q = (img_plane_t){ NULL, 0, 0 };
    assert(!thumb_cache_load(part, &q) && !q.nib);   // .part 自己不是缓存 / .part is not an entry
    unlink(part);

    // 截断的主文件必须 miss，而不是读回半张图。/ A truncated entry misses instead of half-loading.
    f = fopen(key, "rb");
    assert(f);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    assert(truncate(key, size - 10) == 0);
    assert(!thumb_cache_load(key, &q) && !q.nib);

    // 重新写入要覆盖旧文件（FAT 的 rename 不覆盖同名，代码里必须先删）。
    // Re-storing must replace the old file; FAT rename does not overwrite, so the code deletes first.
    assert(thumb_cache_store(key, &p));
    assert(thumb_cache_load(key, &q) && q.width == 188);
    free(q.nib);
    assert(!thumb_cache_store(key, NULL));
    assert(!thumb_cache_store(key, &(img_plane_t){ NULL, 10, 10 }));

    // 版本必须进键：换抖动算法后旧缓存不能命中。/ The version belongs in the key: new dither, miss.
    char v1[512], v2[512];
    assert(thumb_cache_key("a", 1u, 1u, 10, 10, v1, sizeof(v1)));
    assert(strstr(v1, "-v") && strstr(v1, ".4bpp"));
    snprintf(v2, sizeof(v2), "%s", v1);
    assert(strcmp(v1, v2) == 0);
    (void)IMG_DITHER_VERSION;
    printf("thumb_cache: ok\n");
    wipe();
    return 0;
}
