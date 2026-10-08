# stb_image

`stb_image.h` v2.30 is an unmodified upstream file from
[nothings/stb](https://github.com/nothings/stb/blob/master/stb_image.h).
Its MIT/public-domain dual-license notice is retained at the end of the header.
`book_image.c` enables JPEG/PNG only and supplies a bounded PSRAM allocator.

`stb_image.h` v2.30 为未修改的上游文件，文件末尾保留 MIT/公共领域双许可声明。
`book_image.c` 仅启用 JPEG/PNG，并提供有总量限制的 PSRAM 分配器。

`tjpgd.c` and `tjpgd.h` are unmodified upstream TJpgDec R0.03 sources from
[Espressif esp_jpeg](https://github.com/espressif/idf-extra-components/tree/master/esp_jpeg/tjpgd).
ChaN's original redistribution notice is retained in `tjpgd.c`.
`tjpgdcnf.h` is the local configuration: 8-bit grayscale output, scaling enabled,
512-byte input buffer, optimization level 0, no default Huffman tables.

`tjpgd.c` 与 `tjpgd.h` 保留上游原文和 ChaN 的再分发声明。
`tjpgdcnf.h` 是本地配置：八位灰度输出、启用缩小、512字节输入缓冲、零级优化、不补缺失的霍夫曼表。

The caller converts saturated RGB blocks to grayscale. This avoids luminance
wraparound in the upstream grayscale path with optimization level 1.
调用方将已饱和的RGB块转灰度，避免上游一级优化灰度路径中的亮度溢出回绕。

上面这条规避**仍然必要，但它的归因是错的**：`tools/test_tjpgd_gray_config.py` 用一张故意让 DC 冲过头的
夹具量过，`JD_FORMAT 2` 在 `JD_FASTDECODE` 0 与 1、`JD_TBLCLIP` 0 与 1 四种组合下都把溢出值截断成
`7`，而 libjpeg-turbo 的参照是饱和到 `255`；RGB 档两种优化级别都出 `255`。
所以回绕与优化级别无关，也不受 `JD_TBLCLIP` 控制——它是灰度直出分支本身按字节截断。
在给出该分支的本地饱和补丁之前，**不要切到 `JD_FORMAT 2`**（即便它快 30~43%）。
This workaround is still required, but its stated cause was wrong: the grayscale path truncates
overflowing luminance at *both* optimization levels and regardless of `JD_TBLCLIP`, while libjpeg
saturates. Do not switch to `JD_FORMAT 2` (even though it measured 30~43% faster) until that branch
carries a local saturation patch.
