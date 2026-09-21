#include "app.h"
#include <WiFi.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

namespace ui {

enum Screen  { SC_LIST, SC_DOC, SC_FEEDS, SC_SETTINGS, SC_MSG };
enum DocKind { DOC_COMMENTS, DOC_ARTICLE };

// Every interactive thing on a screen is a Target. A render builds the list
// in highlight-ring order (paging first, so one long-press turns the page);
// the physical button walks the ring and touch hits the rectangles. Both
// end up in the same activate() path.
enum TKind { T_NEXT, T_PREV, T_BACK, T_FEEDMENU, T_ROW, T_CELL, T_MENU, T_COMMENT, T_HDR };
struct Target { TKind kind; int id; int x, y, w, h; };
// style: 0 body, 1 byline (cidx = comment), 2 heading, 3 facts, 4 paragraph gap, 5 comment gap
struct RLine  { String text; uint8_t indent; uint8_t style; int16_t cidx; };

// ---- state ----------------------------------------------------------------
static Screen  screen = SC_LIST;
static bool    inBookmarks = false;
static Feed    feed = FEED_TOP;
static std::vector<Story> stories;
static std::vector<int>   pageStarts;      // index of the first story on each page
static int     page = 0, focus = 0;

static Story   curStory;
static std::vector<Comment> comments;
static String  artTitle, artBody;
static std::vector<RLine> doc;
static DocKind docKind = DOC_COMMENTS;
static int     docPage = 0, docFocus = 0;
static uint32_t commentsFor = 0;
static int     commentsPage = 0;
static bool    articleFromComments = false;

static int     menuFocus = 0;
static String  msgTitle, msgBody, msgHint;
static bool    msgArticleFail = false;     // offer the thread instead of a bare retry
static uint32_t lastInput = 0;
static int     partialsSinceFull = 0;

static std::vector<Target> targets;
static std::vector<String> barCells;       // labels of the bar drawn last

RTC_DATA_ATTR static int  rtcFeed = 0, rtcPage = 0, rtcFocus = 0;
RTC_DATA_ATTR static bool rtcBookmarks = false, rtcValid = false;

static const char *APP_TITLE = "Hacker News";

// ---- typography -----------------------------------------------------------
static bool portrait() { return gfx::W() < gfx::H(); }
static const GFXfont *bodyFont() {
    switch (store::textSize()) { case 0: return fonts::bodyS; case 2: return fonts::bodyL;
                                 default: return fonts::bodyM; }
}
static int bodyLineH() { return bodyFont()->advance_y; }
static const char *sizeName() {
    switch (store::textSize()) { case 0: return "S"; case 2: return "L"; default: return "M"; }
}
static int linesPerPage() { return CONTENT_H / bodyLineH(); }

static const GFXfont *titleFont(bool) {
    return fonts::titleBold ? fonts::titleBold : fonts::title;
}
static int titleLH() { return fonts::title->advance_y - 4; }      // 40 for Roboto18
static int titleAsc() { return fonts::title->ascender; }
static int metaAsc()  { return fonts::meta->ascender; }

// Bold without a bold face: draw twice, one pixel apart. Reads well on e-paper.
static void drawTitleText(const GFXfont *f, const String &s, int x, int base, Tone t, bool bold) {
    gfx::drawText(f, s.c_str(), x, base, t);
    if (bold && !fonts::titleBold) gfx::drawText(f, s.c_str(), x + 1, base, t);
}
static void drawTitleTrunc(const GFXfont *f, const String &s, int x, int base, int maxW, Tone t, bool bold) {
    gfx::drawTextTrunc(f, s, x, base, maxW, t);
    if (bold && !fonts::titleBold) gfx::drawTextTrunc(f, s, x + 1, base, maxW, t);
}

// ---- small drawings -------------------------------------------------------
static void chevron(int cx, int cy, int size, bool left, uint8_t color) {
    int tipX = left ? cx - size / 2 : cx + size / 2;
    for (int i = 0; i <= size; i++) {
        int x = left ? tipX + i : tipX - i;
        gfx::rect(x, cy - i, 3, 3, color);
        gfx::rect(x, cy + i, 3, 3, color);
    }
}
static void chevronV(int cx, int cy, int size, bool up, uint8_t color) {
    int tipY = up ? cy - size / 2 : cy + size / 2;
    for (int i = 0; i <= size; i++) {
        int y = up ? tipY + i : tipY - i;
        gfx::rect(cx - i, y, 3, 3, color);
        gfx::rect(cx + i, y, 3, 3, color);
    }
}
// Header icons, ~22 px. Refresh: three-quarter ring with an arrowhead.
static void iconRefresh(int cx, int cy, uint8_t color) {
    for (int a = 40; a <= 320; a += 5) {
        double r = a * 3.14159265 / 180.0;
        int x = cx + (int)(9.0 * cos(r) + 0.5), y = cy + (int)(9.0 * sin(r) + 0.5);
        gfx::rect(x - 1, y - 1, 3, 3, color);
    }
    // arrowhead at the ring's end (about 320 degrees: upper right)
    int ax = cx + 7, ay = cy - 6;
    for (int i = 0; i < 6; i++) gfx::rect(ax - i, ay - 5 + i, 2 + i, 1, color);
}
// Gear: ring with eight teeth and a hole.
static void iconGear(int cx, int cy, uint8_t color) {
    for (int a = 0; a < 360; a += 4) {
        double r = a * 3.14159265 / 180.0;
        int x = cx + (int)(7.0 * cos(r) + 0.5), y = cy + (int)(7.0 * sin(r) + 0.5);
        gfx::rect(x - 1, y - 1, 3, 3, color);
    }
    for (int a = 0; a < 360; a += 45) {
        double r = a * 3.14159265 / 180.0;
        int x = cx + (int)(10.0 * cos(r) + 0.5), y = cy + (int)(10.0 * sin(r) + 0.5);
        gfx::rect(x - 2, y - 2, 4, 4, color);
    }
    for (int a = 0; a < 360; a += 6) {
        double r = a * 3.14159265 / 180.0;
        int x = cx + (int)(2.5 * cos(r) + 0.5), y = cy + (int)(2.5 * sin(r) + 0.5);
        gfx::rect(x, y, 1, 1, gfx::paper());
    }
    gfx::rect(cx - 1, cy - 1, 3, 3, gfx::paper());
}
static void ribbonShape(int x, int y, int w, int h, uint8_t fill, uint8_t notch) {
    gfx::rect(x, y, w, h, fill);
    for (int k = 0; k < w / 2; k++) gfx::rect(x + 1 + k, y + h - 1 - k, w - 2 - 2 * k, 1, notch);
}
static void ribbon(int x, int y, uint8_t color) {           // 14x20 bookmark, filled
    ribbonShape(x, y, 14, 20, color, gfx::paper());
}
static void ribbonOutline(int x, int y, uint8_t color) {    // same, hollow
    ribbonShape(x, y, 14, 20, color, gfx::paper());
    ribbonShape(x + 2, y + 2, 10, 16, gfx::paper(), color);
}
static void iconDoc(int cx, int cy, uint8_t c) {            // a page with text lines
    int x = cx - 8, y = cy - 10;
    gfx::frame(x, y, 16, 20, c);
    gfx::frame(x + 1, y + 1, 14, 18, c);
    for (int i = 0; i < 3; i++) gfx::rect(x + 4, y + 6 + i * 4, 8, 2, c);
}
static void iconBubble(int cx, int cy, uint8_t c) {         // speech bubble
    int x = cx - 10, y = cy - 9;
    gfx::frame(x, y, 20, 14, c);
    gfx::frame(x + 1, y + 1, 18, 12, c);
    gfx::rect(x + 4, y + 12, 6, 2, gfx::paper());           // open the rim for the tail
    for (int i = 0; i < 4; i++) gfx::rect(x + 4, y + 12 + i, 6 - i, 2, c);
}
// Facts separated by dots; whole facts are dropped from the end when they do
// not fit, which reads better than an ellipsis through a number.
static void drawMeta(const std::vector<String> &segs, int x, int base, int maxW, Tone t) {
    int cx = x;
    const int gap = 16;
    for (size_t i = 0; i < segs.size(); i++) {
        int w = gfx::textW(fonts::meta, segs[i].c_str());
        if (cx + (i ? gap : 0) + w > x + maxW) break;
        if (i) { gfx::rect(cx + 6, base - 9, 3, 3, gfx::ruleC()); cx += gap; }
        gfx::drawText(fonts::meta, segs[i].c_str(), cx, base, t);
        cx += w;
    }
}

// ---- wrapping -------------------------------------------------------------
static std::vector<String> wrapLines(const GFXfont *f, const String &s, int maxW) {
    std::vector<String> out;
    maxW -= 2;
    const int spaceW = gfx::textW(f, " ");
    const int n = s.length();
    int i = 0;
    while (i <= n) {
        int nl = s.indexOf('\n', i);
        int segEnd = (nl < 0) ? n : nl;
        if (segEnd <= i) {
            out.push_back("");
            if (nl < 0) break;
            i = segEnd + 1;
            continue;
        }
        String line;
        int lineW = 0, p = i;
        while (p < segEnd) {
            int sp = s.indexOf(' ', p);
            if (sp < 0 || sp > segEnd) sp = segEnd;
            String word = s.substring(p, sp);
            if (word.length()) {
                int wW = gfx::textW(f, word.c_str());
                if (wW > maxW) {
                    if (line.length()) { out.push_back(line); line = ""; lineW = 0; }
                    String chunk;
                    int chunkW = 0;
                    for (size_t k = 0; k < word.length(); k++) {
                        char cb[2] = {word[k], 0};
                        int cw = gfx::textW(f, cb);
                        if (chunkW + cw > maxW && chunk.length()) { out.push_back(chunk); chunk = ""; chunkW = 0; }
                        chunk += word[k];
                        chunkW += cw;
                    }
                    line = chunk;
                    lineW = chunkW;
                } else if (line.length() && lineW + spaceW + wW > maxW) {
                    out.push_back(line);
                    line = word;
                    lineW = wW;
                } else {
                    if (line.length()) { line += ' '; lineW += spaceW; }
                    line += word;
                    lineW += wW;
                }
            }
            p = sp;
            while (p < segEnd && s[p] == ' ') p++;
        }
        if (line.length()) out.push_back(line);
        if (nl < 0) break;
        i = segEnd + 1;
    }
    return out;
}

// Title lines for a list row: at most three, the last one truncated.
static std::vector<String> titleLines(const GFXfont *f, const String &t, int maxW) {
    auto lines = wrapLines(f, t, maxW);
    if (lines.size() > 3) {
        String rest = lines[2];
        for (size_t k = 3; k < lines.size(); k++) rest += String(" ") + lines[k];
        lines.resize(3);
        lines[2] = rest;                              // drawn with the truncator
    }
    if (lines.empty()) lines.push_back("");
    return lines;
}

// ---- focus / targets ------------------------------------------------------
static int *focusVar() {
    switch (screen) { case SC_DOC: return &docFocus; case SC_LIST: return &focus; default: return &menuFocus; }
}
// Touch device: targets are hit areas only, nothing is ever "focused".
static bool isF(TKind, int) { return false; }
static void addTarget(TKind k, int id, int x = 0, int y = 0, int w = 0, int h = 0) {
    targets.push_back({k, id, x, y, w, h});
}
static void clampFocus() {
    int *f = focusVar();
    if (targets.empty()) *f = 0;
    else if (*f < 0 || *f >= (int)targets.size()) *f = 0;
}

// ---- chrome ---------------------------------------------------------------
static void drawHeader(const String &title, bool back, const String &right, TKind titleKind) {
    const int base = 31;
    int x = MARGIN_X;
    if (back) {
        chevron(x + 5, base - 9, 7, true, gfx::inkC());
        x += 26;
    }
    int rightW = right.length() ? gfx::textW(fonts::sm, right.c_str()) + 18 : 0;
    int tw = CONTENT_W - (x - MARGIN_X) - rightW;
    gfx::drawTextTrunc(fonts::sm, title, x, base, tw, TONE_INK);
    if (titleKind == T_FEEDMENU) {                  // small "open menu" mark
        int w = gfx::textW(fonts::sm, title.c_str());
        if (w < tw - 20) {
            int cx = x + w + 12;
            for (int i = 0; i < 4; i++) gfx::rect(cx - 4 + i, base - 9 + i, 8 - 2 * i, 1, gfx::ruleC());
        }
    }
    if (right.length()) gfx::drawRight(fonts::sm, right, gfx::W() - MARGIN_X, base, TONE_DIM);
    gfx::hline(MARGIN_X, HEADER_H - 1, CONTENT_W, gfx::inkC());
    if (titleKind != T_ROW) addTarget(titleKind, 0, 0, 0, gfx::W() - rightW - MARGIN_X, HEADER_H);
}

// Bottom menu: one or more rows of equal-width cells. Cell labels carry a
// leading marker: \x02 page up, \x03 page down, \x01 a menu label (gets a
// triangle), \x04 plain non-interactive text.
static const int BAR_ROW = 44;
static std::vector<std::vector<String>> barSpec;
static bool barCanPrev = true, barCanNext = true;    // dim the arrow that has nowhere to go
static void setBar(const std::vector<std::vector<String>> &rows) {
    barSpec = rows;
    barCells.clear();
    for (auto &r : rows) for (auto &c : r) barCells.push_back(c);
    gfx::footerH = (int)rows.size() * BAR_ROW;
}
static void addBarTargets() {
    int top = gfx::H() - gfx::footerH, flat = 0;
    for (size_t r = 0; r < barSpec.size(); r++) {
        auto &row = barSpec[r];
        int n = (int)row.size(), w = n ? CONTENT_W / n : 0;
        for (int i = 0; i < n; i++, flat++) {
            int x = MARGIN_X + i * w, y = top + (int)r * BAR_ROW;
            char m = row[i][0];
            if (m == '\x02')      addTarget(T_PREV, 0, x, y, w, BAR_ROW);
            else if (m == '\x03') addTarget(T_NEXT, 0, x, y, w, BAR_ROW);
            else if (m != '\x04') addTarget(T_CELL, flat, x, y, w, BAR_ROW);
        }
    }
}
static void drawBar() {
    if (barSpec.empty()) return;
    int top = gfx::H() - gfx::footerH;
    gfx::hline(MARGIN_X, top, CONTENT_W, gfx::ruleC());
    for (size_t r = 0; r < barSpec.size(); r++) {
        auto &row = barSpec[r];
        int n = (int)row.size(), w = n ? CONTENT_W / n : 0;
        int y = top + (int)r * BAR_ROW, cy = y + BAR_ROW / 2;
        if (r) gfx::hline(MARGIN_X, y, CONTENT_W, gfx::ruleC());
        for (int i = 0; i < n; i++) {
            int x = MARGIN_X + i * w;
            if (i) gfx::rect(x, y, 1, BAR_ROW, gfx::ruleC());     // same weight as the rules
            char m = row[i][0];
            if (m == '\x02')      { chevronV(x + w / 2, cy, 9, true,  barCanPrev ? gfx::inkC() : gfx::dimC()); continue; }
            if (m == '\x03')      { chevronV(x + w / 2, cy, 9, false, barCanNext ? gfx::inkC() : gfx::dimC()); continue; }
            if (m == '\x05')      { iconRefresh(x + w / 2, cy, gfx::inkC()); continue; }
            if (m == '\x06')      { iconGear(x + w / 2, cy, gfx::inkC()); continue; }
            if (m == '\x07')      { ribbon(x + w / 2 - 7, cy - 10, gfx::inkC()); continue; }
            if (m == '\x10')      { ribbonOutline(x + w / 2 - 7, cy - 10, gfx::inkC()); continue; }
            if (m == '\x11')      { ribbon(x + w / 2 - 7, cy - 10, gfx::inkC()); continue; }
            if (m == '\x12')      { iconDoc(x + w / 2, cy, gfx::inkC()); continue; }
            if (m == '\x13')      { iconBubble(x + w / 2, cy, gfx::inkC()); continue; }
            if (m == '\x14')      { chevron(x + w / 2, cy, 9, true, gfx::inkC()); continue; }
            bool menu = (m == '\x01'), plain = (m == '\x04');
            String label = (menu || plain) ? row[i].substring(1) : row[i];
            int tw = gfx::textW(fonts::sm, label.c_str()) + (menu ? 18 : 0);
            int tx = x + (w - (tw < w - 8 ? tw : w - 8)) / 2;
            gfx::drawTextTrunc(fonts::sm, label, tx, cy + 9, w - 8, plain ? TONE_DIM : TONE_INK);
            if (menu) {
                int cx = tx + tw - 6;
                for (int k = 0; k < 4; k++) gfx::rect(cx - 4 + k, cy + k, 8 - 2 * k, 1, gfx::inkC());
            }
        }
    }
}

static void present(bool full) {
    int limit = gfx::isDark() ? 4 : 6;
    if (full || partialsSinceFull >= limit) { gfx::flushFull(); partialsSinceFull = 0; }
    else { gfx::flushRegion(0, 0, gfx::W(), gfx::H(), false); partialsSinceFull++; }
}

static void showBusy(const String &msg) {
    int tw = gfx::textW(fonts::md, msg.c_str());
    int w = tw + 72, h = 84;
    if (w > CONTENT_W) w = CONTENT_W;
    int x = (gfx::W() - w) / 2, y = (gfx::H() - h) / 2;
    gfx::rect(x, y, w, h, gfx::paper());
    gfx::frame(x, y, w, h, gfx::inkC());
    gfx::frame(x + 1, y + 1, w - 2, h - 2, gfx::inkC());
    gfx::drawTextTrunc(fonts::md, msg, x + 24, y + h / 2 + 12, w - 48);
    gfx::flushRegion(x - 2, y - 2, w + 4, h + 4, false);
}

static String statusRight(const String &pageInfo) {
    String right;
    if (pageInfo.length()) right += pageInfo + "   ";
    if (!net::isUp()) right += "offline   ";
    uint32_t mv = batteryMilliVolts();
    if (mv > 2500) {
        int pct = (int)((mv - 3300) * 100 / (4200 - 3300));
        right += String(pct < 0 ? 0 : (pct > 100 ? 100 : pct)) + "%   ";
    }
    time_t now = time(nullptr);
    if (now > 1600000000) {
        struct tm tmv;
        localtime_r(&now, &tmv);
        char b[8];
        snprintf(b, sizeof(b), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
        right += b;
    }
    right.trim();
    return right;
}

// ---- story list -----------------------------------------------------------
static std::vector<Story> &activeList() { return inBookmarks ? store::bookmarks() : stories; }

static int rowHeightFor(const Story &s, int tw) {
    auto lines = titleLines(titleFont(store::isRead(s.id)), s.title, tw);
    return 8 + titleAsc() + titleLH() * ((int)lines.size() - 1) + 8 + metaAsc() + 10;
}
static int titleWidth() { return CONTENT_W; }

// Rows are as tall as their titles, so pages are laid out from the content.
static void layoutPages() {
    pageStarts.clear();
    auto &list = activeList();
    int tw = titleWidth();
    int i = 0, n = (int)list.size();
    if (!n) { pageStarts.push_back(0); pageStarts.push_back(0); return; }
    while (i < n) {
        pageStarts.push_back(i);
        int y = CONTENT_TOP;
        int first = i;
        while (i < n) {
            int h = rowHeightFor(list[i], tw);
            if (i > first && y + h > CONTENT_BOT) break;
            y += h;
            i++;
        }
    }
    pageStarts.push_back(n);
}
static int pageCount()   { return (int)pageStarts.size() > 1 ? (int)pageStarts.size() - 1 : 1; }
static int itemsOnPage() {
    if (page < 0 || page + 1 >= (int)pageStarts.size()) return 0;
    return pageStarts[page + 1] - pageStarts[page];
}

// Header icons on the list screen, on the left: refresh, settings.
// The status (page, battery, time) keeps its place on the right.


static void renderList(bool full) {
    setBar({{inBookmarks ? String(kFeedName[feed]) : String("\x07"), "\x05", "\x06"},
            {"\x02", String("\x01") + (inBookmarks ? "Saved" : kFeedName[feed]), "\x03"}});
    layoutPages();
    if (page >= pageCount()) page = pageCount() - 1;
    if (page < 0) page = 0;
    barCanPrev = page > 0;
    barCanNext = page + 1 < pageCount();
    auto &list = activeList();
    int n = itemsOnPage(), start = page < (int)pageStarts.size() ? pageStarts[page] : 0;
    int tw = titleWidth();

    targets.clear();
    addBarTargets();
    {   // row rectangles need the same geometry pass as drawing
        int y = CONTENT_TOP;
        for (int i = 0; i < n; i++) {
            int h = rowHeightFor(list[start + i], tw);
            addTarget(T_ROW, i, 0, y, gfx::W(), h);
            y += h;
        }
    }
    clampFocus();

    gfx::clearBuffer();
    char pi[24];
    snprintf(pi, sizeof(pi), "%d/%d", page + 1, pageCount());
    {   // header: title on the left, status on the right
        const int base = 31;
        gfx::drawText(fonts::sm, APP_TITLE, MARGIN_X, base, TONE_INK);
        gfx::drawRight(fonts::sm, statusRight(pi), gfx::W() - MARGIN_X, base, TONE_DIM);
        gfx::hline(MARGIN_X, HEADER_H - 1, CONTENT_W, gfx::inkC());
    }

    if (n == 0) {
        gfx::drawText(fonts::md, inBookmarks ? "Nothing saved yet." : "Nothing loaded.",
                      MARGIN_X, CONTENT_TOP + 56, TONE_DIM);
        gfx::drawTextTrunc(fonts::meta,
                           inBookmarks ? "Open a story and choose Save." : "Choose Refresh below.",
                           MARGIN_X, CONTENT_TOP + 92, CONTENT_W, TONE_DIM);
    }

    int y = CONTENT_TOP;
    for (int i = 0; i < n; i++) {
        const Story &s = list[start + i];
        bool read = store::isRead(s.id);
        const GFXfont *tf = titleFont(read);
        auto lines = titleLines(tf, s.title, tw);
        int h = 8 + titleAsc() + titleLH() * ((int)lines.size() - 1) + 8 + metaAsc() + 10;

        int base = y + 8 + titleAsc();
        int tx = MARGIN_X;
        Tone tt = read ? TONE_READ : TONE_INK;
        for (size_t k = 0; k < lines.size(); k++) {
            int b = base + titleLH() * (int)k;
            if (k == lines.size() - 1) drawTitleTrunc(tf, lines[k], tx, b, tw, tt, true);
            else                       drawTitleText(tf, lines[k], tx, b, tt, true);
        }

        int mb = base + titleLH() * ((int)lines.size() - 1) + 8 + metaAsc();
        std::vector<String> segs;
        segs.push_back(String(s.points) + " pts");
        segs.push_back(String(s.comments) + (s.comments == 1 ? " comment" : " comments"));
        String ago = timeAgo(s.time);
        if (ago.length()) segs.push_back(ago);
        String dom = domainOf(s.url);
        if (dom.length()) segs.push_back(dom);
        drawMeta(segs, tx, mb, CONTENT_W, read ? TONE_FAINT : TONE_DIM);

        y += h;
    }

    drawBar();
    present(full);
}

// ---- paged document (comments / article) ----------------------------------
static std::vector<int>  docStarts;        // first line of each page
static std::vector<bool> collapsed;        // per comment

static int metaLH() { return fonts::meta->advance_y + 2; }
static int lineHeightOf(const RLine &l) {
    switch (l.style) {
        case 0:  return bodyLineH();
        case 1:  return metaLH() + 2;
        case 2:  return titleLH();
        case 3:  return metaLH();
        case 4:  return bodyLineH() / 3;
        default: return 18;
    }
}
static bool docRoom() { return doc.size() < MAX_DOC_LINES && ESP.getFreeHeap() > HEAP_FLOOR; }

// Wrapped text; blank lines between paragraphs become small gaps.
static void pushWrapped(const GFXfont *f, const String &text, int indent, uint8_t style) {
    bool lastBlank = true;
    for (auto &ln : wrapLines(f, text, CONTENT_W - indent)) {
        if (ln.length() == 0) {
            if (!lastBlank) doc.push_back({"", (uint8_t)indent, 4, -1});
            lastBlank = true;
            continue;
        }
        lastBlank = false;
        doc.push_back({ln, (uint8_t)indent, style, -1});
    }
}

static void layoutDoc() {
    gfx::footerH = 2 * BAR_ROW;
    docStarts.clear();
    docStarts.push_back(0);
    int y = CONTENT_TOP;
    for (size_t i = 0; i < doc.size(); i++) {
        int h = lineHeightOf(doc[i]);
        if (y + h > CONTENT_BOT && (int)i > docStarts.back()) { docStarts.push_back((int)i); y = CONTENT_TOP; }
        y += h;
    }
    docStarts.push_back((int)doc.size());
}
static int docPageCount() { return (int)docStarts.size() > 1 ? (int)docStarts.size() - 1 : 1; }
static void clampDocPage() {
    if (docPage >= docPageCount()) docPage = docPageCount() - 1;
    if (docPage < 0) docPage = 0;
}

static void buildCommentDoc() {
    doc.clear();
    const int indentPx = 22, maxDepth = 5;
    doc.push_back({"", 0, 4, -1});                       // breathing room under the header rule
    pushWrapped(titleFont(false), curStory.title, 0, 2);
    doc.push_back({"", 0, 4, -1});
    {   // one facts line; drawMeta drops trailing facts that do not fit
        String f = String(curStory.points) + " pts  -  " + curStory.comments + " comments";
        String ago = timeAgo(curStory.time);
        if (ago.length()) f += String("  -  ") + ago;
        if (curStory.author.length()) f += String("  -  ") + curStory.author;
        String d = domainOf(curStory.url);
        if (d.length()) f += String("  -  ") + d;
        doc.push_back({f, 0, 3, -1});
    }
    if (curStory.text.length()) { doc.push_back({"", 0, 5, -1}); pushWrapped(bodyFont(), curStory.text, 0, 0); }
    doc.push_back({"", 0, 5, -1});
    doc.push_back({"", 0, 4, -1});

    if (comments.empty()) { doc.push_back({"No comments yet.", 0, 3, -1}); return; }
    if (collapsed.size() != comments.size()) collapsed.assign(comments.size(), false);
    int skipDepth = -1;
    for (size_t i = 0; i < comments.size(); i++) {
        const Comment &c = comments[i];
        if (skipDepth >= 0 && c.depth > skipDepth) continue;   // inside a folded subtree
        skipDepth = -1;
        if (!docRoom()) { doc.push_back({"[Thread truncated to fit memory.]", 0, 3, -1}); break; }
        int d = c.depth > maxDepth ? maxDepth : c.depth;
        int ind = d * indentPx;
        String head = c.author;
        String ago = timeAgo(c.time);
        if (ago.length()) head += String("  -  ") + ago;
        if (collapsed[i]) {
            int replies = 0;
            for (size_t k = i + 1; k < comments.size() && comments[k].depth > c.depth; k++) replies++;
            head += String("  -  +") + (replies ? String(replies) + (replies == 1 ? " reply" : " replies")
                                               : String("folded"));
            skipDepth = c.depth;
        }
        doc.push_back({head, (uint8_t)ind, 1, (int16_t)i});
        if (!collapsed[i]) pushWrapped(bodyFont(), c.text, ind, 0);
        doc.push_back({"", (uint8_t)ind, 5, -1});
    }
}

static void buildArticleDoc() {
    doc.clear();
    doc.push_back({"", 0, 4, -1});
    pushWrapped(titleFont(false), artTitle, 0, 2);
    doc.push_back({"", 0, 4, -1});
    doc.push_back({domainOf(curStory.url), 0, 3, -1});
    doc.push_back({"", 0, 5, -1});
    doc.push_back({"", 0, 4, -1});
    int start = 0;
    bool lastGap = true;
    while (start <= (int)artBody.length()) {
        if (!docRoom()) { doc.push_back({"[Article truncated to fit memory.]", 0, 3, -1}); break; }
        int nl = artBody.indexOf('\n', start);
        int end = (nl < 0) ? artBody.length() : nl;
        String para = artBody.substring(start, end);
        if (para.length() == 0) { if (!lastGap) doc.push_back({"", 0, 4, -1}); lastGap = true; }
        else if (para.startsWith("# ")) {
            if (!lastGap) doc.push_back({"", 0, 4, -1});
            pushWrapped(titleFont(false), para.substring(2), 0, 2);
            lastGap = false;
        } else { pushWrapped(bodyFont(), para, 0, 0); lastGap = false; }
        if (nl < 0) break;
        start = nl + 1;
    }
}

// \x10 save (outline) / \x11 saved (filled), \x12 article, \x13 comments, \x14 back
static std::vector<String> docCells() {
    std::vector<String> c;
    c.push_back(store::isBookmarked(curStory.id) ? "\x11" : "\x10");
    if (docKind == DOC_COMMENTS) { if (curStory.url.length()) c.push_back("\x12"); }
    else c.push_back("\x13");
    c.push_back("\x14");
    return c;
}

static void drawFacts(const String &text, int x, int base, int maxW, Tone t) {
    std::vector<String> segs;
    int from = 0;
    while (true) {
        int k = text.indexOf("  -  ", from);
        segs.push_back(text.substring(from, k < 0 ? (int)text.length() : k));
        if (k < 0) break;
        from = k + 5;
    }
    if (segs.size() > 1) drawMeta(segs, x, base, maxW, t);
    else gfx::drawTextTrunc(fonts::meta, text, x, base, maxW, t);
}

static void renderDoc(bool full) {
    gfx::footerH = 2 * BAR_ROW;
    clampDocPage();
    char pg[24];
    snprintf(pg, sizeof(pg), "\x04%d / %d", docPage + 1, docPageCount());
    barCanPrev = docPage > 0;
    barCanNext = docPage + 1 < docPageCount();
    setBar({docCells(), {"\x02", pg, "\x03"}});
    int from = docStarts.size() > 1 ? docStarts[docPage] : 0;
    int to   = docStarts.size() > 1 ? docStarts[docPage + 1] : 0;

    targets.clear();
    addBarTargets();
    {   // bylines on this page fold their subtree
        int y = CONTENT_TOP;
        for (int i = from; i < to; i++) {
            int h = lineHeightOf(doc[i]);
            // a byline is ~3 mm tall on the panel; give the finger the gap above it too
            if (doc[i].style == 1 && doc[i].cidx >= 0) addTarget(T_COMMENT, doc[i].cidx, 0, y - 14, gfx::W(), h + 20);
            y += h;
        }
    }
    clampFocus();

    gfx::clearBuffer();
    drawHeader(APP_TITLE, false, statusRight(""), T_ROW);

    int y = CONTENT_TOP;
    for (int i = from; i < to; i++) {
        const RLine &l = doc[i];
        int h = lineHeightOf(l);
        int x = MARGIN_X + l.indent;
        if (l.indent && (l.style == 0 || l.style == 1))
            gfx::rect(MARGIN_X + l.indent - 12, y, 2, h, gfx::faintC());
        switch (l.style) {
            case 0: gfx::drawText(bodyFont(), l.text.c_str(), x, y + bodyFont()->ascender, TONE_INK); break;
            case 1:
                drawFacts(l.text, x, y + metaAsc(), CONTENT_W - l.indent, TONE_DIM);
                break;
            case 2: drawTitleText(titleFont(false), l.text, x, y + titleAsc(), TONE_INK, true); break;
            case 3: drawFacts(l.text, x, y + metaAsc(), CONTENT_W - l.indent, TONE_DIM); break;
            default: break;
        }
        y += h;
    }
    drawBar();
    present(full);
}

// ---- menus ----------------------------------------------------------------
static void checkmark(int x, int y, uint8_t c) {                 // ~22 x 18
    for (int i = 0; i < 7; i++)  gfx::rect(x + i, y + 9 + i, 3, 3, c);
    for (int i = 0; i < 14; i++) gfx::rect(x + 6 + i, y + 15 - i, 3, 3, c);
}

// rows: bold titles; subs: description under the title; values: right-aligned
// setting values; current: row that gets a check. Spacing does the separating.
static void renderMenu(const String &title, const std::vector<String> &rows,
                       const std::vector<String> &subs, const std::vector<String> &values,
                       int current, bool full) {
    setBar({});
    targets.clear();
    bool hasSubs = false;
    for (auto &x : subs) if (x.length()) hasSubs = true;
    int mh = hasSubs ? 14 + titleAsc() + 8 + metaAsc() + 14 : 16 + titleAsc() + 16;
    int bottom = gfx::H() - 8;
    if ((int)rows.size() * mh > bottom - CONTENT_TOP)
        mh = (bottom - CONTENT_TOP) / (rows.size() ? (int)rows.size() : 1);
    for (size_t i = 0; i < rows.size(); i++)
        addTarget(T_MENU, (int)i, 0, CONTENT_TOP + (int)i * mh, gfx::W(), mh);
    addTarget(T_BACK, 0, 0, 0, gfx::W(), HEADER_H);
    clampFocus();

    gfx::clearBuffer();
    drawHeader(title, true, statusRight(""), T_ROW);
    const GFXfont *tf = titleFont(false);
    for (size_t i = 0; i < rows.size(); i++) {
        int y0 = CONTENT_TOP + (int)i * mh;
        int base = y0 + (hasSubs ? 14 : (mh - titleAsc()) / 2) + titleAsc();
        int rightPad = 0;
        if (i < values.size() && values[i].length()) {
            int vw = gfx::textW(fonts::sm, values[i].c_str());
            gfx::drawRight(fonts::sm, values[i], gfx::W() - MARGIN_X, base, TONE_DIM);
            rightPad = vw + 16;
        }
        if ((int)i == current) { checkmark(gfx::W() - MARGIN_X - 22, base - 18, gfx::inkC()); rightPad = 40; }
        drawTitleTrunc(tf, rows[i], MARGIN_X, base, CONTENT_W - rightPad, TONE_INK, true);
        if (hasSubs && i < subs.size() && subs[i].length())
            gfx::drawTextTrunc(fonts::meta, subs[i], MARGIN_X, base + 8 + metaAsc(), CONTENT_W, TONE_DIM);
    }
    present(full);
}

static void renderFeeds(bool full) {
    std::vector<String> rows, subs;
    for (int i = 0; i < FEED_COUNT; i++) { rows.push_back(kFeedName[i]); subs.push_back(kFeedDesc[i]); }
    rows.push_back("Saved");
    subs.push_back(String("Stories you have saved  (") + store::bookmarks().size() + ")");
    renderMenu("Categories", rows, subs, {}, inBookmarks ? FEED_COUNT : (int)feed, full);
}

static void renderSettings(bool full) {
    std::vector<String> rows = {"Theme", "Text size", "Open story with", "Mark this feed as read",
                                "Clear read history", "Clear cached feeds", "Wi-Fi setup", "Sleep now"};
    std::vector<String> values = {store::dark() ? "Dark" : "Light", sizeName(),
                                  store::openArticle() ? "Article" : "Comments", "",
                                  String(store::readCount()) + " stories", "", "", ""};
    renderMenu("Settings", rows, {}, values, -1, full);
}

static void renderMessage() {
    if (msgArticleFail) setBar({{"Comments", "Retry", "Back"}});
    else                setBar({{"Retry", "Wi-Fi setup", "Back"}});
    targets.clear();
    addBarTargets();
    clampFocus();

    gfx::clearBuffer();
    drawHeader(APP_TITLE, false, statusRight(""), T_ROW);
    int y = CONTENT_TOP + 60;
    gfx::drawWrapped(fonts::lg, msgTitle, MARGIN_X, y - 40, CONTENT_W, 52, 2, true, TONE_INK);
    y += 80;
    gfx::drawWrapped(fonts::md, msgBody, MARGIN_X, y, CONTENT_W, 42, 5, true, TONE_DIM);
    if (msgHint.length())
        gfx::drawWrapped(fonts::meta, msgHint, MARGIN_X, CONTENT_BOT - 64, CONTENT_W, 28, 2, true, TONE_DIM);
    drawBar();
    present(true);
}

static void message(const String &t, const String &b, const String &hint = "", bool articleFail = false) {
    msgTitle = t; msgBody = b; msgHint = hint; msgArticleFail = articleFail;
    screen = SC_MSG;
    menuFocus = 0;
    renderMessage();
}

// ---- actions --------------------------------------------------------------
static void goSleep() {
    gfx::clearBuffer();
    targets.clear();
    drawHeader("Hacker News", false, "", T_ROW);
    gfx::drawText(fonts::lg, "Sleeping", MARGIN_X, CONTENT_TOP + 80);
    gfx::drawWrapped(fonts::md, "Press the button or tap the screen to wake.", MARGIN_X,
                     CONTENT_TOP + 110, CONTENT_W, 42, 3, true, TONE_DIM);
    gfx::flushFull();
    gfx::powerDown();

    rtcFeed = feed; rtcPage = page; rtcFocus = focus;
    rtcBookmarks = inBookmarks; rtcValid = true;
    store::setLastFeed(feed);

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    rtc_gpio_pullup_en((gpio_num_t)BUTTON_1);
    rtc_gpio_pulldown_dis((gpio_num_t)BUTTON_1);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_1, 0);
    delay(50);
    esp_deep_sleep_start();
}

static bool loadFeed(bool forceNetwork) {
    uint32_t at = 0;
    if (!forceNetwork && store::loadFeedCache(feed, stories, at) && !stories.empty()) {
        time_t now = time(nullptr);
        if (now < 1600000000 || now - (time_t)at < CACHE_TTL_S) return true;
    }
    showBusy(String("Loading ") + kFeedName[feed] + "...");
    net::ensureUp();
    String err;
    std::vector<Story> fresh;
    if (hn::fetchFeed(feed, fresh, err)) {
        stories = fresh;
        store::saveFeedCache(feed, stories);
        return true;
    }
    if (!stories.empty()) return true;
    if (store::loadFeedCache(feed, stories, at) && !stories.empty()) return true;
    message("Could not load stories", err, "Retry, or open Wi-Fi setup to change network.");
    return false;
}

static void showCommentDoc(int atPage) {
    docKind = DOC_COMMENTS;
    buildCommentDoc();
    layoutDoc();
    docPage = atPage;
    clampDocPage();
    docFocus = 0;
    screen = SC_DOC;
    renderDoc(true);
}

static void openComments(const Story &s) {
    if (commentsFor == s.id && !comments.empty()) { showCommentDoc(commentsPage); return; }
    curStory = s;
    store::markRead(s.id);
    comments.clear();
    commentsFor = 0;
    showBusy("Loading comments...");
    net::ensureUp();
    String err;
    if (!hn::fetchComments(s.id, comments, curStory, err)) {
        message("Could not load comments", err, "Tap the header to go back.");
        return;
    }
    commentsFor = s.id;
    commentsPage = 0;
    collapsed.assign(comments.size(), false);
    showCommentDoc(0);
}

static void openArticle(const Story &s) {
    if (s.url.length() == 0) { openComments(s); return; }
    if (curStory.id != s.id) curStory = s;
    store::markRead(s.id);
    if (ESP.getFreeHeap() < 110000) { comments.clear(); commentsFor = 0; }
    showBusy("Fetching article...");
    net::ensureUp();
    String err;
    if (!reader::fetch(s.url, artTitle, artBody, err)) {
        message("Could not read article", err, domainOf(s.url), true);
        return;
    }
    docKind = DOC_ARTICLE;
    buildArticleDoc();
    layoutDoc();
    docPage = 0; docFocus = 0;
    screen = SC_DOC;
    renderDoc(true);
}

static void openStory(const Story &s) {
    articleFromComments = false;
    if (store::openArticle() && s.url.length()) { curStory = s; openArticle(s); }
    else openComments(s);
}

static void applyTheme() { gfx::setDark(store::dark()); }

static void back();

static void activateList(const Target &t) {
    switch (t.kind) {
        case T_NEXT: if (page + 1 < pageCount()) { page++; renderList(false); } break;
        case T_PREV: if (page > 0)               { page--; renderList(false); } break;
        case T_FEEDMENU: screen = SC_FEEDS; menuFocus = feed; renderFeeds(true); break;
        case T_BACK: back(); break;
        case T_ROW: {
            int start = pageStarts[page];
            openStory(activeList()[start + t.id]);
            break;
        }
        case T_CELL: {
            String label = t.id < (int)barCells.size() ? barCells[t.id] : "";
            char m = label[0];
            if (m == '\x01') {                              // category picker
                screen = SC_FEEDS;
                menuFocus = inBookmarks ? FEED_COUNT : (int)feed;
                renderFeeds(true);
            } else if (m == '\x05') {                       // refresh
                if (inBookmarks) renderList(true);
                else { page = 0; if (loadFeed(true)) renderList(true); }
            } else if (m == '\x06') {                       // settings
                screen = SC_SETTINGS; menuFocus = 0; renderSettings(true);
            } else if (m == '\x07') {                       // bookmark: the saved list
                inBookmarks = true; page = 0; renderList(true);
            } else {                                        // feed name: leave Saved
                inBookmarks = false; page = 0;
                if (stories.empty() && !loadFeed(false)) return;
                renderList(true);
            }
            break;
        }
        default: break;
    }
}

static void activateDoc(const Target &t) {
    switch (t.kind) {
        case T_NEXT: if (docPage + 1 < docPageCount()) { docPage++; renderDoc(false); } break;
        case T_PREV: if (docPage > 0) { docPage--; renderDoc(false); } break;
        case T_BACK: back(); break;
        case T_CELL: {
            String label = t.id < (int)barCells.size() ? barCells[t.id] : "";
            char m = label[0];
            if (m == '\x10' || m == '\x11') { store::toggleBookmark(curStory); renderDoc(false); }
            else if (m == '\x12') { commentsPage = docPage; articleFromComments = true; openArticle(curStory); }
            else if (m == '\x13') { articleFromComments = true; openComments(curStory); }
            else if (m == '\x14') back();
            break;
        }
        case T_COMMENT:
            if (t.id >= 0 && t.id < (int)collapsed.size()) {
                collapsed[t.id] = !collapsed[t.id];
                buildCommentDoc();
                layoutDoc();
                renderDoc(false);
            }
            break;
        default: break;
    }
}

static void activateFeeds(const Target &t) {
    if (t.kind == T_MENU && t.id == FEED_COUNT) {          // Saved
        inBookmarks = true;
        page = 0; focus = 0;
        screen = SC_LIST;
        renderList(true);
        return;
    }
    if (t.kind == T_MENU) {
        feed = (Feed)t.id;
        store::setLastFeed(feed);
        inBookmarks = false;
        page = 0; focus = 0;
        screen = SC_LIST;
        if (loadFeed(false)) renderList(true);
    } else back();
}

static void activateSettings(const Target &t) {
    if (t.kind != T_MENU) { back(); return; }
    switch (t.id) {
        case 0: store::setDark(!store::dark()); applyTheme(); break;
        case 1: store::setTextSize((store::textSize() + 1) % 3); break;
        case 2: store::setOpenArticle(!store::openArticle()); break;
        case 3: store::markAllRead(activeList()); break;
        case 4: store::clearRead(); break;
        case 5: store::clearCache(); stories.clear(); break;
        case 6: net::runPortal(); delay(100); ESP.restart(); break;
        case 7: goSleep(); break;
    }
    renderSettings(true);
}

static void activateMsg(const Target &t) {
    if (t.kind == T_BACK) { back(); return; }
    String label = t.id < (int)barCells.size() ? barCells[t.id] : "";
    if (label == "Comments")        { articleFromComments = false; openComments(curStory); }
    else if (label == "Back")       back();
    else if (label == "Wi-Fi setup"){ net::runPortal(); delay(100); ESP.restart(); }
    else if (label == "Sleep")      goSleep();
    else if (label == "Retry") {
        if (msgArticleFail) openArticle(curStory);
        else { screen = SC_LIST; if (loadFeed(true)) renderList(true); }
    }
}

static void activate(const Target &t) {
    switch (screen) {
        case SC_LIST:     activateList(t); break;
        case SC_DOC:      activateDoc(t); break;
        case SC_FEEDS:    activateFeeds(t); break;
        case SC_SETTINGS: activateSettings(t); break;
        case SC_MSG:      activateMsg(t); break;
    }
}

static void back() {
    switch (screen) {
        case SC_DOC:
            if (docKind == DOC_ARTICLE && articleFromComments) {
                articleFromComments = false;
                openComments(curStory);
                return;
            }
            if (docKind == DOC_COMMENTS) commentsPage = docPage;
            screen = SC_LIST; renderList(true); break;
        case SC_FEEDS:
        case SC_SETTINGS:
        case SC_MSG:
            screen = SC_LIST; renderList(true); break;
        default:
            if (inBookmarks) { inBookmarks = false; page = 0; focus = 0; }
            else { page = 0; focus = 0; }
            renderList(true);
    }
}

static void rerender() {
    switch (screen) {
        case SC_LIST:     renderList(false); break;
        case SC_DOC:      renderDoc(false); break;
        case SC_FEEDS:    renderFeeds(false); break;
        case SC_SETTINGS: renderSettings(false); break;
        case SC_MSG:      renderMessage(); break;
    }
}

static void pageBy(int dir) {
    if (screen == SC_LIST) {
        if (dir > 0 && page + 1 < pageCount()) { page++; renderList(false); }
        if (dir < 0 && page > 0)               { page--; renderList(false); }
    } else if (screen == SC_DOC) {
        if (dir > 0 && docPage + 1 < docPageCount()) { docPage++; renderDoc(false); }
        if (dir < 0 && docPage > 0)                  { docPage--; renderDoc(false); }
    }
}

static void handle(Gesture g) {
    if (g == G_NONE) return;
    lastInput = millis();
    if (g == G_SHORT)       pageBy(+1);
    else if (g == G_LONG)   pageBy(-1);
    else if (g == G_DOUBLE) back();
}

// ---- lifecycle ------------------------------------------------------------
void begin() {
    button::begin();
    applyTheme();
    lastInput = millis();

    bool woke = esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0;
    if (woke && rtcValid) {
        feed = (Feed)constrain(rtcFeed, 0, FEED_COUNT - 1);
        inBookmarks = rtcBookmarks;
        page = rtcPage;
        focus = rtcFocus;
    } else {
        feed = (Feed)constrain(store::lastFeed(), 0, FEED_COUNT - 1);
        page = 0; focus = 0;
    }

    screen = SC_LIST;
    if (inBookmarks) { renderList(true); return; }

    uint32_t at = 0;
    if (store::loadFeedCache(feed, stories, at) && !stories.empty()) {
        renderList(true);
        time_t now = time(nullptr);
        if (!woke && now > 1600000000 && now - (time_t)at > CACHE_TTL_S)
            if (loadFeed(true)) renderList(true);
        return;
    }
    if (loadFeed(true)) renderList(true);
}

// Touch: the target under the finger, or a page turn on a document.
void tap(int x, int y) {
    lastInput = millis();
    for (size_t i = 0; i < targets.size(); i++) {
        const Target &t = targets[i];
        if (t.w <= 0 || t.h <= 0) continue;
        if (x >= t.x && x < t.x + t.w && y >= t.y && y < t.y + t.h) { activate(t); return; }
    }
    if (screen == SC_DOC && y > HEADER_H && y < gfx::H() - FOOTER_H) {
        if (y < (CONTENT_TOP + CONTENT_BOT) / 2) { if (docPage > 0) { docPage--; renderDoc(false); } }
        else if (docPage + 1 < docPageCount())   { docPage++; renderDoc(false); }
    }
}

void tick() {
    handle(button::poll());
    if (IDLE_SLEEP_MS && millis() - lastInput > IDLE_SLEEP_MS) goSleep();
}

}  // namespace ui
