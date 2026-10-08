/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：PNG 逐行解码宿主机测试。夹具内容由公式给出，所以期望值不经过任何解码器。
 * English: Host test for row-wise PNG decoding. Fixtures are formula-generated, so the expected
 * bytes never pass through a decoder.
 * 冻结：仅用于宿主测试。/ Frozen: Host tests only.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "img_decode.h"

// 宿主堆替身要的计数器；只在"关掉分配"这一用例里改成 0。
// The host heap shim needs this counter; only the allocation-off case sets it to 0.
int image_fail_after = -1;

static uint8_t* read_file(const char* path, size_t* len) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t* buf = n > 0 ? malloc((size_t)n) : NULL;
    if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return NULL; }
    fclose(f);
    *len = (size_t)n;
    return buf;
}

typedef struct { uint8_t* buf; int rows, width, next_row, mismatch, calls; } sink_t;

static bool collect(void* ctx, const uint8_t* rows, int count, int first, int width) {
    sink_t* s = ctx;
    ++s->calls;
    if (s->width && s->width != width) s->mismatch = 1;
    s->width = width;
    if (first != s->next_row || first + count > s->rows) s->mismatch = 1;
    s->next_row = first + count;
    if (s->mismatch) return false;
    for (int i = 0; i < count; ++i)
        memcpy(s->buf + (size_t)(first + i) * width, rows + (size_t)i * width, (size_t)width);
    return true;
}

static void check_gray(const char* path, int w, int h, uint8_t (*pixel)(int, int)) {
    size_t len = 0;
    uint8_t* data = read_file(path, &len);
    if (!data) { printf("SKIP %s\n", path); return; }
    img_reader_t r;
    assert(img_reader_from_memory(&r, data, len));
    uint8_t* plane = malloc((size_t)w * h);
    sink_t sink = { plane, h, 0, 0, 0, 0 };
    int gw = 0, gh = 0;
    assert(img_decode_png_rows(&r, collect, &sink, &gw, &gh));
    assert(!sink.mismatch && gw == w && gh == h && sink.next_row == h);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
        if (plane[(size_t)y * w + x] == pixel((unsigned)x, (unsigned)y)) continue;
        printf("MISMATCH %s at row %d col %d: got %u want %u\n", path, y, x,
               plane[(size_t)y * w + x], pixel((unsigned)x, (unsigned)y));
        fflush(stdout);
        abort();
    }
    printf("ok %s %dx%d in %d strips\n", path, gw, gh, sink.calls);
    fflush(stdout);

    // 关掉分配就必须失败：这条断言证明缓冲确实经过我们的分配器，而不是偷偷用默认 malloc。
    // With allocations disabled the decode must fail: that proves the buffers go through our
    // allocator instead of quietly using the default malloc.
    img_reader_t r2;
    assert(img_reader_from_memory(&r2, data, len));
    image_fail_after = 0;
    int ew = 7, eh = 7;
    assert(!img_decode_png_rows(&r2, collect, &sink, &ew, &eh) && !ew && !eh);
    image_fail_after = -1;
    free(plane);
    free(data);
}

static uint8_t tiny_pixel(int x, int y) { return (uint8_t)(x * 31 + y * 7); }
static uint8_t gray_pixel(int x, int y) { return (uint8_t)(x * 3 + y * 5); }

int main(void) {
    check_gray("build/img-fixtures/tiny.png", 8, 8, tiny_pixel);
    check_gray("build/img-fixtures/gray160x80.png", 160, 80, gray_pixel);

    size_t len = 0;
    uint8_t* inter = read_file("build/img-fixtures/interlaced.png", &len);
    if (inter) {
        img_reader_t r;
        int w = 7, h = 7;
        sink_t sink = { NULL, 0, 0, 0, 0, 0 };
        assert(img_reader_from_memory(&r, inter, len));
        assert(!img_decode_png_rows(&r, collect, &sink, &w, &h) && !w && !h);   // Adam7 直接拒 / Adam7 refused
        printf("ok interlaced refused\n");
        free(inter);
    } else {
        printf("SKIP interlaced\n");
    }
    printf("img_png: ok\n");
    return 0;
}
