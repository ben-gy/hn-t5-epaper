// Renders the Wi-Fi setup screen to a BMP on the desktop, at both panel sizes,
// so the first screen a new device shows can be checked without flashing.
#include "emu.h"
#include <cstdio>
#include <cstring>
#include <vector>

static void writeBmp(const char *path) {
    const int W = gfx::W(), H = gfx::H();
    const uint32_t pix = W * H, off = 14 + 40 + 256 * 4, total = off + pix;
    std::vector<uint8_t> o(total, 0);
    auto p32 = [&](size_t at, uint32_t v) { o[at] = v; o[at+1] = v >> 8; o[at+2] = v >> 16; o[at+3] = v >> 24; };
    auto p16 = [&](size_t at, uint16_t v) { o[at] = v; o[at+1] = v >> 8; };
    o[0] = 'B'; o[1] = 'M'; p32(2, total); p32(10, off);
    p32(14, 40); p32(18, W); p32(22, H); p16(26, 1); p16(28, 8); p32(34, pix); p32(46, 256); p32(50, 256);
    for (int i = 0; i < 256; i++) o[54 + i*4] = o[55 + i*4] = o[56 + i*4] = i;
    for (int y = 0; y < H; y++) {
        uint8_t *row = &o[off + (size_t)(H - 1 - y) * W];
        const uint8_t *src = gfx::fb + (size_t)y * (W / 2);
        for (int x = 0; x < W; x++) row[x] = ((x & 1) ? (src[x/2] >> 4) : (src[x/2] & 0x0F)) * 17;
    }
    FILE *f = fopen(path, "wb");
    fwrite(o.data(), 1, o.size(), f);
    fclose(f);
    printf("%s  %dx%d\n", path, W, H);
}

// Any pixel of ink in the right-hand margin means something overflowed.
static int inkPastMargin(int marginX) {
    int worst = -1;
    for (int y = 0; y < gfx::H(); y++)
        for (int x = gfx::W() - marginX; x < gfx::W(); x++) {
            const uint8_t *src = gfx::fb + (size_t)y * (gfx::W() / 2);
            uint8_t nib = (x & 1) ? (src[x/2] >> 4) : (src[x/2] & 0x0F);
            if (nib < 14 && y > worst) worst = y;
        }
    return worst;
}

int main(int argc, char **argv) {
    const char *out = argc > 1 ? argv[1] : "portal";
    struct { const char *name; int w, h; } devs[] = {{"t5pro", 540, 960}, {"trmnl", 800, 480}};
    int bad = 0;
    for (auto &d : devs) {
        gfx_host_setSize(d.w, d.h);
        gfx::setDark(false);
        net::drawPortalScreen("Waiting for setup. Hold BOOT 3s to skip.");
        char path[256];
        snprintf(path, sizeof path, "%s_%s.bmp", out, d.name);
        writeBmp(path);
        int y = inkPastMargin(24);
        if (y >= 0) { printf("  FAIL %s: ink in the right margin, lowest at y=%d\n", d.name, y); bad = 1; }
        else printf("  ok   %s: nothing crosses the right margin\n", d.name);
    }
    return bad;
}
