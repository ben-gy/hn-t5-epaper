#!/bin/sh
# Build the reader-mode extractor for the desktop.  The firmware sources are
# copied next to the host headers so their `#include "app.h"` picks up the
# host stand-in rather than the on-device one.
set -e
d=$(cd "$(dirname "$0")" && pwd)
b=${TMPDIR:-/tmp}/hn_reader_build
rm -rf "$b"; mkdir -p "$b"
cp "$d/Arduino.h" "$d/app.h" "$d/main.cpp" "$b/"
cp "$d/../../src/text.cpp" "$d/../../src/reader.cpp" "$b/"
c++ -std=c++17 -O1 -I "$b" "$b/main.cpp" "$b/text.cpp" "$b/reader.cpp" -o "$b/reader_test"
echo "$b/reader_test"
