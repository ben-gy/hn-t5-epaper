// Measures every fixed string the firmware draws at an absolute position and
// reports the ones wider than the page. Text drawn through drawWrapped or
// drawTextTrunc cannot overflow; text drawn with drawText can, and did — the
// Wi-Fi setup screen shipped three lines 70 % wider than the panel.
// Input lines: <font>\t<limit px>\t<text>
#include "glyph.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>
#include "fonts/roboto8.h"
#include "fonts/roboto9.h"
#include "fonts/roboto11.h"
#include "fonts/roboto13.h"
#include "fonts/roboto13bold.h"
#include "fonts/roboto16.h"
#include "fonts/roboto18.h"
#include "fonts/roboto28.h"

extern "C" int epd_fb_width = 540, epd_fb_height = 960;

static const struct { const char *name; const GFXfont *f; } FONTS[] = {
    {"sm", &Roboto11}, {"md", &Roboto16}, {"lg", &Roboto18}, {"xl", &Roboto28},
    {"title", &Roboto13}, {"titleBold", &Roboto13Bold}, {"meta", &Roboto9},
    {"bodyS", &Roboto8}, {"bodyM", &Roboto11}, {"bodyL", &Roboto13},
};

static int widthOf(const GFXfont *f, const std::string &s) {
    int t = 0;
    for (unsigned char c : s) {
        GFXglyph *g = nullptr;
        get_glyph(f, c, &g);
        if (!g) get_glyph(f, '?', &g);
        if (g) t += g->advance_x;
    }
    return t;
}

int main() {
    std::string line;
    int checked = 0, bad = 0;
    while (std::getline(std::cin, line)) {
        size_t a = line.find('\t'), b = line.find('\t', a + 1);
        if (a == std::string::npos || b == std::string::npos) continue;
        std::string fname = line.substr(0, a);
        int limit = atoi(line.substr(a + 1, b - a - 1).c_str());
        std::string text = line.substr(b + 1);
        const GFXfont *f = nullptr;
        for (auto &e : FONTS) if (fname == e.name) f = e.f;
        if (!f) continue;
        int w = widthOf(f, text);
        checked++;
        if (w > limit) { bad++; printf("  OVERFLOW %4d > %d  [%s]  \"%s\"\n", w, limit, fname.c_str(), text.c_str()); }
    }
    printf("%d fixed strings checked, %d wider than the page\n", checked, bad);
    return bad ? 1 : 0;
}
