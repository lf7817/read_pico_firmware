#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
# 在仓库根目录运行；产物仅在build。/ Run from the repository root; artifacts stay in build.
# EPUB主机解压替身需要zlib开发包。/ The EPUB host inflate shim requires zlib development headers.
set -euo pipefail
mkdir -p build/book-tests
python tools/gen_book_fixtures.py
python tools/gen_image_fixtures.py > /dev/null   # img_decode 的 JPEG 夹具 / fixtures for img_decode
flags=(-std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer -Imain/book)
source tools/host_image_deps.sh
# 丢掉未引用段的链接选项按链接器分支：GNU ld 认 --gc-sections，macOS 的 ld 只认 -dead_strip。
# Dead-strip is linker-specific: GNU ld takes --gc-sections, macOS ld only understands -dead_strip.
case "$(uname)" in
    Darwin) gc_sections=(-Wl,-dead_strip) ;;
    *) gc_sections=(-Wl,--gc-sections) ;;
esac
gcc "${flags[@]}" -Itools/book_epub_stubs -Itools/zip_host_stubs -Itools/book_layout_stubs -Itools/book_image_stubs \
  "${png_flags[@]}" tools/book_source_host_test.c main/book/book_source.c main/book/book_txt.c main/book/gbk.c main/book/book_epub.c main/book/zip_reader.c main/book/html_text.c main/book/book_image.c "${image_pipeline[@]}" tools/test_display_stub.c -lpng -lz -o build/book-tests/source
# book_layout 会搬平面，所以链平面存储与缩放抖动；解码器不在其中（img_plane_build 才牵扯 libpng）。
# book_layout blits planes, so it links plane storage plus scale and dither; the decoders are
# not in that set - only img_plane_build pulls in libpng.
gcc "${flags[@]}" -Itools/book_layout_stubs -Itools/book_source_host_stubs -Itools/book_image_stubs \
  tools/book_layout_host_test.c main/book/book_layout.c main/book/img_plane.c main/book/img_scale.c main/book/img_dither.c -o build/book-tests/layout
gcc "${flags[@]}" -Itools/book_storage_stubs tools/book_storage_host_test.c main/book/book_progress.c -o build/book-tests/progress
gcc "${flags[@]}" -Itools/book_storage_stubs -Icomponents/read_pico/include tools/book_store_host_test.c -o build/book-tests/store
gcc "${flags[@]}" tools/book_policy_host_test.c -o build/book-tests/policy
build/book-tests/source build/book-fixtures/books/*.txt build/book-fixtures/books/*.epub
build/book-tests/layout
build/book-tests/progress
build/book-tests/store
build/book-tests/policy
gcc "${flags[@]}" -Itools/ui_gesture_stubs -Imain/ui tools/ui_gesture_host_test.c main/ui/ui_gesture.c -o build/book-tests/gesture
gcc "${flags[@]}" -ffunction-sections -fdata-sections "${gc_sections[@]}" -Itools/ui_gesture_stubs -Imain/ui tools/ui_kit_host_test.c main/ui/ui_kit.c -o build/book-tests/ui_kit
build/book-tests/gesture
build/book-tests/ui_kit
gcc "${flags[@]}" -Itools/html_text_stubs -Itools/book_layout_stubs -Itools/book_image_stubs \
  tools/html_text_host_test.c main/book/html_text.c main/book/img_plane.c tools/test_display_stub.c \
  -o build/book-tests/html
build/book-tests/html
gcc "${flags[@]}" tools/img_dither_host_test.c main/book/img_dither.c -o build/book-tests/img_dither
build/book-tests/img_dither
gcc "${flags[@]}" tools/img_scale_host_test.c main/book/img_scale.c -o build/book-tests/img_scale
build/book-tests/img_scale
# 对照实现在测试内部（同一档 TJpgDec 写整图），所以这里不链 book_image.c。
# The reference lives inside the test, so book_image.c is not linked here.
gcc "${flags[@]}" -Itools/book_image_stubs tools/img_decode_host_test.c main/book/img_decode.c main/book/vendor/tjpgd.c -o build/book-tests/img_decode
build/book-tests/img_decode
# reader 桥在 img_decode.c 里，所以连它和它要的 TJpgDec 一起链进来。
# The reader bridge lives in img_decode.c, so link it and the TJpgDec it needs.
gcc "${flags[@]}" -Itools/book_image_stubs "${png_flags[@]}" tools/img_png_host_test.c \
  main/book/img_png.c main/book/img_decode.c main/book/vendor/tjpgd.c -lpng \
  -o build/book-tests/img_png
build/book-tests/img_png
# img_plane 用堆跟踪桩量峰值，所以走 book_epub_stubs 的 esp_heap_caps.h（宏接管 malloc/free）。
# img_plane measures its peak through the tracking shim, so it takes book_epub_stubs'
# esp_heap_caps.h, which shadows malloc/free by macro.
gcc "${flags[@]}" -DBOOK_HEAP_TRACK -Itools/book_epub_stubs -Itools/book_image_stubs \
  -Itools/book_layout_stubs "${png_flags[@]}" tools/img_plane_host_test.c main/book/img_plane.c main/book/img_plane_build.c \
  main/book/img_scale.c \
  main/book/img_dither.c main/book/img_decode.c main/book/img_png.c main/book/vendor/tjpgd.c \
  tools/book_heap_track.c -lpng -lz -o build/book-tests/img_plane
build/book-tests/img_plane
# thumb_cache 链进整条图像管线，所以它需要与 img_plane 相同的平台替身。
# thumb_cache links the whole image pipeline, so it needs the same platform shims as img_plane.
gcc "${flags[@]}" "${png_flags[@]}" -Itools/book_image_stubs -Itools/book_layout_stubs \
  tools/thumb_cache_host_test.c main/book/thumb_cache.c \
  main/book/img_plane.c main/book/img_scale.c main/book/img_dither.c main/book/img_decode.c \
  main/book/img_png.c main/book/vendor/tjpgd.c -lpng -lz -o build/book-tests/thumb_cache
build/book-tests/thumb_cache
gcc "${flags[@]}" tools/book_cover_host_test.c main/book/book_cover.c -o build/book-tests/book_cover
build/book-tests/book_cover
if [ "$png_ready" = 0 ]; then
    echo "SKIP: img_png, img_plane and thumb_cache need libpng development headers (png.h)"
    exit 1
fi
