#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
# 中文：同一张彩度图在四种 TJpgDec 配置下的亮度分布，用来结案"灰度路径会亮度回绕"这条声明。
# English: Luminance distribution of one chroma image across four TJpgDec configs, settling the
# claimed grayscale-path luminance wraparound.
# 冻结：只测量并打印，不改仓库里的 tjpgdcnf.h；换配置由人看完表再决定。
# Frozen: measures and prints only; it never edits the in-tree tjpgdcnf.h. A human flips the
# config after reading the table.
import os
import re
import shutil
import subprocess
import sys

VENDOR = "main/book/vendor"
FIXTURE = "build/img-fixtures/chroma400.jpg"
REF = "build/img-fixtures/chroma400-gray.pgm"
WORK = "build/tjpgd-cfg"

PROBE_C = r'''
/* Reports the luminance distribution TJpgDec produces for one JPEG. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tjpgd.h"

static unsigned char *g_data;
static size_t g_len, g_at;
static unsigned long g_width;
static unsigned long long g_sum, g_weighted;
static unsigned long g_min, g_max, g_n;

static size_t in_(JDEC *jd, uint8_t *out, size_t n) {
    (void)jd;
    if (n > g_len - g_at) n = g_len - g_at;
    if (out) memcpy(out, g_data + g_at, n);
    g_at += n;
    return n;
}

static int out_(JDEC *jd, void *bitmap, JRECT *r) {
    (void)jd;
    unsigned char *p = (unsigned char *)bitmap;
    unsigned wide = (unsigned)(r->right - r->left + 1);
    for (unsigned y = r->top; y <= r->bottom; ++y) {
        for (unsigned x = 0; x < wide; ++x) {
            unsigned long v;
#if JD_FORMAT == 2
            v = p[(size_t)(y - r->top) * wide + x];
#else
            const unsigned char *c = p + ((size_t)(y - r->top) * wide + x) * 3;
            v = (77u * c[0] + 150u * c[1] + 29u * c[2]) >> 8;
#endif
            if (v < g_min) g_min = v;
            if (v > g_max) g_max = v;
            g_sum += v;
            g_weighted += (unsigned long long)v * ((unsigned long long)y * g_width + r->left + x + 1);
            ++g_n;
        }
    }
    return 1;
}

int main(int argc, char **argv) {
    FILE *f;
    (void)argc;
    if (argc < 2) return 2;
    f = fopen(argv[1], "rb");
    if (!f) return 2;
    fseek(f, 0, SEEK_END);
    g_len = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    g_data = malloc(g_len);
    if (!g_data || fread(g_data, 1, g_len, f) != g_len) return 2;
    fclose(f);
    void *work = malloc(8192);
    static JDEC jd;
    if (!work || jd_prepare(&jd, in_, work, 8192, 0) != JDR_OK) return 3;
    g_width = jd.width;
    g_min = 255;
    if (jd_decomp(&jd, out_, 0) != JDR_OK) return 4;
    printf("%lu %lu %.6f %llu %llu\n", g_min, g_max, (double)g_sum / (double)g_n,
           g_sum, g_weighted);
    return 0;
}
'''


def build_config(fmt, fast):
    """Compile a probe against a copy of the vendored TJpgDec with the requested config.

    按指定配置编译一份探针；仓库里的 tjpgdcnf.h 不动。
    """
    tag = "f%d_d%d" % (fmt, fast)
    out = os.path.join(WORK, tag)
    os.makedirs(out, exist_ok=True)
    for name in ("tjpgd.c", "tjpgd.h"):
        shutil.copyfile(os.path.join(VENDOR, name), os.path.join(out, name))
    conf = open(os.path.join(VENDOR, "tjpgdcnf.h"), encoding="utf-8").read()
    conf = re.sub(r"#define JD_FORMAT \d", "#define JD_FORMAT %d" % fmt, conf)
    conf = re.sub(r"#define JD_FASTDECODE \d", "#define JD_FASTDECODE %d" % fast, conf)
    with open(os.path.join(out, "tjpgdcnf.h"), "w", encoding="utf-8") as handle:
        handle.write(conf)
    with open(os.path.join(out, "probe.c"), "w", encoding="utf-8") as handle:
        handle.write(PROBE_C)
    exe = os.path.join(out, "probe")
    subprocess.run(["cc", "-std=c11", "-O1", "-Wall", "-Wextra", "-Werror", "-I", out,
                    os.path.join(out, "probe.c"), os.path.join(out, "tjpgd.c"), "-o", exe],
                   check=True)
    return exe


def measure(exe, path):
    fields = subprocess.run([exe, path], capture_output=True, text=True, check=True).stdout.split()
    return {"min": int(fields[0]), "max": int(fields[1]), "mean": float(fields[2]),
            "sum": int(fields[3]), "weighted": int(fields[4])}


