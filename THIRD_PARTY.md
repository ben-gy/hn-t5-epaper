# Third-party components

| Component | Where | Licence | Notes |
|---|---|---|---|
| LilyGo-EPD47 (epdiy-derived panel driver, glyph renderer, Fira Sans / Roboto tables) | `lib/LilyGo-EPD47/` | GPL-3.0 | Vendored unmodified from [Xinyuan-LilyGO/LilyGo-EPD47](https://github.com/Xinyuan-LilyGO/LilyGo-EPD47). The emulator compiles its `font.c`; this is why the project as a whole is GPL-3.0. |
| FastEPD | `probe/lib/FastEPD/` | Apache-2.0 | Larry Bank / BitBank Software. Drives the T5 E-Paper S3 Pro panel (`BB_PANEL_LILYGO_T5PRO`). |
| Roboto | `fonts/`, generated tables `lib/LilyGo-EPD47/src/roboto*.h` | Apache-2.0 | Google. Additional sizes and the bold faces were generated with LilyGo's `fontconvert.py`. |
| Fira Sans | `lib/LilyGo-EPD47/src/firasans.h` | SIL OFL 1.1 | Shipped with LilyGo-EPD47. |
| ArduinoJson | fetched by PlatformIO | MIT | Not vendored. |
| Algolia HN Search API | network | — | Data source; see <https://hn.algolia.com/api>. |

The board pin map and factory firmware referenced in the README come from
[Xinyuan-LilyGO/T5S3-4.7-e-paper-PRO](https://github.com/Xinyuan-LilyGO/T5S3-4.7-e-paper-PRO).
