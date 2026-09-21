// Glyph renderer for a 4-bit-per-pixel framebuffer, shared by the device and
// the desktop emulator. Derived from LilyGo-EPD47's font.c (itself from
// epdiy); GPL-3.0. The framebuffer's size is a runtime value so the same
// code draws the phone-shaped portrait screen and other devices.
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int epd_fb_width;    // logical framebuffer size in pixels
extern int epd_fb_height;
#define EPD_WIDTH  epd_fb_width
#define EPD_HEIGHT epd_fb_height

typedef struct { int32_t x, y, width, height; } Rect_t;

typedef enum {
    BLACK_ON_WHITE = 1 << 0,
    WHITE_ON_WHITE = 1 << 1,
    WHITE_ON_BLACK = 1 << 2,
} DrawMode_t;

enum DrawFlags { DRAW_BACKGROUND = 1 << 0 };

typedef struct {
    uint8_t  fg_color : 4;
    uint8_t  bg_color : 4;
    uint32_t fallback_glyph;
    uint32_t flags;
} FontProperties;

typedef struct {
    uint8_t  width, height, advance_x;
    int16_t  left, top;
    uint16_t compressed_size;
    uint32_t data_offset;
} GFXglyph;

typedef struct { uint32_t first, last, offset; } UnicodeInterval;

typedef struct {
    uint8_t         *bitmap;
    GFXglyph        *glyph;
    UnicodeInterval *intervals;
    uint32_t         interval_count;
    bool             compressed;     // must be false: bitmaps are stored raw
    uint8_t          advance_y;
    int32_t          ascender;
    int32_t          descender;
} GFXfont;

void get_text_bounds(const GFXfont *font, const char *string, int32_t *x, int32_t *y,
                     int32_t *x1, int32_t *y1, int32_t *w, int32_t *h,
                     const FontProperties *props);
void writeln(const GFXfont *font, const char *string, int32_t *cursor_x, int32_t *cursor_y,
             uint8_t *framebuffer);
void write_mode(const GFXfont *font, const char *string, int32_t *cursor_x, int32_t *cursor_y,
                uint8_t *framebuffer, DrawMode_t mode, const FontProperties *properties);
void write_string(const GFXfont *font, const char *string, int32_t *cursor_x, int32_t *cursor_y,
                  uint8_t *framebuffer);
void get_glyph(const GFXfont *font, uint32_t code_point, GFXglyph **glyph);

#ifdef __cplusplus
}
#endif
