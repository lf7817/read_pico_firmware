/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：4bpp 平面宿主机测试。对照路径在测试里现搭：整图解码 -> 整图缩放 -> 同一套抖动打包。
 * English: Host test for the 4bpp plane. The reference is assembled in the test: whole-image
 * decode, whole-image scale, then the same dither and packing.
 * 冻结：仅用于宿主测试。/ Frozen: Host tests only.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "img_dither.h"
#include "img_plane.h"
#include "img_scale.h"

size_t book_heap_peak(void);
void book_heap_reset(void);

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

// 桩 epd_draw_pixel：按 epdiy 的规则写 nibble，好让测试能核对 blit 的基准。
// Stub of epd_draw_pixel: packs nibbles the way epdiy does, so the test can check the blit basis.
static uint8_t* s_fb;
static int s_fb_w, s_fb_h;
void epd_draw_pixel(int x, int y, uint8_t color, uint8_t* framebuffer) {
    (void)framebuffer;
    if (x < 0 || y < 0 || x >= s_fb_w || y >= s_fb_h) return;
    uint8_t* byte = &s_fb[(size_t)y * ((s_fb_w + 1) / 2) + x / 2];
    *byte = (uint8_t)((x & 1) ? ((*byte & 0x0F) | (color & 0xF0)) : ((*byte & 0xF0) | (color >> 4)));
}

typedef struct { uint8_t* plane; int width, rows, mismatch; } whole_t;

static bool gather(void* ctx, const uint8_t* rows, int count, int first, int width) {
    whole_t* w = (whole_t*)ctx;
    if (first != w->rows) w->mismatch = 1;
    for (int i = 0; i < count; ++i)
        memcpy(w->plane + (size_t)(first + i) * width, rows + (size_t)i * width, (size_t)width);
    w->rows = first + count;
    return !w->mismatch;
}

// 对照：整图 8-bit 平面一次缩放，再按同一套档位打包。测试里允许存在整图平面，产品路径不允许。
// Reference: one whole-image 8-bit plane scaled at once, then packed with the same levels. The test
// may hold a whole plane; the product path may not.
static img_plane_t build_reference(const uint8_t* data, size_t len, img_format_t format, int src_w,
                                   int src_h, int tw, int th) {
    img_plane_t plane = {0};
    int dec_w = format == IMG_FORMAT_JPEG && (src_w >> 3) >= tw && (src_h >> 3) >= th ? src_w >> 3 : src_w;
    int dec_h = format == IMG_FORMAT_JPEG && (src_w >> 3) >= tw && (src_h >> 3) >= th ? src_h >> 3 : src_h;
    whole_t whole = { malloc((size_t)dec_w * dec_h), dec_w, 0, 0 };
    assert(whole.plane);
    img_reader_t r;
    assert(img_reader_from_memory(&r, data, len));
    int gw = 0, gh = 0;
    if (format == IMG_FORMAT_JPEG) {
        uint8_t* work = malloc(IMG_JPEG_WORK_MIN);
        uint8_t* strip = malloc((size_t)IMG_STRIP_ROWS * dec_w);
        assert(work && strip);
        assert(img_decode_jpeg_strips(&r, work, IMG_JPEG_WORK_MIN, strip,
                                      (size_t)IMG_STRIP_ROWS * dec_w,
                                      dec_w != src_w ? 3 : 0, gather, &whole, &gw, &gh));
        free(strip);
        free(work);
    } else {
        assert(img_decode_png_rows(&r, gather, &whole, &gw, &gh));
    }
    assert(gw == dec_w && gh == dec_h && !whole.mismatch);
    size_t store_size = img_box_scaler_memory(tw);
    uint8_t* store = malloc(store_size);
    img_box_scaler_t scaler;
    assert(img_box_scaler_init(&scaler, store, store_size, dec_w, dec_h, tw, th));
    uint8_t* scaled = malloc((size_t)tw * th);
    assert(store && scaled);
    assert(img_box_push(&scaler, whole.plane, dec_h, scaled, (size_t)tw * th) == th);
    plane.nib = malloc(img_plane_bytes(tw, th));
    assert(plane.nib);
    plane.width = (uint16_t)tw;
    plane.height = (uint16_t)th;
    for (int y = 0; y < th; ++y) for (int x = 0; x < tw; ++x)
        img_plane_set(&plane, x, y, img_dither_gray_to_4bpp(scaled[(size_t)y * tw + x], x, y));
    free(scaled);
    free(store);
    free(whole.plane);
    return plane;
}

