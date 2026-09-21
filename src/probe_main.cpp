// Bring-up probe: the production display and touch path, drawing a pattern
// that makes orientation, greys and touch mapping unmistakable. Built with
// -DPROBE (env "probe"); the app's main.cpp is compiled out.
#ifdef PROBE
#include "app.h"
#include <Wire.h>

namespace touch { bool begin(); bool poll(int &x, int &y); }
namespace light { void set(int); }
namespace light { void set(int level) {
    static bool up = false;
    if (!up) { ledcSetup(0, 1000, 8); ledcAttachPin(PIN_BL_EN, 0); up = true; }
    ledcWrite(0, level ? 140 : 0);
} }
// the app's ui/store/net are not linked in this env; satisfy gfx_pro's needs only
int  batteryPercentUnused();

static int taps = 0;

static void drawPattern() {
    gfx::clearBuffer();
    int W = gfx::W(), H = gfx::H();
    gfx::drawText(fonts::md, "TOP of screen - HN reader probe", 24, 48);
    gfx::hline(24, 60, W - 48, gfx::inkC());
    gfx::drawText(fonts::sm, "If this reads the right way up, orientation is correct.", 24, 96, TONE_DIM);

    // 16-level ramp, black on the left
    for (int i = 0; i < 16; i++) gfx::rect(24 + i * 30, 120, 29, 60, (uint8_t)(i * 17));
    gfx::frame(23, 119, 16 * 30 + 2, 62, gfx::inkC());
    gfx::drawText(fonts::meta, "16 greys, black to white", 24, 204, TONE_DIM);

    gfx::drawText(fonts::titleBold, "Bold title face at list size", 24, 260);
    gfx::drawText(fonts::bodyM, "Body text at the default reading size. Tap anywhere:", 24, 300);
    gfx::drawText(fonts::bodyM, "a dot is drawn where the panel thinks you touched,", 24, 332);
    gfx::drawText(fonts::bodyM, "and the raw coordinates go to the serial port.", 24, 364);

    gfx::frame(24, 400, W - 48, H - 400 - 70, gfx::ruleC());
    gfx::drawText(fonts::meta, "corner markers:", 30, 424, TONE_DIM);
    gfx::rect(24, 400, 24, 24, gfx::inkC());                 // top-left
    gfx::rect(W - 48, H - 94, 24, 24, gfx::inkC());          // bottom-right
    gfx::drawText(fonts::md, "BOTTOM", 24, H - 24);
    gfx::drawRight(fonts::sm, "540 x 960", W - 24, H - 24, TONE_DIM);
}

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.println("\n=== T5 E-PAPER S3 PRO PROBE (epdiy) ===");
    setCpuFrequencyMhz(240);
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Serial.println("I2C scan:");
    for (uint8_t a = 1; a < 127; a++) { Wire.beginTransmission(a); if (Wire.endTransmission() == 0) Serial.printf("   0x%02X\n", a); }
    bool ok = gfx::begin();
    Serial.printf("gfx::begin -> %d  (%d x %d)  heap %u  psram %u\n", ok, gfx::W(), gfx::H(),
                  ESP.getFreeHeap(), ESP.getFreePsram());
    Serial.printf("touch::begin -> %d\n", touch::begin());
    Serial.printf("battery: %u mV  %d %%\n", batteryMilliVolts(), batteryPercent());
    light::set(1);
    uint32_t t0 = millis();
    drawPattern();
    gfx::flushFull();
    Serial.printf("pattern drawn and flushed in %lu ms\n", millis() - t0);
    Serial.println("=== READY: tap the screen ===");
}

void loop() {
    int x, y;
    if (touch::poll(x, y)) {
        taps++;
        Serial.printf("TAP %d: x=%d y=%d\n", taps, x, y);
        gfx::rect(x - 8, y - 8, 16, 16, gfx::inkC());
        char b[48]; snprintf(b, sizeof b, "tap %d: %d, %d", taps, x, y);
        gfx::rect(24, 440, 300, 34, gfx::paper());
        gfx::drawText(fonts::sm, b, 30, 464);
        gfx::flushRegion(0, 0, gfx::W(), gfx::H(), false);
    }
    delay(10);
}
#endif
