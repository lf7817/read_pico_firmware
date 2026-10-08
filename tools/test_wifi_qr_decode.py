# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
# 用独立解码器校验生产代码生成的240px二维码。/ Independently decode production-generated 240px QR images.
from pathlib import Path
import sys

try:
    from PIL import Image
    import zxingcpp
except ImportError as missing:
    # 缺解码依赖时这份校验跑不了；生成侧回归在 run_wifi_qr_host_test.sh 里，仍然必跑。
    # Without a decoder this check cannot run; the generation regression lives in
    # run_wifi_qr_host_test.sh and stays mandatory.
    raise SystemExit(f"SKIP: needs {missing.name} (pip install pillow zxing-cpp) to decode the QR")

samples = {
    "url-ap": "http://192.168.4.1",
    "url-sta": "http://192.168.123.234",
    "normal": "WIFI:T:WPA;S:ReadPico-5945;P:readpico;;",
    "escaped": 'WIFI:T:WPA;S:Pico\\;\\,\\:\\\\\\";P:pass\\;\\,\\:\\\\\\";;',
}
for name, expected in samples.items():
    path = Path("build") / f"wifi-qr-{name}.pgm"
    picture = Image.open(path)
    result = zxingcpp.read_barcode(picture)
    assert result is not None and result.text == expected, name
    assert picture.size == (240, 240)
    print(f"{name}: independent QR decode passed")
