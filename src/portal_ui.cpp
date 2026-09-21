// The Wi-Fi setup screen, kept apart from the radio code so it compiles on the
// host too: it is the first thing the device ever draws and the only screen a
// new owner is guaranteed to see, so it is worth being able to look at it
// without flashing anything (test/host/portal.sh renders it to a PNG).
#include "app.h"

namespace net {

// The first screen the device ever shows, so it has to fit. Every prose line
// is wrapped to the content width rather than placed by hand: the fixed
// strings this replaces were 830 px wide on a 492 px page and ran off the
// right edge. Step numbers hang in the margin beside their wrapped text.
static const char *STEP1 = "Join this Wi-Fi network from your phone or laptop:";
static const char *STEP2 = "The setup page usually opens by itself. If it does not, browse to:";
static const char *STEP3 = "Choose your network, enter its password and press Save.";
static const char *PORTAL_URL = "http://192.168.4.1";

void drawPortalScreen(const char *status) {
    gfx::clearBuffer();
    const int numX  = MARGIN_X;
    const int textX = MARGIN_X + 34;
    const int textW = CONTENT_W - 34;
    const int lh    = fonts::sm->advance_y;
    const int bigH  = fonts::lg->advance_y;

    int y = 56;
    gfx::drawText(fonts::xl, "Wi-Fi setup", MARGIN_X, y);
    y += 26;
    gfx::hline(MARGIN_X, y, CONTENT_W, gfx::inkC());
    y += 36;

    // 1 — join the access point, its name in a panel of its own
    gfx::drawText(fonts::sm, "1.", numX, y + lh - 6);
    y += gfx::drawWrapped(fonts::sm, STEP1, textX, y, textW, lh, 3, true) * lh;
    y += 8;
    // An outline, not a fill: the glyph renderer paints each glyph's whole box
    // in the background colour, so text over a grey panel punches white notches.
    int boxW = gfx::textW(fonts::lg, AP_SSID) + 32;
    if (boxW > textW) boxW = textW;
    gfx::frame(textX, y, boxW, bigH + 14, gfx::ruleC());
    gfx::drawTextTrunc(fonts::lg, AP_SSID, textX + 16, y + bigH + 1, boxW - 32);
    y += bigH + 14 + 22;

    // 2 — the setup page
    gfx::drawText(fonts::sm, "2.", numX, y + lh - 6);
    y += gfx::drawWrapped(fonts::sm, STEP2, textX, y, textW, lh, 3, true) * lh;
    y += 6;
    gfx::drawText(fonts::lg, PORTAL_URL, textX, y + bigH - 8);
    y += bigH + 18;

    // 3 — pick a network
    gfx::drawText(fonts::sm, "3.", numX, y + lh - 6);
    y += gfx::drawWrapped(fonts::sm, STEP3, textX, y, textW, lh, 3, true) * lh;

    gfx::hline(MARGIN_X, gfx::H() - FOOTER_H, CONTENT_W, gfx::ruleC());
    gfx::drawTextTrunc(fonts::sm, status, MARGIN_X, gfx::H() - 14, CONTENT_W, TONE_DIM);
    gfx::flushFull();
}

}  // namespace net
