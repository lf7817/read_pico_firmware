# stb_image

`stb_image.h` v2.30 is an unmodified upstream file from
[nothings/stb](https://github.com/nothings/stb/blob/master/stb_image.h).
Its MIT/public-domain dual-license notice is retained at the end of the header.
`book_image.c` enables JPEG/PNG only and supplies a bounded PSRAM allocator.

`stb_image.h` v2.30 为未修改的上游文件，文件末尾保留 MIT/公共领域双许可声明。
`book_image.c` 仅启用 JPEG/PNG，并提供有总量限制的 PSRAM 分配器。

`tjpgd.h` is an unmodified upstream TJpgDec R0.03 header from
[Espressif esp_jpeg](https://github.com/espressif/idf-extra-components/tree/master/esp_jpeg/tjpgd).
ChaN's original redistribution notice is retained in `tjpgd.c`.

`tjpgd.c` carries **one local change**: the DC-only fast path in `mcu_load` clamps the block
luminance to 0..255 before storing it as `jd_yuv_t`. Upstream casts it straight, so a block whose
DC value overflows (e.g. 263) becomes 7 in the uint8 storage, or reaches the grayscale output
unclamped at higher fast-decode levels and is truncated there. Saturation is what libjpeg does.
`tjpgd.c` 有**一处本地修改**：`mcu_load` 里"无 AC 系数就跳过 IDCT"的快路径在写入 `jd_yuv_t` 前把亮度
钳到 0..255。上游直接转型，溢出块（如 263）在零级优化下按 uint8 变成 7，或在一级优化下带着 263
到灰度输出才被截断；libjpeg 的行为是饱和。

`tjpgdcnf.h` is the local configuration: 8-bit grayscale output, scaling enabled, 512-byte input
buffer, optimization level 0, no default Huffman tables. Grayscale output skips chroma
dequantization and upsampling, measured 30~43% faster than the previous RGB config.
`tjpgdcnf.h` 是本地配置：八位灰度输出、启用缩小、512 字节输入缓冲、零级优化、不补缺失的霍夫曼表。
灰度输出跳过色度的反量化与插值，实测比原来的 RGB 档快 30~43%。

历史上这里写过「调用方将已饱和的 RGB 块转灰度，以避免一级优化灰度路径的亮度溢出回绕」。那条归因是错的
（回绕与优化级别无关，也不受 `JD_TBLCLIP` 控制，根因就是上面那处缺钳位），且已由本地修改修好，故改回灰度
直出。`tools/test_tjpgd_gray_config.py` 用一张故意让 DC 冲过头的夹具钉住这个结论，四档配置都要过。
The previous note blamed a grayscale wraparound on optimization level 1 and worked around it by
decoding RGB; that attribution was wrong (the missing clamp was the cause) and the workaround is
gone. The host test pins the fixed behaviour across all four configs.
