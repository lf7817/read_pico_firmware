/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：条带解码宿主机测试。对照实现是"按绝对坐标一次写满整图"的同一档 TJpgDec，
 *       不是 book_image.c——它有自己的降采样再放大策略，不是本任务要钉的性质。
 * English: Host test for strip decoding. The reference is the same TJpgDec configuration writing
 *          the whole image at absolute coordinates, not book_image.c, which applies its own
 *          downscale-then-enlarge policy and is not the property this task pins.
 * 冻结：仅用于宿主测试。/ Frozen: Host tests only.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "img_decode.h"
#include "vendor/tjpgd.h"

// 宿主堆替身要的计数器；本测试不做失败注入，停在关闭位。
// The host heap shim needs this counter; this test injects no failures, so it stays disabled.
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

typedef struct { uint8_t* buf; int rows; int width; int next_row; int mismatch; int calls; } sink_t;

static bool collect(void* ctx, const uint8_t* rows, int count, int first, int width) {
    sink_t* s = ctx;
    ++s->calls;
    if (s->width && s->width != width) s->mismatch = 1;
    s->width = width;
    if (first != s->next_row) s->mismatch = 1;           // 条带必须首尾相接，不重不漏 / strips must tile the image
    if (first + count > s->rows) s->mismatch = 1;
    s->next_row = first + count;
    if (s->mismatch) return false;
    for (int i = 0; i < count; ++i)
        memcpy(s->buf + (size_t)(first + i) * width, rows + (size_t)i * width, (size_t)width);
    return true;
}

// 对照：同一档 TJpgDec，块回调直接按 rect 的绝对坐标写进整图平面，没有任何条带拼接逻辑。
// TJpgDec 只有一个 device 指针，所以输入流和输出平面必须住在同一个结构里——
// img_decode.c 也是因为这个才从 out 结构里再指回 reader。
// Reference: the same TJpgDec build writing at absolute coordinates with no strip assembly.
// TJpgDec carries a single device pointer, so the stream and the plane must share one struct;
// that is also why img_decode.c reaches the reader through its output struct.
typedef struct {
    const uint8_t* data;
    size_t len, at;
    uint8_t* plane;
    int width;
} ref_t;

static size_t ref_read(JDEC* jd, uint8_t* dst, size_t want) {
    ref_t* in = (ref_t*)jd->device;
    size_t n = in->len - in->at;
    if (n > want) n = want;
    if (dst && n) memcpy(dst, in->data + in->at, n);
    in->at += n;
    return n;
}

static int ref_block(JDEC* jd, void* bitmap, JRECT* r) {
    ref_t* ref = (ref_t*)jd->device;
    unsigned wide = (unsigned)(r->right - r->left + 1), tall = (unsigned)(r->bottom - r->top + 1);
    for (unsigned y = 0; y < tall; ++y)
        memcpy(ref->plane + ((size_t)(r->top + y) * ref->width) + r->left,
               (const uint8_t*)bitmap + (size_t)y * wide, wide);
    return 1;
}

static uint8_t* decode_whole(const uint8_t* data, size_t len, uint8_t scale, int* width, int* height) {
    void* work = malloc(IMG_JPEG_WORK_MIN);
    JDEC jd;
    ref_t ref = { data, len, 0, NULL, 0 };
    assert(work);
    assert(jd_prepare(&jd, ref_read, work, IMG_JPEG_WORK_MIN, &ref) == JDR_OK);
    *width = (int)jd.width >> scale;
    *height = (int)jd.height >> scale;
    ref.plane = malloc((size_t)*width * *height);
    ref.width = *width;
    assert(ref.plane);
    assert(jd_decomp(&jd, ref_block, scale) == JDR_OK);
    free(work);
    return ref.plane;
}

// 逐字节一致失败时，报出第一个差异在哪一行哪一列——行错位和列错位要区分得开。
// On a byte mismatch, report the first differing row and column: row drift and column drift need
// to tell each other apart.
static bool identical(const uint8_t* got, const uint8_t* want, size_t n, int width, const char* what) {
    for (size_t i = 0; i < n; ++i) {
        if (got[i] == want[i]) continue;
        int diffs = 0;
        for (size_t z = 0; z < n; ++z) diffs += got[z] != want[z];
        printf("MISMATCH %s at row %zu col %zu: strip %u whole %u, %d of %zu bytes differ\n",
               what, i / (size_t)width,
               i % (size_t)width, got[i], want[i], diffs, n);
        fflush(stdout);
        return false;
    }
    return true;
}

