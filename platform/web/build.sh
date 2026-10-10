#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build the browser simulator: the shared C application -> WebAssembly (clang, wasm32, no libc),
# plus the page.   platform/web/build.sh  ->  build/web/{index.html, app.js, worklet.js, tapeos.wasm}
# Needs: clang with the wasm32 target and wasm-ld (Ubuntu: clang lld), python3.
set -e
cd "$(dirname "$0")/../.."
CC="${CC:-clang}"
python3 third_party/felucca/gen_tables.py third_party/felucca/felucca_tables.h >/dev/null
mkdir -p build/web
"$CC" --target=wasm32 -O2 -std=gnu99 -ffreestanding -fno-builtin -nostdlib -mbulk-memory \
    -Wall -Wno-unused-function -Wno-unused-const-variable \
    -Isrc -Ithird_party/felucca \
    -Wl,--no-entry -Wl,-z,stack-size=262144 -Wl,--initial-memory=2097152 \
    -o build/web/tapeos.wasm platform/web/web.c
cp web/index.html web/app.js web/worklet.js web/style.css build/web/
echo "web: build/web ($(wc -c < build/web/tapeos.wasm | tr -d ' ') B wasm)"
