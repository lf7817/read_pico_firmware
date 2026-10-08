/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：封面探测宿主机测试。/ English: Host test for cover lookup.
 * 冻结：仅用于宿主测试。/ Frozen: Host tests only.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "book_cover.h"

#define DIR "/tmp/rp-cover-host"

static void touch(const char* name, const char* body) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s", DIR, name);
    FILE* f = fopen(path, "wb");
    assert(f);
    fputs(body, f);
    fclose(f);
}

int main(void) {
    if (system("rm -rf " DIR) != 0) { /* 首次运行本来就不存在 / legitimately absent on the first run */ }
    assert(mkdir(DIR, 0777) == 0);
    char out[256], probe[256];
    touch("a.epub", "book");
    touch("a.jpg", "cover");
    touch("b.epub", "book");
    touch("c.txt", "book");
    touch("c.jpeg", "cover");
    touch("empty.png", "");
    touch("empty.epub", "book");

    snprintf(probe, sizeof(probe), "%s/a.epub", DIR);
    assert(book_cover_source(probe, out, sizeof(out)) && strstr(out, "a.jpg"));
    snprintf(probe, sizeof(probe), "%s/b.epub", DIR);
    assert(!book_cover_source(probe, out, sizeof(out)) && !out[0]);   // 没有同名图片 / no sibling image
    snprintf(probe, sizeof(probe), "%s/c.txt", DIR);
    assert(book_cover_source(probe, out, sizeof(out)) && strstr(out, "c.jpeg"));
    snprintf(probe, sizeof(probe), "%s/empty.epub", DIR);
    assert(!book_cover_source(probe, out, sizeof(out)));              // 零字节文件不算封面 / empty is not a cover
    assert(!book_cover_source("noextension", out, sizeof(out)));
    assert(!book_cover_source(".hidden", out, sizeof(out)));          // 后缀其实是隐藏名 / the dot is a hidden name
    assert(!book_cover_source(DIR "/a.epub", out, 4));                // 放不下就拒绝 / refuse to overflow
    assert(book_cover_source(NULL, out, sizeof(out)) == false);
    printf("book_cover: ok\n");
    if (system("rm -rf " DIR) != 0) return 1;
    return 0;
}
