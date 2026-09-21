# Third-party components

| Component | Where | Licence | Notes |
|---|---|---|---|
| Glyph renderer (`src/glyph.c`) | `src/` | GPL-3.0 | Derived from `font.c` in [Xinyuan-LilyGO/LilyGo-EPD47](https://github.com/Xinyuan-LilyGO/LilyGo-EPD47) (itself from epdiy), with the panel-driver and zlib paths removed. This is why the project as a whole is GPL-3.0. |
| epdiy | `lib/epdiy/` | LGPL-3.0 | Panel driver, as vendored by LilyGo in [T5S3-4.7-e-paper-PRO](https://github.com/Xinyuan-LilyGO/T5S3-4.7-e-paper-PRO) (`epd_board_v7`, ED047TC1). |
| SensorLib | `lib/SensorLib/` | MIT | Lewis He. GT911 touch driver. |
| Roboto | `fonts/` (TTF), `src/fonts/` (generated tables) | Apache-2.0 | Google. Tables generated uncompressed with LilyGo's `fontconvert.py`. |
| ArduinoJson | fetched by PlatformIO | MIT | Not vendored. |
| Algolia HN Search API | network | — | Data source; see <https://hn.algolia.com/api>. |

The board pin map and factory firmware referenced in the README come from
[Xinyuan-LilyGO/T5S3-4.7-e-paper-PRO](https://github.com/Xinyuan-LilyGO/T5S3-4.7-e-paper-PRO).
