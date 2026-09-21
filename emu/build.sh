#!/bin/sh
# Build the HN reader for macOS with host stand-ins for the panel, button and radio.
set -e
cd "$(dirname "$0")/.."
AJ=.pio/libdeps/t5-s3/ArduinoJson/src
LIB=lib/LilyGo-EPD47/src
OUT=emu/build
mkdir -p "$OUT"
# The device font renderer and font tables are compiled from copies so their
# quoted #include "epd_driver.h" resolves to the host shim (runtime panel size)
# instead of the header sitting next to them.
cp "$LIB/font.c" "$LIB/roboto12.h" "$LIB/roboto18.h" "$LIB/roboto18bold.h" "$LIB/roboto13bold.h" "$LIB/roboto8.h" "$LIB/roboto10.h" "$LIB/roboto11.h" "$LIB/roboto13.h" "$LIB/roboto16.h" "$LIB/roboto32.h" "$LIB/firasans.h" "$OUT/"
COMMON="-O1 -g -w -I emu/shim -I $OUT -I $LIB -I src -I emu"
clang -std=c11 $COMMON -c "$OUT/font.c" -o "$OUT/font.o"
clang++ -std=c++17 $COMMON -I "$AJ" \
  -DARDUINOJSON_ENABLE_ARDUINO_STRING=1 -DARDUINOJSON_ENABLE_ARDUINO_STREAM=1 -DARDUINOJSON_ENABLE_ARDUINO_PRINT=1 \
  src/text.cpp src/reader.cpp src/hn.cpp src/store.cpp src/ui.cpp \
  emu/gfx_host.cpp emu/http_host.cpp emu/net_host.cpp emu/button_host.cpp emu/server.cpp \
  "$OUT/font.o" -lcurl -lz -o "$OUT/hn-emu"
echo "built $OUT/hn-emu"
