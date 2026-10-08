#!/usr/bin/env python3
# 生成 64x64 blue noise 阈值面，输出头文件；不要手改产物。
# Generates the 64x64 blue-noise threshold map as a header; never edit the output by hand.
#
# 中文：定序用"最远点采样"（maximal-distance ordering）：每次加入离已选点集最远的那一格，
#       得到 0..4095 的一个排列，再线性映射到 0..255 阈值。前 k 格在环面上尽量互相远离，
#       这正是有序抖动要的谱特性。距离按环面算，因为阈值面会平铺，边界不许起接缝。
# English: Ordering by farthest-point sampling — repeatedly add the cell farthest from the chosen
#          set, yielding a permutation of 0..4095 mapped linearly onto thresholds 0..255. Toroidal
#          distance, because the map tiles and must not seam at the border.
#
# 计划稿里的 void-and-cluster 写法有 bug：阶段一在 512 格 break，剩下 3072 格拿不到 rank。
# The plan's void-and-cluster snippet was broken: it broke out of phase one at 512 cells, leaving
# 3072 of 4096 without a rank.
import random
import sys

SIDE, SEED = 64, 20261008
OUT = "main/assets/blue_noise_64.h"
N = SIDE * SIDE


def main():
    rng = random.Random(SEED)
    # 环面平方距离按轴预计算 / per-axis toroidal squared distance, precomputed
    axis = [min(d, SIDE - d) ** 2 for d in range(SIDE)]
    tie = rng.sample(range(N), N)  # 同分时的定序，保证可复现 / deterministic tiebreak order

    nearest = [1 << 30] * N
    taken = [False] * N
    order = []
    first = rng.randrange(N)
    stack = [first]

    def absorb(index):
        x, y = index % SIDE, index // SIDE
        row = axis
        for cy in range(SIDE):
            dy = row[(cy - y) % SIDE]
            base = cy * SIDE
            for cx in range(SIDE):
                v = dy + row[(cx - x) % SIDE]
                k = base + cx
                if v < nearest[k]:
                    nearest[k] = v

    absorb(first)
    taken[first] = True
    order.append(first)
    while len(order) < N:
        best, best_key = -1, -1
        for i in range(N):
            if taken[i]:
                continue
            if nearest[i] > best or (nearest[i] == best and tie[i] > best_key):
                best, best_key, pick = nearest[i], tie[i], i
        absorb(pick)
        taken[pick] = True
        order.append(pick)
        # 每 512 格报一次进度，纯 Python 这趟要跑几十秒 / progress line; pure Python takes tens of seconds
        if len(order) % 1024 == 0:
            print("ordered %d/%d" % (len(order), N), file=sys.stderr)

    rank = [0] * N
    for i, cell in enumerate(order):
        rank[cell] = i
    values = [rank[y * SIDE + x] * 256 // N for y in range(SIDE) for x in range(SIDE)]
    for v in range(256):
        assert values.count(v) == 16, "value %d appears %d times, expected 16" % (v, values.count(v))

    # 质量自检：前 k 格的最近邻平均距离要明显好于随机同规模子集。
    # Quality gate: mean nearest-neighbour distance among the first k ranks must beat a random subset.
    def nn_mean(cells):
        s = set(cells)
        total = 0
        for c in cells:
            x, y = c % SIDE, c // SIDE
            best = 1 << 30
            for o in cells:
                if o == c:
                    continue
                ox, oy = o % SIDE, o // SIDE
                d = axis[(ox - x) % SIDE] + axis[(oy - y) % SIDE]
                if d < best:
                    best = d
            total += best ** 0.5
        assert len(s) == len(cells)
        return total / len(cells)

    probe = 256
    blue = nn_mean(order[:probe])
    rnd = nn_mean(rng.sample(range(N), probe))
    print("nearest-neighbour mean over the first %d ranks: blue %.2f px vs random %.2f px"
          % (probe, blue, rnd), file=sys.stderr)
    assert blue > rnd, "the map is no bluer than random; check the ordering"

    with open(OUT, "w", encoding="utf-8") as f:
        f.write("/*\n")
        f.write(" * SPDX-FileCopyrightText: 2026 mindreset\n")
        f.write(" * SPDX-License-Identifier: CC0-1.0\n")
        f.write(" *\n")
        f.write(" * 中文：blue noise 阈值面，最远点采样生成（tools/gen_blue_noise.py，种子 %d）。\n" % SEED)
        f.write(" * English: Blue-noise threshold map from farthest-point sampling "
                "(tools/gen_blue_noise.py, seed %d).\n" % SEED)
        f.write(" * 冻结：生成物，勿手改；换算法要同步 IMG_DITHER_VERSION。\n")
        f.write(" * Frozen: generated; do not hand-edit. Bump IMG_DITHER_VERSION when the map changes.\n")
        f.write(" */\n#pragma once\n#include <stdint.h>\n")
        f.write("static const uint8_t img_blue_noise[64][64] = {\n")
        for y in range(SIDE):
            row = ", ".join("%3d" % values[y * SIDE + x] for x in range(SIDE))
            f.write("    { %s }%s\n" % (row, "," if y < SIDE - 1 else ""))
        f.write("};\n")
    print("wrote %s (%d values, 0..255 each exactly 16x)" % (OUT, N), file=sys.stderr)


main()