static void check(const char* path) {
    size_t len = 0;
    uint8_t* data = read_file(path, &len);
    if (!data) { printf("SKIP %s\n", path); return; }
    uint8_t* work = malloc(IMG_JPEG_WORK_MIN);
    uint8_t* strip = malloc(IMG_STRIP_ROWS * 2048);
    assert(work && strip);
    for (uint8_t scale = 0; scale <= 2; ++scale) {
        int sw = 0, sh = 0;
        uint8_t* ref = decode_whole(data, len, scale, &sw, &sh);
        img_reader_t r;
        assert(img_reader_from_memory(&r, data, len));
        uint8_t* plane = malloc((size_t)sw * sh);
        assert(plane);
        sink_t sink = { plane, sh, 0, 0, 0, 0 };
        int gw = 0, gh = 0;
        assert(img_decode_jpeg_strips(&r, work, IMG_JPEG_WORK_MIN, strip, IMG_STRIP_ROWS * 2048, scale,
                                      collect, &sink, &gw, &gh));
        assert(!sink.mismatch && sink.next_row == sh && sink.width == sw);
        assert(gw == sw && gh == sh);
        char what[128];
        snprintf(what, sizeof(what), "%s scale=%u", path, scale);
        assert(identical(plane, ref, (size_t)sw * sh, sw, what));   // 条带拼接必须与整图逐字节一致
        printf("ok %s scale=%u %dx%d in %d strips\n", path, scale, sw, sh, sink.calls);
        fflush(stdout);   // ASan 中止时也要留下已经过了哪一档 / keep the log when ASan aborts
        free(plane);
        free(ref);
    }
    free(work);
    free(strip);
    free(data);
}

int main(void) {
    check("build/img-fixtures/mono400.jpg");             // 4:2:0 彩色（R=G=B 的灰度内容）
    check("build/img-fixtures/chroma400.jpg");           // 4:2:0 彩色，彩度错位会暴露行错位
    check("build/img-fixtures/plain.jpg");               // 手搓 8x8 灰度，单 MCU

    size_t len = 0;
    uint8_t* prog = read_file("build/img-fixtures/chroma400_prog.jpg", &len);
    if (prog) {
        uint8_t* work = malloc(IMG_JPEG_WORK_MIN);
        uint8_t* strip = malloc(IMG_STRIP_ROWS * 400);
        uint8_t* plane = malloc(400 * 640);
        assert(work && strip && plane);
        img_reader_t r;
        int w = 0, h = 0;
        sink_t sink = { plane, 640, 0, 0, 0, 0 };
        assert(img_reader_from_memory(&r, prog, len));
        // progressive 必须被拒（TJpgDec 在 SOF2 返回 JDR_FMT3）。/ Progressive must be refused.
        assert(!img_decode_jpeg_strips(&r, work, IMG_JPEG_WORK_MIN, strip, IMG_STRIP_ROWS * 400, 0,
                                       collect, &sink, &w, &h));
        // 工作区小于下限时必须失败，不许越界写。/ An undersized work area fails instead of overrunning.
        assert(!img_decode_jpeg_strips(&r, work, 2048, strip, IMG_STRIP_ROWS * 400, 0, collect, &sink, &w, &h));
        // 条带缓冲装不下 16 行时也要失败而不是写穿。/ A strip buffer under 16 rows must fail too.
        assert(!img_decode_jpeg_strips(&r, work, IMG_JPEG_WORK_MIN, strip, 16, 0, collect, &sink, &w, &h));
        // scale 超过 3 直接拒，不静默钳到 1/8。/ scale above 3 is rejected, never silently clamped.
        assert(!img_decode_jpeg_strips(&r, work, IMG_JPEG_WORK_MIN, strip, IMG_STRIP_ROWS * 400, 4,
                                       collect, &sink, &w, &h));
        free(prog); free(plane); free(strip); free(work);
    } else {
        printf("SKIP progressive\n");
    }
    printf("img_decode: ok\n");
    return 0;
}
