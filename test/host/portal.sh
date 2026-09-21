#!/bin/sh
# Renders the Wi-Fi setup screen at both panel sizes and checks that nothing
# crosses the right margin. It is the first screen a new device shows and the
# one screen the emulator cannot reach, so it is worth looking at directly.
set -e
cd "$(dirname "$0")/../.."
OUT=${1:-emu/build}
mkdir -p "$OUT"
AJ=$(ls -d .pio/libdeps/*/ArduinoJson/src 2>/dev/null | head -1)
[ -n "$AJ" ] || { echo "ArduinoJson not fetched yet - run: pio run -e t5pro" >&2; exit 1; }
clang -std=c11 -w -O1 -I emu/shim -I src -I emu -c src/glyph.c -o "$OUT/glyph.o"
clang++ -std=c++17 -w -O1 -I emu/shim -I src -I emu -I "$AJ" \
  test/host/portal_main.cpp src/portal_ui.cpp emu/gfx_host.cpp "$OUT/glyph.o" -o "$OUT/portal"
( cd "$OUT" && ./portal portal )
echo "wrote $OUT/portal_t5pro.bmp and $OUT/portal_trmnl.bmp"