static void check_build(const char* path, img_format_t format, int src_w, int src_h, int max_w,
                        int max_h) {
    size_t len = 0;
    uint8_t* data = read_file(path, &len);
    if (!data) { printf("SKIP %s\n", path); return; }
    int tw = 0, th = 0;
    img_plane_target(src_w, src_h, max_w, max_h, &tw, &th);
    img_reader_t r;
    assert(img_reader_from_memory(&r, data, len));
    book_heap_reset();
    img_plane_t plane = {0};
    assert(img_plane_build(&r, format, src_w, src_h, max_w, max_h, &plane));
    size_t peak = book_heap_peak();
    assert(plane.width == tw && plane.height == th);
    // 峰值只允许是平面本身加一小撮条带缓冲；出现整图 8-bit 平面会立刻超线。
    // The peak may be the plane plus a handful of strip buffers; a whole 8-bit plane trips it.
    assert(peak <= img_plane_bytes(tw, th) + 64u * 1024u);
    img_plane_t ref = build_reference(data, len, format, src_w, src_h, tw, th);
    assert(ref.width == tw && ref.height == th);
    for (int y = 0; y < th; ++y) for (int x = 0; x < tw; ++x) {
        if (img_plane_get(&plane, x, y) == img_plane_get(&ref, x, y)) continue;
        printf("MISMATCH %s at row %d col %d: plane %u reference %u\n", path, y, x,
               img_plane_get(&plane, x, y), img_plane_get(&ref, x, y));
        fflush(stdout);
        abort();
    }
    printf("ok %s %dx%d -> %dx%d peak %zu KiB\n", path, src_w, src_h, tw, th, peak / 1024);
    fflush(stdout);
    img_plane_free(&ref);
    img_plane_free(&plane);
    free(data);
}

int main(void) {
    // 打包基准：奇数列在高 4 位，偶数列在低 4 位，与 epd_draw_pixel 一致。
    // Packing basis: odd columns in the high half, even in the low half, matching epd_draw_pixel.
    uint8_t nib[5] = {0};
    img_plane_t p = { nib, 9, 1 };
    for (int x = 0; x < 9; ++x) img_plane_set(&p, x, 0, (uint8_t)(x + 3));
    assert(nib[0] == 0x43 && nib[1] == 0x65 && nib[4] == 0x0B);
    for (int x = 0; x < 9; ++x) assert(img_plane_get(&p, x, 0) == (uint8_t)(x + 3));
    assert(img_plane_get(&p, 9, 0) == 15 && img_plane_get(&p, -1, 0) == 15);   // 越界读白 / reads white
    img_plane_set(&p, 99, 0, 1);                                               // 越界写忽略 / writes ignored
    assert(nib[4] == 0x0B);

    // 等比内接：只缩小、不放大，任一边至少 1。
    // Aspect fit: shrink only, never enlarge, each side at least one.
    int tw = 0, th = 0;
    img_plane_target(100, 100, 200, 200, &tw, &th);
    assert(tw == 100 && th == 100);
    img_plane_target(1000, 1333, 604, 1064, &tw, &th);
    assert(tw == 604 && th == 805);
    img_plane_target(4000, 100, 604, 1064, &tw, &th);
    assert(tw == 604 && th == 15);
    img_plane_target(3, 4000, 604, 1064, &tw, &th);
    assert(tw == 1 && th == 1064);
    assert(img_plane_bytes(9, 7) == 5 * 7);

    // blit 与平面读取同基准。/ The blit shares the plane's packing basis.
    uint8_t fb[3 * ((10 + 1) / 2)] = {0};
    s_fb = fb; s_fb_w = 10; s_fb_h = 3;
    img_plane_blit(&p, fb, 0, 0, 9, 1);
    for (int x = 0; x < 9; ++x) assert((fb[x / 2] >> ((x & 1) ? 4 : 0) & 0x0F) == (uint8_t)(x + 3));
    // 裁剪：只给 4 列宽就要停在那 4 列，不能把整条平面涂出去。
    // Clipping: asking for four columns must paint four columns, not the whole plane.
    memset(fb, 0, sizeof(fb));
    img_plane_blit(&p, fb, 0, 0, 4, 1);
    for (int x = 0; x < 4; ++x) assert((fb[x / 2] >> ((x & 1) ? 4 : 0) & 0x0F) == (uint8_t)(x + 3));
    assert((fb[2] >> 4) == 0);

    check_build("build/img-fixtures/mono400.jpg", IMG_FORMAT_JPEG, 400, 640, 200, 320);
    check_build("build/img-fixtures/chroma400.jpg", IMG_FORMAT_JPEG, 400, 640, 50, 80);
    check_build("build/img-fixtures/gray160x80.png", IMG_FORMAT_PNG, 160, 80, 80, 40);
    printf("img_plane: ok\n");
    return 0;
}
