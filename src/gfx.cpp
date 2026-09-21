#include "app.h"
#include <esp_heap_caps.h>
#include <esp_adc_cal.h>
// Fonts are only pulled in here so the glyph tables exist exactly once.
#include "roboto12.h"
#include "roboto18.h"
#include "roboto32.h"
#include "firasans.h"
#include "roboto18bold.h"
#include "roboto13bold.h"
#include "roboto8.h"
#include "roboto10.h"
#include "roboto11.h"
#include "roboto13.h"
#include "roboto16.h"

namespace fonts {
    const GFXfont *sm = &Roboto12;   // advance_y 29
    const GFXfont *md = &Roboto18;   // advance_y 44
    const GFXfont *lg = &FiraSans;   // advance_y 50
    const GFXfont *xl = &Roboto32;   // advance_y 78
    const GFXfont *title = &Roboto13;          // metrics for title rows
    const GFXfont *titleBold = &Roboto13Bold;
    const GFXfont *meta = &Roboto10;
    const GFXfont *bodyS = &Roboto8;    // 22 px line
    const GFXfont *bodyM = &Roboto10;   // 25
    const GFXfont *bodyL = &Roboto11;   // 30
}

namespace gfx {

uint8_t *fb = nullptr;
static bool darkMode = false;
static uint8_t *regionBuf = nullptr;
static const size_t FB_BYTES = (size_t)EPD_WIDTH / 2 * EPD_HEIGHT;
static bool powered = false;
static int  partialCount = 0;

bool begin() {
    epd_init();
    fb = (uint8_t *)heap_caps_malloc(FB_BYTES, MALLOC_CAP_SPIRAM);
    regionBuf = (uint8_t *)heap_caps_malloc(FB_BYTES, MALLOC_CAP_SPIRAM);
    if (!fb || !regionBuf) return false;
    clearBuffer();
    return true;
}

void setDark(bool on) { darkMode = on; }
bool isDark() { return darkMode; }

uint8_t paper()  { return darkMode ? 0x00 : 0xFF; }
uint8_t inkC()   { return darkMode ? 0xFF : 0x00; }
uint8_t ruleC()  { return darkMode ? 0x90 : 0x80; }
uint8_t faintC() { return darkMode ? 0x38 : 0xE0; }
uint8_t dimC()   { return darkMode ? 0x66 : 0x99; }
int footerH = 44;
int W() { return EPD_WIDTH; }
int H() { return EPD_HEIGHT; }

void clearBuffer() { memset(fb, paper(), FB_BYTES); }

static void powerOn()  { if (!powered) { epd_poweron();      powered = true;  } }
void powerDown()       { if (powered)  { epd_poweroff_all(); powered = false; } }

void flushFull() {
    powerOn();
    epd_clear();
    epd_draw_grayscale_image(epd_full_screen(), fb);
    partialCount = 0;
    powerDown();
}

// Copy a sub-rectangle out of the framebuffer and push just that.  x/w are
// snapped to even values because the framebuffer packs two pixels per byte.
void flushRegion(int x, int y, int w, int h, bool clean) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (w <= 0 || h <= 0) return;
    x &= ~1;
    w = (w + 1) & ~1;
    if (x + w > EPD_WIDTH)  w = EPD_WIDTH - x;
    if (y + h > EPD_HEIGHT) h = EPD_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    const int stride = w / 2;
    for (int r = 0; r < h; r++)
        memcpy(regionBuf + (size_t)r * stride,
               fb + (size_t)(y + r) * (EPD_WIDTH / 2) + x / 2, stride);

    Rect_t area = {x, y, w, h};
    powerOn();
    epd_clear_area_cycles(area, clean ? 3 : 1, clean ? 50 : 30);
    epd_draw_grayscale_image(area, regionBuf);
    partialCount++;
    powerDown();
}

// ------------------------------------------------------------------ text ---
// The glyph renderer paints a glyph's whole bounding box, interpolating from
// bg_color to fg_color, so both ends have to match the surface being drawn on.
static FontProperties propsFor(Tone t) {
    uint8_t fg, bg;
    if (!darkMode) {
        bg = 15;
        switch (t) {
            case TONE_DIM:   fg = 7;  break;
            case TONE_FAINT: fg = 12; break;
            case TONE_READ:  fg = 9;  break;
            case TONE_INV:   fg = 15; bg = 0; break;
            default:         fg = 0;  break;
        }
    } else {
        bg = 0;
        switch (t) {
            case TONE_DIM:   fg = 8;  break;
            case TONE_FAINT: fg = 4;  break;
            case TONE_READ:  fg = 6;  break;
            case TONE_INV:   fg = 0;  bg = 15; break;
            default:         fg = 15; break;
        }
    }
    FontProperties p;
    p.fg_color = fg;
    p.bg_color = bg;
    p.fallback_glyph = '?';
    p.flags = 0;
    return p;
}

