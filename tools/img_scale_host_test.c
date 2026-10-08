/* 宿主机测试：跨条带喂入必须与一次性喂整图逐字节相同；分数比例下必须保均值；1:1 必须透传。
   Host test: strip-fed output must be byte-identical to whole-image output, mean-preserving at
   fractional ratios, and a pass-through at 1:1. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "img_scale.h"

#define W 61
#define H 43
#define DW 17
#define DH 11

static void fill(uint8_t* buf, int w, int h) {
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) buf[(size_t)y * w + x] = (uint8_t)((x * 7 + y * 13) & 0xFF);
}

static double mean_of(const uint8_t* buf, size_t n) {
    unsigned long long s = 0;
    for (size_t i = 0; i < n; ++i) s += buf[i];
    return (double)s / (double)n;
}

int main(void) {
    uint8_t src[W * H], whole[DW * DH], strip[DW * DH];
    fill(src, W, H);
    size_t need = img_box_scaler_memory(DW);
    uint8_t* st1 = malloc(need);
    uint8_t* st2 = malloc(need);
    assert(st1 && st2);

    img_box_scaler_t a, b;
    assert(img_box_scaler_init(&a, st1, need, W, H, DW, DH));
    assert(img_box_push(&a, src, H, whole, DW * DH) == DH);
    assert(img_box_emitted_rows(&a) == DH);

    assert(img_box_scaler_init(&b, st2, need, W, H, DW, DH));
    int sy = 0, out = 0;
    while (sy < H) {
        int n = (H - sy) < 5 ? (H - sy) : 5;
        int wrote = img_box_push(&b, src + (size_t)sy * W, n, strip + (size_t)out * DW, DW * (DH - out));
        assert(wrote == n * DH / H || wrote <= n + 1);   // 条带边界处至多多发一行 / at most one extra row at a band edge
        assert(img_box_source_row(&b) == sy + n);        // 容量够就必须把推入的源行全消费掉
        out += wrote;
        sy += n;
    }
    assert(out == DH && img_box_emitted_rows(&b) == DH);
    assert(!memcmp(whole, strip, DW * DH));              // 分数比例 + 非整条带也要逐字节一致

    // 1:1 透传（存储要按目标宽 61 重新要，不能复用 17 列那份）
    size_t need1to1 = img_box_scaler_memory(W);
    uint8_t* st5 = malloc(need1to1);
    uint8_t* same = malloc((size_t)W * H);
    assert(st5 && same);
    img_box_scaler_t c;
    assert(img_box_scaler_init(&c, st5, need1to1, W, H, W, H));
    assert(img_box_push(&c, src, H, same, (size_t)W * H) == H);
    assert(!memcmp(src, same, (size_t)W * H));

    // 放大一律拒
    img_box_scaler_t d;
    assert(!img_box_scaler_init(&d, NULL, 0, W, H, W * 2, H));
    assert(!img_box_scaler_init(&d, NULL, 0, W, H, W, H + 1));

    // 存储不足要拒，并且不能写坏调用方的内存
    assert(!img_box_scaler_init(&d, st1, need - 1, W, H, DW, DH));

    // 容量不足时不得吞掉源行：只消费到能发射为止，剩下的由调用方按 img_box_source_row 重推。
    // A short destination buffer must not swallow source rows: it stops at what it can emit.
    img_box_scaler_t f;
    assert(img_box_scaler_init(&f, st2, need, W, H, DW, DH));
    assert(img_box_push(&f, src, 20, strip, DW * 2) == 2);
    assert(img_box_source_row(&f) <= 20 && img_box_emitted_rows(&f) == 2);

    // 大比例保均值：1000x250 -> 333x111（分数比例），按 7 行的条带喂入
    const int bw = 1000, bh = 250, bdw = 333, bdh = 111;
    uint8_t* big = malloc((size_t)bw * bh);
    for (int y = 0; y < bh; ++y)
        for (int x = 0; x < bw; ++x) big[(size_t)y * bw + x] = (uint8_t)((x * 3 + y) & 0xFF);
    uint8_t* out2 = malloc((size_t)bdw * bdh);
    uint8_t* out3 = malloc((size_t)bdw * bdh);
    size_t need2 = img_box_scaler_memory(bdw);
    uint8_t* st3 = malloc(need2);
    uint8_t* st4 = malloc(need2);
    img_box_scaler_t e, g;
    assert(img_box_scaler_init(&e, st3, need2, bw, bh, bdw, bdh));
    int emitted = 0, at = 0;
    while (at < bh) {
        int n = (bh - at) < 7 ? (bh - at) : 7;
        emitted += img_box_push(&e, big + (size_t)at * bw, n, out2 + (size_t)emitted * bdw,
                                (size_t)bdw * (bdh - emitted));
        at += n;
    }
    assert(emitted == bdh);
    assert(img_box_scaler_init(&g, st4, need2, bw, bh, bdw, bdh));
    assert(img_box_push(&g, big, bh, out3, (size_t)bdw * bdh) == bdh);
    assert(!memcmp(out2, out3, (size_t)bdw * bdh));      // 条带与整图在分数比例下也必须逐字节一致
    double dm = mean_of(big, (size_t)bw * bh) - mean_of(out2, (size_t)bdw * bdh);
    assert(dm > -0.5 && dm < 0.5);                       // 分数比例下也必须保住均值
    // 发丝线不丢：600 宽里唯一一根黑列缩到 100 宽（6:1）后必须落在一个格子 1/6 的权重上，
    // 既不能被最近邻整根丢掉（255），也不能被放大成整格黑（0）。
    // A hairline must survive a 6:1 downscale as 1/6 of one cell — nearest neighbour would either
    // drop it entirely (255) or thicken it into a whole black column (0).
    const int hw = 600, hh = 10, hdw = 100, hdh = 5;
    uint8_t* line = malloc((size_t)hw * hh);
    uint8_t* lout = malloc((size_t)hdw * hdh);
    size_t need4 = img_box_scaler_memory(hdw);
    uint8_t* st6 = malloc(need4);
    assert(line && lout && st6);
    for (int y = 0; y < hh; ++y)
        for (int x = 0; x < hw; ++x) line[(size_t)y * hw + x] = (uint8_t)(x == 300 ? 0 : 255);
    img_box_scaler_t k;
    assert(img_box_scaler_init(&k, st6, need4, hw, hh, hdw, hdh));
    assert(img_box_push(&k, line, hh, lout, (size_t)hdw * hdh) == hdh);
    for (int y = 0; y < hdh; ++y) for (int dx = 0; dx < hdw; ++dx) {
        uint8_t v = lout[(size_t)y * hdw + dx];
        if (dx == 50) assert(v >= 205 && v <= 220);   // 255 - 255/6 = 212.5
        else assert(v == 255);
    }

    free(st1); free(st2); free(st5); free(same); free(big); free(out2); free(out3);
    free(st3); free(st4); free(line); free(lout); free(st6);
    printf("img_scale: ok\n");
    return 0;
}
