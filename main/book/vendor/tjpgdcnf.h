/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：TJpgDec 分块缩小 + 八位灰度直出，跳过色度的反量化与插值，实测比 RGB 档快三成以上。
 * English: TJpgDec block scaling with 8-bit grayscale output; skipping chroma dequantization and
 * upsampling measured 30%+ faster than the RGB config.
 * 冻结：灰度直出，调用方不得按每像素三字节读块；灰度分支的溢出必须已饱和（见 tjpgd.c 的本地修改）。
 * Frozen: grayscale output; callers must not read blocks as three bytes per pixel, and the
 * grayscale path must stay saturated (see the local change in tjpgd.c).
 */
#define JD_SZBUF 512
#define JD_FORMAT 2
#define JD_USE_SCALE 1
#define JD_TBLCLIP 0
#define JD_FASTDECODE 0
#define JD_DEFAULT_HUFFMAN 0
