# Hacker News for the LilyGo T5 E-Paper S3 Pro

A Hacker News reader for the phone-shaped 4.7" e-paper handheld. Read-only by
design — no commenting, voting or login — with everything a reader wants: all
categories, full comment threads you can fold, bookmarks, read/unread
tracking, light and dark themes, and a reader-mode browser for the linked
articles.

It comes with a **desktop emulator**: the real firmware UI compiled for macOS,
drawn by the device's own glyph renderer, against live Hacker News. Every
screen below was captured from it.

<p align="center">
  <img src="docs/screenshots/list.png" width="230" alt="Story list">
  <img src="docs/screenshots/comments.png" width="230" alt="Comment thread">
  <img src="docs/screenshots/categories.png" width="230" alt="Categories">
</p>

## Status

| | |
|---|---|
| Application (UI, feeds, comments, reader mode, bookmarks, settings) | complete, runs in the emulator |
| Panel bring-up on the board (FastEPD `BB_PANEL_LILYGO_T5PRO`) | proven — see `probe/` |
| Device display/touch port of the app | **not yet done** |

`src/gfx.cpp` still targets the plain LilyGo-EPD47 driver, which addresses the
wrong pins on this board. **Do not flash `src/` as it stands.** The next piece
of work is replacing that layer with FastEPD plus a GT911 touch driver; the
emulator exists so that the UI on the other side of that port is already
finished and tested.

## Screenshots

<table>
<tr>
<td align="center"><img src="docs/screenshots/list.png" width="200"><br><sub>Story list — a read story at 40 % ink</sub></td>
<td align="center"><img src="docs/screenshots/comments.png" width="200"><br><sub>Comments</sub></td>
<td align="center"><img src="docs/screenshots/comments_folded.png" width="200"><br><sub>Tap a byline to fold its replies</sub></td>
<td align="center"><img src="docs/screenshots/saved.png" width="200"><br><sub>Saved stories</sub></td>
</tr>
<tr>
<td align="center"><img src="docs/screenshots/categories.png" width="200"><br><sub>Categories</sub></td>
<td align="center"><img src="docs/screenshots/settings.png" width="200"><br><sub>Settings</sub></td>
<td align="center"><img src="docs/screenshots/list_dark.png" width="200"><br><sub>Dark theme</sub></td>
<td align="center"><img src="docs/screenshots/trmnl.png" width="200"><br><sub>Same code on a TRMNL (800×480, 1-bit)</sub></td>
</tr>
</table>

## Run the emulator

Needs clang and the libcurl and libz that ship with macOS. Nothing to install.

```bash
sh emu/build.sh
./emu/build/hn-emu          # optional: ./emu/build/hn-emu 8090
```

Open <http://127.0.0.1:8087>.

- **Tap** stories, toolbar cells and menu rows on the phone screen. On a
  thread or article, tapping the upper half of the page turns back, the lower
  half forward; tapping a comment's byline folds its replies.
