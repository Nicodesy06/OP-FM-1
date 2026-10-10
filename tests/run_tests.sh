#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# FM1 Tape OS: every test that runs without hardware.
#   tests/run_tests.sh            host tests (tape, audio, UI) + the WebAssembly build + a Node smoke test
#   WEB=1 tests/run_tests.sh      + the browser end-to-end test (Playwright + Chromium)
#   FM1=1 tests/run_tests.sh      + the FM1 build (JIELI_TOOLCHAIN, see docs/BUILD_FM1.md); nothing is flashed
set -e
cd "$(dirname "$0")/.."
CC="${CC:-cc}"
python3 third_party/felucca/gen_tables.py third_party/felucca/felucca_tables.h >/dev/null
mkdir -p build/test
FAIL=0
for t in tape audio ui; do
    "$CC" -std=gnu99 -O2 -Wall -Wno-unused-function -Wno-unused-const-variable -Isrc -Ithird_party/felucca \
        "tests/test_$t.c" -o "build/test/test_$t" -lm
    "build/test/test_$t" || FAIL=1
done
python3 - <<'PY'
from pathlib import Path
from PIL import Image
for p in sorted(Path("build/test").glob("ui_*.ppm")):
    Image.open(p).save(p.with_suffix(".png"))
print("ui screenshots -> build/test/ui_*.png")
PY
platform/web/build.sh
node - <<'JS' || FAIL=1
const fs = require('fs');
(async () => {
  const { instance } = await WebAssembly.instantiate(fs.readFileSync('build/web/tapeos.wasm'), {});
  const e = instance.exports;
  e.web_init();
  e.web_panel(2, 12, 0);                       // a key down
  let peak = 0;
  for (let i = 0; i < 200; i++) {
    e.web_render(128);
    for (const v of new Float32Array(e.memory.buffer, e.web_out_l(), 128)) peak = Math.max(peak, Math.abs(v));
  }
  const ok = peak > 0.05 && e.web_ms() === 580 && e.web_fb_serial() > 1;
  console.log(`wasm smoke: peak ${peak.toFixed(3)}, ${e.web_ms()} ms, frames ${e.web_fb_serial()}: ${ok ? 'ok' : 'FAIL'}`);
  process.exit(ok ? 0 : 1);
})();
JS
if [ "${WEB:-0}" = 1 ]; then
    python3 -m http.server 8790 -d build/web >/dev/null 2>&1 &
    SRV=$!
    sleep 1
    NODE_PATH="$(npm root -g)" node tests/web_test.cjs http://localhost:8790/ || FAIL=1
    kill "$SRV" 2>/dev/null || true
fi
if [ "${FM1:-0}" = 1 ]; then
    python3 firmware/tools/build.py || FAIL=1
fi
[ "$FAIL" = 0 ] && echo "ALL TESTS PASSED" || { echo "SOME TESTS FAILED"; exit 1; }
