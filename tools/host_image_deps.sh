# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
# 中文：宿主测试共用的图像管线链接片段。book_epub.c 现在依赖它，所以每个链 book_epub.c 的套件都要 source。
# English: Shared host-test link inputs for the image pipeline. book_epub.c depends on it now, so
# every suite that links book_epub.c must source this file.
# libpng 缺失时相关套件必须明确 SKIP，而不是伪装成 PASS。
# When libpng is missing the affected suites must SKIP loudly instead of pretending to pass.
png_flags=()
for rp_prefix in /opt/homebrew /usr/local /usr; do
    [ -f "$rp_prefix/include/png.h" ] && png_flags=(-I"$rp_prefix/include" -L"$rp_prefix/lib") && break
done
png_ready=1
[ ${#png_flags[@]} -eq 0 ] && png_ready=0
image_pipeline=(main/book/img_plane.c main/book/img_plane_build.c main/book/img_scale.c
                main/book/img_dither.c main/book/img_decode.c main/book/img_png.c main/book/vendor/tjpgd.c)
image_includes=(-Itools/book_layout_stubs -Itools/book_image_stubs)