// Width the cursor will actually advance: the renderer steps by advance_x per
// glyph, whereas get_text_bounds() reports ink bounds and drops trailing
// bearings, which under-measures a line by a few pixels per word.
int textW(const GFXfont *f, const char *s) {
    if (!s || !*s) return 0;
    int w = 0;
    const uint8_t *p = (const uint8_t *)s;
    while (*p) {
        uint32_t cp = *p++;
        if (cp >= 0x80) {                              // decode UTF-8
            int extra = (cp & 0xE0) == 0xC0 ? 1 : (cp & 0xF0) == 0xE0 ? 2 : (cp & 0xF8) == 0xF0 ? 3 : 0;
            cp &= (0x3F >> extra);
            while (extra-- && *p) cp = (cp << 6) | (*p++ & 0x3F);
        }
        GFXglyph *g = nullptr;
        get_glyph(f, cp, &g);
        if (!g) get_glyph(f, '?', &g);
        if (g) w += g->advance_x;
    }
    return w;
}

int drawText(const GFXfont *f, const char *s, int x, int baselineY, Tone t) {
    if (!s || !*s) return x;
    int32_t cx = x, cy = baselineY;
    FontProperties p = propsFor(t);
    write_mode(f, s, &cx, &cy, fb, BLACK_ON_WHITE, &p);
    return (int)cx;
}

int drawRight(const GFXfont *f, const String &s, int rightX, int baselineY, Tone t) {
    int w = textW(f, s.c_str());
    return drawText(f, s.c_str(), rightX - w, baselineY, t);
}

int drawTextTrunc(const GFXfont *f, const String &s, int x, int baselineY, int maxW,
                  Tone t) {
    if (s.length() == 0) return x;
    if (textW(f, s.c_str()) <= maxW) return drawText(f, s.c_str(), x, baselineY, t);
    // binary search the longest prefix that fits with an ellipsis
    int lo = 0, hi = s.length();
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        String cand = s.substring(0, mid) + "...";
        if (textW(f, cand.c_str()) <= maxW) lo = mid; else hi = mid - 1;
    }
    String out = s.substring(0, lo);
    out.trim();
    out += "...";
    return drawText(f, out.c_str(), x, baselineY, t);
}

int drawWrapped(const GFXfont *f, const String &s, int x, int topY, int maxW,
                int lineH, int maxLines, bool draw, Tone tone) {
    const int n = (int)s.length();
    int line = 0, i = 0;
    while (i <= n && line < maxLines) {
        int nl = s.indexOf('\n', i);
        int segEnd = (nl < 0) ? n : nl;
        if (segEnd <= i) {                                   // blank line
            line++;
            if (nl < 0) break;
            i = segEnd + 1;
            continue;
        }
        int cur = i;
        while (cur < segEnd && line < maxLines) {
            int best = cur, probe = cur;
            while (probe < segEnd) {                          // grow by whole words
                int sp = s.indexOf(' ', probe + 1);
                if (sp < 0 || sp > segEnd) sp = segEnd;
                if (gfx::textW(f, s.substring(cur, sp).c_str()) <= maxW) { best = sp; probe = sp; }
                else break;
            }
            if (best == cur) {                                // word longer than a line
                int k = cur + 1;
                while (k < segEnd && gfx::textW(f, s.substring(cur, k + 1).c_str()) <= maxW) k++;
                best = (k > cur) ? k : cur + 1;
            }
            if (draw) {
                String ln = s.substring(cur, best);
                ln.trim();
                if (ln.length())
                    drawText(f, ln.c_str(), x, topY + line * lineH + lineH - 6, tone);
            }
            line++;
            cur = best;
            while (cur < segEnd && s[cur] == ' ') cur++;
        }
        if (nl < 0) break;
        i = segEnd + 1;
    }
    return line;
}

// ---------------------------------------------------------------- shapes ---
void rect(int x, int y, int w, int h, uint8_t color)  { epd_fill_rect(x, y, w, h, color, fb); }
void frame(int x, int y, int w, int h, uint8_t color) { epd_draw_rect(x, y, w, h, color, fb); }
void hline(int x, int y, int w, uint8_t color)        { epd_draw_hline(x, y, w, color, fb); }

}  // namespace gfx

uint32_t batteryMilliVolts() {
    static bool inited = false;
    static esp_adc_cal_characteristics_t ch;
    if (!inited) {
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 1100, &ch);
        inited = true;
    }
    uint32_t acc = 0;
    for (int i = 0; i < 8; i++) acc += analogRead(BATT_PIN);
    return esp_adc_cal_raw_to_voltage(acc / 8, &ch) * 2;
}
