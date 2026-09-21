#!/bin/sh
# Every string the firmware draws at a fixed x must fit the narrowest page it
# can appear on. Run from anywhere; exits non-zero if one does not.
set -e
cd "$(dirname "$0")/../.."
LIMIT=${1:-492}            # 540 px panel less two 24 px margins
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
clang -std=c11 -w -O1 -I emu/shim -I src -I emu -c src/glyph.c -o "$OUT/glyph.o"
clang++ -std=c++17 -w -O1 -I emu/shim -I src -I emu test/host/fits_main.cpp "$OUT/glyph.o" -o "$OUT/fits"
grep -rhno 'gfx::drawText(fonts::[A-Za-z]*, *"[^"]*"' src/*.cpp \
  | sed 's/^[0-9]*://' \
  | sed 's/gfx::drawText(fonts::\([A-Za-z]*\), *"\(.*\)"$/\1	'"$LIMIT"'	\2/' \
  | "$OUT/fits"
