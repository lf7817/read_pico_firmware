/* 宿主机测试：抖动必须是 f(gray,x,y) 的位置确定纯函数，且只落在签字的 6 档上。
   Host test: dithering must be a position-deterministic pure function onto the six signed levels. */
#include <assert.h>
#include <stdio.h>
#include "img_dither.h"

int main(void) {
    assert(img_dither_levels[0] == 0x0 && img_dither_levels[1] == 0x3 && img_dither_levels[2] == 0x6 &&
           img_dither_levels[3] == 0xB && img_dither_levels[4] == 0xD && img_dither_levels[5] == 0xF);
    // 端点不抖动：纯黑必须是 0、纯白必须是 F / no dither at the ends
    for (int y = 0; y < 64; ++y) {
        assert(img_dither_gray_to_4bpp(0, y * 3, y) == 0x0);
        assert(img_dither_gray_to_4bpp(255, y * 5, y) == 0xF);
    }
    // 位置确定性：同 (gray,x,y) 永远同值，且与处理顺序无关
    uint8_t a[8];
    for (int i = 0; i < 8; ++i) a[i] = img_dither_gray_to_4bpp(100, 10 + i, 7);
    for (int i = 0; i < 8; ++i) assert(img_dither_gray_to_4bpp(100, 10 + i, 7) == a[i]);
    // 面积守恒：中间灰在 256 个位置上产生的平均码值要接近该灰对应的目标档均值
    long sum = 0;
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) sum += img_dither_gray_to_4bpp(128, x, y);
    double mean = (double)sum / 256.0;
    // 128 落在 0x6(=96/15) 与 0xB(=176/15) 之间：均值必须夹在两者中间
    assert(mean > 6.0 && mean < 11.5);
    // 只输出签字档位 / only the signed-off levels come out
    for (int gray = 0; gray < 256; ++gray) for (int i = 0; i < 64; ++i) {
        uint8_t code = img_dither_gray_to_4bpp((uint8_t)gray, i, gray);
        int hit = 0;
        for (int l = 0; l < IMG_DITHER_LEVELS; ++l) hit |= code == img_dither_levels[l];
        assert(hit);
    }
    // 行接口与逐点一致 / the row entry point matches the per-pixel one
    uint8_t row[8], one;
    const uint8_t src[8] = {10, 60, 110, 160, 200, 230, 250, 5};
    img_dither_row_to_4bpp(src, 3, 9, 8, row);
    for (int i = 0; i < 8; ++i) {
        one = img_dither_gray_to_4bpp(src[i], 3 + i, 9);
        assert(row[i] == one);
    }
    printf("img_dither: ok\n");
    return 0;
}
