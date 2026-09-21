#pragma once
#include <Arduino.h>
#include <vector>
#include "glyph.h"
#include "pins.h"

// ---------------------------------------------------------------- config ---
#define AP_SSID          "HN-Reader-Setup"
#define HTTP_UA          "Mozilla/5.0 (compatible; LilyGoT5-HN/1.1)"
#define STORIES_PER_PAGE 6
#define MAX_STORIES      60
#define MAX_COMMENTS     250
#define MAX_BOOKMARKS    200
#define MAX_READ_IDS     1500
#define IDLE_SLEEP_MS    120000UL
#define CACHE_TTL_S      1800
// Rendered text lives in Arduino Strings (internal RAM), so it needs a budget.
#define MAX_COMMENT_CHARS  60000
#define MAX_ONE_COMMENT    2500
#define MAX_ARTICLE_CHARS  60000
#define MAX_DOC_LINES      4000
#define HEAP_FLOOR         45000

// layout
#define MARGIN_X    24
#define HEADER_H    44
#define FOOTER_H    (gfx::footerH)        // bottom menu; height set per screen
#define CONTENT_TOP (HEADER_H + 6)
#define CONTENT_BOT (gfx::H() - FOOTER_H - 6)
#define CONTENT_H   (CONTENT_BOT - CONTENT_TOP)
#define CONTENT_W   (gfx::W() - 2 * MARGIN_X)

// Semantic text tones; the concrete greys depend on the active theme.
enum Tone : uint8_t { TONE_INK = 0, TONE_DIM, TONE_FAINT, TONE_INV, TONE_READ };   // READ = ink at 40%

namespace fonts {
    extern const GFXfont *sm, *md, *lg, *xl;        // 29/44/50/78 px line height
    extern const GFXfont *title, *titleBold, *meta; // roles; titleBold may be null (faux bold)
    extern const GFXfont *bodyS, *bodyM, *bodyL;    // reading sizes
}

// ----------------------------------------------------------------- types ---
struct Story {
    uint32_t id = 0;
    String   title;
    String   url;
    String   author;
    uint32_t points = 0;
    uint32_t comments = 0;
    uint32_t time = 0;
    String   text;
};

struct Comment {
    String   author;
    String   text;
    uint8_t  depth = 0;
    uint32_t time = 0;
};

enum Feed { FEED_TOP = 0, FEED_NEW, FEED_BEST, FEED_ASK, FEED_SHOW, FEED_JOBS, FEED_COUNT };
extern const char *kFeedName[FEED_COUNT];
extern const char *kFeedDesc[FEED_COUNT];

// ------------------------------------------------------------------ text ---
String utf8ToAscii(const String &in);
String latin1ToUtf8(const String &in);
String htmlToText(const String &in);
String timeAgo(uint32_t unixSecs);
String domainOf(const String &url);

// -------------------------------------------------------------------- gfx ---
namespace gfx {
    extern uint8_t *fb;
    bool begin();
    int  W();            // logical screen size (portrait on the phone-shaped Pro)
    int  H();
    extern int footerH;  // height of the bottom menu on the current screen
    void setDark(bool on);
    bool isDark();

    uint8_t paper();     // page background
    uint8_t inkC();      // strongest foreground, for rules/fills
    uint8_t ruleC();     // hairlines
    uint8_t faintC();    // separators
    uint8_t dimC();      // ink at 40%, for disabled controls

    void clearBuffer();
    void flushFull();
    void flushRegion(int x, int y, int w, int h, bool clean);
    void powerDown();

    int  textW(const GFXfont *f, const char *s);
    int  drawText(const GFXfont *f, const char *s, int x, int baselineY, Tone t = TONE_INK);
    int  drawRight(const GFXfont *f, const String &s, int rightX, int baselineY, Tone t = TONE_INK);
    int  drawTextTrunc(const GFXfont *f, const String &s, int x, int baselineY, int maxW,
                       Tone t = TONE_INK);
    int  drawWrapped(const GFXfont *f, const String &s, int x, int topY, int maxW,
                     int lineH, int maxLines, bool draw, Tone t = TONE_INK);
    void rect(int x, int y, int w, int h, uint8_t color);
    void frame(int x, int y, int w, int h, uint8_t color);
    void hline(int x, int y, int w, uint8_t color);
}

// ----------------------------------------------------------------- store ---
namespace store {
    bool begin();

    bool  loadWifi(String &ssid, String &pass);
    void  saveWifi(const String &ssid, const String &pass);
    void  clearWifi();

    std::vector<Story> &bookmarks();
    bool  isBookmarked(uint32_t id);
    bool  toggleBookmark(const Story &s);

    bool  isRead(uint32_t id);
    void  markRead(uint32_t id);
    void  markAllRead(const std::vector<Story> &v);
    void  clearRead();
    size_t readCount();

    bool  loadFeedCache(Feed f, std::vector<Story> &out, uint32_t &fetchedAt);
    void  saveFeedCache(Feed f, const std::vector<Story> &in);
    void  clearCache();

    // settings
    bool  dark();          void setDark(bool v);
    int   textSize();      void setTextSize(int v);      // 0 = S, 1 = M, 2 = L
    bool  openArticle();   void setOpenArticle(bool v);
    int   lastFeed();      void setLastFeed(int v);
    int   frontlight();    void setFrontlight(int v);  // 0 off .. 3 high
}

// -------------------------------------------------------------------- net ---
namespace net {
    bool haveCreds();
    bool connect(uint32_t timeoutMs = 20000);
    bool isUp();
    void syncTime();
    void runPortal();
    void ensureUp();
}

// ------------------------------------------------------------------- http ---
namespace http {
    // Body is collected into PSRAM; caller must free with freeBody().
    bool get(const String &url, char **body, size_t *len, String *contentType, String &err,
             size_t maxBytes = 3u * 1024 * 1024);
    void freeBody(char *body);
}

// --------------------------------------------------------------------- hn ---
namespace hn {
    bool fetchFeed(Feed f, std::vector<Story> &out, String &err);
    bool fetchComments(uint32_t storyId, std::vector<Comment> &out, Story &story, String &err);
}

// ----------------------------------------------------------------- reader ---
namespace reader {
    // Best-effort reader-mode extraction of an article.
    bool fetch(const String &url, String &title, String &body, String &err);
}

// ----------------------------------------------------------------- button ---
enum Gesture { G_NONE = 0, G_SHORT, G_LONG, G_DOUBLE };
namespace button {
    void begin();
    Gesture poll();
    bool heldAtBoot(uint32_t ms);
}

// --------------------------------------------------------------------- ui ---
namespace ui { void begin(); void tick(); void tap(int x, int y); }

uint32_t batteryMilliVolts();
int      batteryPercent();               // -1 when no gauge
namespace light { void set(int level); }
