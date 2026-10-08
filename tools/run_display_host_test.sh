#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
# 编译真实显示出口，只替换硬件边界。/ Compile the production display path, replacing hardware boundaries only.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/book-tests
# 丢掉未引用段的链接选项按链接器分支：GNU ld 认 --gc-sections，macOS 的 ld 只认 -dead_strip。
# Dead-strip is linker-specific: GNU ld takes --gc-sections, macOS ld only understands -dead_strip.
case "$(uname)" in
    Darwin) gc_sections=(-Wl,-dead_strip) ;;
    *) gc_sections=(-Wl,--gc-sections) ;;
esac
cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -ffunction-sections -fdata-sections "${gc_sections[@]}" \
    -Itools/display_host_stubs -Imain -Imain/app \
    tools/display_host_test.c main/display.c -o build/book-tests/display
build/book-tests/display
