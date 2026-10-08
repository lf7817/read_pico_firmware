#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
# 中文：生成图像回归用的 fixture；彩色与 progressive 两张需要 cjpeg，缺了就跳过并在用例里降级。
# English: Emits image fixtures; the chroma and progressive files need cjpeg, and are skipped without it.
# 冻结：只写 build/，不往仓库里放手造二进制（AGENTS.md：fixture 由脚本生成）。
# Frozen: writes only under build/; no hand-placed binaries in the tree (AGENTS.md).
import os
import shutil
import struct
import subprocess
import sys
import zlib

OUT = "build/img-fixtures"


def write_png(path, width, height, pixel):
    """8-bit grayscale PNG with no filtering, so decoders see a plain scanline stream."""
    raw = b"".join(b"\x00" + bytes(pixel(x, y) for x in range(width)) for y in range(height))

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    with open(path, "wb") as handle:
        handle.write(b"\x89PNG\r\n\x1a\n"
                     + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0))
                     + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))
    return len(raw)


def write_adam7_png(path, width, height, pixel):
    """真正的 Adam7 隔行 PNG：七个 pass 各带自己的行宽与 filter 字节。
    A structurally valid Adam7 PNG: each of the seven passes carries its own row width and filter
    byte, so libpng reads the header happily and the decoder has to be the one that refuses."""

    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    passes = [(0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4), (0, 2, 2, 2), (1, 0, 2, 2),
              (0, 1, 1, 1)]
    raw = b""
    for x0, y0, xs, ys in passes:
        pw = (width - x0 + xs - 1) // xs
        ph = (height - y0 + ys - 1) // ys
        if pw <= 0 or ph <= 0:
            continue
        for py in range(ph):
            row = bytes(pixel(x0 + px * xs, y0 + py * ys) for px in range(pw))
            raw += b"\x00" + row
    with open(path, "wb") as handle:
        handle.write(b"\x89PNG\r\n\x1a\n"
                     + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 1))
                     + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))
    return len(raw)


def write_ppm(path, width, height, rgb):
    with open(path, "wb") as handle:
        handle.write(b"P6\n%d %d\n255\n" % (width, height))
        for y in range(height):
            row = bytearray()
            for x in range(width):
                row += bytes(rgb(x, y))
            handle.write(bytes(row))


def mono(x, y):
    """Luma-only content: a smooth gradient, thin rules, a 2px checker and a noise patch.

    亮度型内容：渐变、发丝线、2px 棋盘和噪声块。
    """
    v = 248 - (128 * y) // 640
    if 200 <= y < 400 and y % 24 < 2:
        v = 18
    if 40 <= x < 360 and 100 <= y < 300:
        v = 255 if ((x >> 1) + (y >> 1)) & 1 else 0
    if 60 <= x < 200 and 440 <= y < 520:
        v = ((x * 73856093) ^ (y * 19349663)) & 0xFF
    return (v, v, v)


def chroma(x, y):
    """Saturated colour blocks with hard edges, so IDCT overshoot and clipping are exercised.

    高彩度硬边色块，专门逼出反冲与饱和截断。
    """
    palette = [(255, 0, 255), (0, 255, 255), (255, 255, 0), (0, 0, 0),
               (255, 128, 0), (128, 0, 255), (0, 255, 128), (255, 255, 255)]
    r, g, b = palette[(y // 80) % len(palette)]
    if (x >> 3) & 1:
        r = min(255, r + 60)
        g = max(0, g - 60)
    if (y % 96) < 12:
        b = 255 if b < 128 else 0
    if y == 320 and 40 <= x < 360:
        r = g = b = 0
    if y == 321 and 40 <= x < 360:
        r = g = b = 255
    if y == 500 and 200 <= x < 210:
        r, g, b = 0, 0, 255
    return (r, g, b)


def cjpeg(src_ppm, dst_jpg, *extra):
    subprocess.run(["cjpeg", "-quality", "90", *extra, "-outfile", dst_jpg, src_ppm], check=True)


def main():
    os.makedirs(OUT, exist_ok=True)
    write_png(os.path.join(OUT, "tiny.png"), 8, 8, lambda x, y: (x * 31 + y * 7) & 0xFF)
    write_png(os.path.join(OUT, "gray160x80.png"), 160, 80, lambda x, y: (x * 3 + y * 5) & 0xFF)
    write_adam7_png(os.path.join(OUT, "interlaced.png"), 16, 8,
                   lambda x, y: (x * 31 + y * 7) & 0xFF)
    write_ppm(os.path.join(OUT, "mono400.ppm"), 400, 640, mono)
    write_ppm(os.path.join(OUT, "chroma400.ppm"), 400, 640, chroma)
    if shutil.which("cjpeg") is None:
        print("cjpeg=no; JPEG fixtures skipped (install libjpeg-turbo)")
        return 0
    cjpeg(os.path.join(OUT, "mono400.ppm"), os.path.join(OUT, "mono400.jpg"))
    cjpeg(os.path.join(OUT, "chroma400.ppm"), os.path.join(OUT, "chroma400.jpg"))
    cjpeg(os.path.join(OUT, "chroma400.ppm"), os.path.join(OUT, "chroma400_prog.jpg"), "-progressive")
    print("fixtures:", ", ".join(sorted(os.listdir(OUT))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
