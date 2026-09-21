// Hacker News reader for the LilyGo T5 4.7" (ESP32-S3, ED047TC1 960x540).
//
// One user button (GPIO21):
//   short press  - move the highlight
//   long press   - activate what is highlighted
//   double press - go back
// Hold the button while powering on to open Wi-Fi setup.
#include "app.h"
#include <WiFi.h>

// The JSON parser recurses once per nesting level and deep HN threads need
// ~128 levels; the default 8 KB loop stack is not enough.
SET_LOOP_TASK_STACK_SIZE(32 * 1024);

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\nHN reader starting");

    setCpuFrequencyMhz(160);          // plenty for this, and easier on the battery

    if (!gfx::begin()) {
        Serial.println("FATAL: framebuffer allocation failed");
        while (true) delay(1000);
    }
    store::begin();
    gfx::setDark(store::dark());

    bool wantPortal = false;
    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0)
        wantPortal = button::heldAtBoot(3000);

    if (wantPortal || !net::haveCreds()) net::runPortal();

    net::connect(15000);
    net::syncTime();

    ui::begin();
}

void loop() {
    ui::tick();
    delay(5);
}