- **Size:** *Actual* shows the panel at its physical 235 dpi (it is a
  2.3"-wide screen — judge type sizes there, not at *Fit*).
- **Device:** the T5 Pro (540×960 portrait, 16 greys, touch) or a TRMNL
  (800×480, 1-bit). The layout is resolved at render time from the screen
  size, so it is one codebase laying itself out for either.
- Bookmarks, read history, cached feeds and settings persist in `emu/data/`
  under the working directory. Two minutes idle and it deep-sleeps exactly as
  the device does; any tap wakes it.

The emulator also speaks HTTP (`/frame.bmp`, `/tap?x=&y=`, `/g?k=short|long|double`,
`/dev?d=t5pro|trmnl`, `/info`), which is how the screenshots above were
generated and how the UI was tested.

## The interface

- **Story list** — bold titles at full width, up to three lines, never
  truncated mid-word; one facts line (points · comments · age · domain) that
  drops whole facts from the end when it does not fit; rows sized to their
  content, so a page holds as many stories as fit. Stories you have opened
  render at 40 % ink.
- **Toolbar** — two rows: bookmark · refresh · settings over ▲ · category ▾ · ▼.
  Arrows dim when there is nowhere to go.
- **Comments** — pagination by pixel height rather than by uniform lines, so
  paragraph breaks are a third of a line and comments sit 18 px apart. Reply
  depth is shown by indent; a byline tap folds the subtree and shows the reply
  count. Save, article and back are icons in the toolbar with the page count
  between the arrows.
- **Reader mode** — fetches the story's link and strips it to text: script,
  style, nav, header, footer, aside and form subtrees dropped, narrowed to
  `<article>`/`<main>` when present, headings and lists kept. Pages that build
  their body in JavaScript say so instead of showing a blank page, and offer
  the thread instead.
- **Three reading sizes** (22 / 25 / 30 px lines) from Settings.
- **Dark theme** — a true inversion. The glyph renderer paints each glyph's
  full box between a background and a foreground grey, so both are swapped
  together; a filter would punch white rectangles into the page.

There is no highlight or focus state anywhere: it is a touch device. The
single physical button pages forward (short), back a page (long) and back a
screen (double).

## Hardware

**LilyGo T5 E-Paper S3 Pro** — not the T5 4.7" V2.3, which has a different
pinout (that mistake cost this project one blind flash and a factory restore).

| | |
|---|---|
| MCU | ESP32-S3R8, 16 MB flash, 8 MB PSRAM |
| Panel | ED047TC1 960×540, 16 greys, used portrait as 540×960 |
| Panel power | TPS65185 PMIC behind a PCA9535 I/O expander (not GPIO) |
| Touch | GT911 (INT GPIO3, RST GPIO9) |
| Frontlight | GPIO11, PWM ≤ 1 kHz |
| I²C | GPIO39 / 40, shared by touch, RTC (PCF8563), charger (BQ25896), gauge (BQ27220), PMIC, expander |
| Button | on the PCA9535 (IO1_2); BOOT on GPIO0 |
| Also on board | SD, LoRa (SX1262), GPS |

`probe/` is a PlatformIO sketch that brings the panel up with FastEPD, walks
the I²C bus, reads the fuel gauge, drives the frontlight and reports touches —
the ground truth this port will build on. LilyGo's factory firmware for the
board is in their [T5S3-4.7-e-paper-PRO](https://github.com/Xinyuan-LilyGO/T5S3-4.7-e-paper-PRO)
repository (`firmware/`, flash at 0x0).

## How it is built

| Path | Purpose |
|---|---|
| `src/ui.cpp` | screens, layout, touch targets, folding, paging |
| `src/hn.cpp` | Algolia HN client: one request per feed, one per thread, filtered parse in PSRAM |
| `src/reader.cpp` | reader-mode extraction, working on the raw PSRAM buffer |
| `src/store.cpp` | LittleFS bookmarks / read history / feed cache, NVS settings |
| `src/text.cpp` | Unicode folding to the fonts' ASCII, HTML → text, relative times |
| `src/http.cpp`, `net.cpp`, `button.cpp`, `gfx.cpp` | device I/O (display layer pending the FastEPD port) |
| `emu/` | the macOS build: Arduino/ESP-IDF shims, host display, libcurl, HTTP server, device profiles |
| `test/host/` | desktop harness for the extractor (`sh test/host/run.sh page.html`) |
| `probe/` | FastEPD bring-up sketch for the board |
| `lib/LilyGo-EPD47/` | vendored panel driver and glyph renderer; the emulator uses its `font.c` |
| `fonts/` | Roboto TTFs; sizes were generated with LilyGo's `fontconvert.py` |

Text is measured by glyph advance, not ink bounds, so wrapping is exact.
Rendered text lives in Arduino `String`s, i.e. the 320 KB internal heap, so
threads and articles are capped (60 KB each) with a heap floor; raw bodies and
JSON parsing stay in PSRAM. ArduinoJson's nesting limit is raised to 128 with a
32 KB loop-task stack — HN threads routinely exceed the default of 10.

## Licence

GPL-3.0 — see `LICENSE`. The vendored LilyGo-EPD47 driver and glyph renderer
are GPL-3.0 and the emulator compiles them, so the project follows. FastEPD is
Apache-2.0, Roboto is Apache-2.0, Fira Sans is OFL; see `THIRD_PARTY.md`.
