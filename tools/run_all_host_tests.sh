#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
# 全套宿主机回归：tools/run_*.sh 加独立 Python 套件。CI 不跑这些，只有本机跑得起来。
# The whole host regression net: tools/run_*.sh plus the standalone Python harnesses. CI never runs these.
# 在仓库根目录运行。/ Run from the repository root.
# SKIP 不等于通过：它只在缺环境时出现，但必须打出来给人看。
# A SKIP is not a pass; it only appears when the environment lacks a dependency, but it stays printed.
set -uo pipefail

status=0
skips=()

report() {  # report <name> <exit> <output>
    local name=$1 code=$2 out=$3
    if [ "$code" -eq 0 ]; then
        printf 'PASS  %s\n' "$name"
    elif printf '%s' "$out" | grep -q 'SKIP'; then
        printf 'SKIP  %s — %s\n' "$name" "$(printf '%s' "$out" | grep -m1 'SKIP' | cut -c1-120)"
        skips+=("$name")
    else
        printf 'FAIL  %s\n' "$name"
        printf '%s\n' "$out" | tail -5 | sed 's/^/      /'
        status=1
    fi
}

for suite in tools/run_*.sh; do
    [ "$suite" = "tools/run_all_host_tests.sh" ] && continue
    out=$(bash "$suite" 2>&1); report "$suite" "$?" "$out"
done

for harness in tools/test_*.py tools/*_host_test.py tools/transfer_stack_check.py; do
    out=$(python3 "$harness" 2>&1); report "$harness" "$?" "$out"
done

if [ "${#skips[@]}" -gt 0 ]; then
    printf '\n%d 个套件被跳过（缺环境，不是通过）：%s\n' "${#skips[@]}" "${skips[*]}"
    printf '%d suites were skipped for a missing environment — that is not a pass: %s\n' \
        "${#skips[@]}" "${skips[*]}"
fi
exit "$status"