def reference_gray(fixture=FIXTURE, cache=REF):
    """Ground truth Y from libjpeg-turbo, or None when djpeg is unavailable.

    libjpeg-turbo 的 Y 作为参照；没有 djpeg 就返回 None。
    """
    if shutil.which("djpeg") is None:
        return None
    with open(cache, "wb") as handle:
        subprocess.run(["djpeg", "-gray", "-ppm", fixture], stdout=handle, check=True)
    parts = open(cache, "rb").read().split(b"\n", 3)
    assert parts[0] == b"P5", parts[0]
    width, height = (int(v) for v in parts[1].split())
    body = parts[3]
    assert len(body) >= width * height, (len(body), width, height)
    pixels = list(body[:width * height])
    total = sum(pixels)
    weighted = 0
    for index, v in enumerate(pixels):
        weighted += v * (index + 1)
    return {"min": min(pixels), "max": max(pixels), "mean": total / len(pixels),
            "sum": total, "weighted": weighted}


def overshoot_fixture():
    """Reuse the hand-built DC-overshoot JPEG from the image suite; it forces Y out of range.

    复用图像套件里那个手搓的 DC 冲过头夹具，它专门把 Y 顶出合法范围。
    """
    target = "build/img-fixtures/overshoot.jpg"
    if os.path.exists(target):
        return target
    source = open("tools/test_book_images.py", encoding="utf-8").read().split("def main")[0]
    namespace = {"__file__": os.path.abspath("tools/test_book_images.py")}
    exec(source, namespace)
    os.makedirs("build/img-fixtures", exist_ok=True)
    with open(target, "wb") as handle:
        handle.write(namespace["gray_jpeg"](16, 8, overshoot=True))
    return target


def gray_is_truncating():
    """Characterisation test: the grayscale path truncates, the shipped RGB path saturates.

    现状刻画：灰度直出会截断溢出亮度，RGB 档会饱和。哪天上游或本地补丁改了这条，本用例就会失败并
    提醒重新评估 `JD_FORMAT 2`；在此之前它就是 11.4 里那个闸门的看门人。
    """
    fixture = overshoot_fixture()
    reference = reference_gray(fixture, "build/img-fixtures/overshoot-gray.pgm")
    if reference is None:
        print("SKIP: djpeg unavailable, cannot characterise the grayscale truncation")
        return True
    rgb = measure(build_config(0, 1), fixture)
    gray = measure(build_config(2, 0), fixture)
    print("\nDC-overshoot fixture (reference Y = %d): RGB mean %.1f, grayscale mean %.1f"
          % (reference["mean"], rgb["mean"], gray["mean"]))
    assert abs(rgb["mean"] - reference["mean"]) < 1.0, \
        "the shipped RGB path no longer saturates like the reference: %s vs %s" % (rgb, reference)
    assert gray["mean"] < reference["mean"] - 100, \
        "grayscale output now saturates; re-evaluate JD_FORMAT 2 (see spec 11.4)"
    print("PASS: grayscale output still truncates overflowing luminance, so JD_FORMAT stays 0")
    return True


def main():
    os.makedirs(WORK, exist_ok=True)
    if not os.path.exists(FIXTURE):
        subprocess.run([sys.executable, "tools/gen_image_fixtures.py"], check=True)
    results = {}
    for fmt, fast in ((2, 0), (2, 1), (0, 0), (0, 1)):
        results[(fmt, fast)] = measure(build_config(fmt, fast), FIXTURE)
    ref = reference_gray()
    print("%-10s %5s %5s %9s %12s %18s" % ("config", "min", "max", "mean", "sum", "weighted"))
    for (fmt, fast), r in results.items():
        print("F%d D%-8d %5d %5d %9.4f %12d %18d" % (fmt, fast, r["min"], r["max"],
                                                      r["mean"], r["sum"], r["weighted"]))
    if ref:
        print("djpeg      %5d %5d %9.4f %12d %18d   <- reference Y" % (ref["min"], ref["max"],
                                                                       ref["mean"], ref["sum"],
                                                                       ref["weighted"]))
    gray = results[(2, 0)]
    gray_fast = results[(2, 1)]
    shipped = results[(0, 1)]
    print("\ngray(F2D0) vs fast-gray(F2D1): mean %+.4f (%.2f%%)" % (
        gray_fast["mean"] - gray["mean"],
        100.0 * (gray_fast["mean"] - gray["mean"]) / gray["mean"]))
    print("shipped(F0D1 RGB->gray) vs gray(F2D0): mean %+.4f (%.2f%%)" % (
        shipped["mean"] - gray["mean"],
        100.0 * (shipped["mean"] - gray["mean"]) / gray["mean"]))
    if ref:
        delta = abs(gray["mean"] - ref["mean"])
        wdelta = 100.0 * (gray["weighted"] - ref["weighted"]) / ref["weighted"]
        print("gray(F2D0) vs reference: mean delta %.4f, weighted delta %+.3f%%" % (delta, wdelta))
        assert delta < 1.0, "grayscale output is not reference Y: mean delta %.4f" % delta
        assert abs(wdelta) < 1.0, "grayscale output differs locally: weighted %+.3f%%" % wdelta
        assert gray["min"] <= ref["min"] + 1 and gray["max"] >= ref["max"] - 1, \
            "grayscale output clipped away the extremes: %s vs %s" % (gray, ref)
        print("PASS: JD_FORMAT 2 matches the reference Y while values stay in range; "
              "the DC-overshoot case below is the blocker")
    else:
        print("SKIP: djpeg unavailable, so the gray config is unverified against a reference")
    gray_is_truncating()
    return 0


if __name__ == "__main__":
    sys.exit(main())
