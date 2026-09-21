// T5 E-Paper S3 Pro bring-up v2: panel + frontlight + touch + battery.
#include <Arduino.h>
#include <Wire.h>
#include <FastEPD.h>

FASTEPD epaper;

#define PIN_I2C_SDA   39
#define PIN_I2C_SCL   40
#define PIN_BL_EN     11
#define PIN_TOUCH_INT 3
#define PIN_TOUCH_RST 9
#define GT911_ADDR    0x5D
#define BQ27220_ADDR  0x55

// ---- frontlight: PT4103B23F EN pin, PWM at <= 1 kHz per the schematic notes --
static void lightBegin() {
    ledcSetup(0, 1000, 8);
    ledcAttachPin(PIN_BL_EN, 0);
    ledcWrite(0, 0);
}
static void lightSet(uint8_t duty) { ledcWrite(0, duty); }

// ---- GT911 ----------------------------------------------------------------
static bool gtWrite16(uint16_t reg, const uint8_t *d, size_t n) {
    Wire.beginTransmission(GT911_ADDR);
    Wire.write(reg >> 8); Wire.write(reg & 0xFF);
    for (size_t i = 0; i < n; i++) Wire.write(d[i]);
    return Wire.endTransmission() == 0;
}
static bool gtRead16(uint16_t reg, uint8_t *d, size_t n) {
    Wire.beginTransmission(GT911_ADDR);
    Wire.write(reg >> 8); Wire.write(reg & 0xFF);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)GT911_ADDR, (int)n) != (int)n) return false;
    for (size_t i = 0; i < n; i++) d[i] = Wire.read();
    return true;
}
// Holding INT low across the reset pulse selects I2C address 0x5D.
static void gtReset() {
    pinMode(PIN_TOUCH_RST, OUTPUT);
    pinMode(PIN_TOUCH_INT, OUTPUT);
    digitalWrite(PIN_TOUCH_RST, LOW);
    digitalWrite(PIN_TOUCH_INT, LOW);
    delay(12);
    digitalWrite(PIN_TOUCH_RST, HIGH);
    delay(60);
    pinMode(PIN_TOUCH_INT, INPUT);
    delay(60);
}

void setup() {
    Serial.begin(115200);
    delay(2500);
    Serial.println("\n\n=== T5 E-PAPER S3 PRO PROBE v2 ===");

    lightBegin();
    int rc = epaper.initPanel(BB_PANEL_LILYGO_T5PRO);
    Serial.printf("initPanel -> %d   %d x %d\n", rc, epaper.width(), epaper.height());

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000UL);
    gtReset();

    Serial.println("I2C scan:");
    int found = 0;
    for (uint8_t a = 1; a < 127; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) { Serial.printf("   0x%02X\n", a); found++; }
    }

    uint8_t pid[4] = {0};
    bool touchOk = gtRead16(0x8140, pid, 4);
    Serial.printf("GT911 product id: %s (%c%c%c%c)\n", touchOk ? "OK" : "NOT FOUND",
                  pid[0] ? pid[0] : '-', pid[1] ? pid[1] : '-',
                  pid[2] ? pid[2] : '-', pid[3] ? pid[3] : '-');

    uint8_t v[2] = {0}, soc[2] = {0};
    Wire.beginTransmission(BQ27220_ADDR); Wire.write(0x08);
    bool batOk = (Wire.endTransmission(false) == 0) && (Wire.requestFrom(BQ27220_ADDR, 2) == 2);
    if (batOk) { v[0] = Wire.read(); v[1] = Wire.read(); }
    Wire.beginTransmission(BQ27220_ADDR); Wire.write(0x2C);
    if (Wire.endTransmission(false) == 0 && Wire.requestFrom(BQ27220_ADDR, 2) == 2) {
        soc[0] = Wire.read(); soc[1] = Wire.read();
    }
    uint16_t mv = v[0] | (v[1] << 8), pct = soc[0] | (soc[1] << 8);
    Serial.printf("battery: %u mV, %u%%\n", mv, pct);

    // ---- draw ----
    epaper.setMode(BB_MODE_4BPP);
    epaper.fillScreen(0xF);
    epaper.setTextColor(0, 0xF);

    epaper.setFont(FONT_12x16);
    epaper.setCursor(40, 34);
    epaper.print("T5 E-Paper S3 Pro  -  PROBE v2  -  FRONTLIGHT IS ON");

    epaper.setFont(FONT_8x8);
    epaper.setCursor(40, 66);
    epaper.print("The frontlight should now be steady, not flashing.");

    for (int i = 0; i < 16; i++) epaper.fillRect(40 + i * 52, 96, 50, 64, i);
    epaper.drawRect(38, 94, 16 * 52 + 4, 68, 0);
    epaper.setCursor(40, 176);
    epaper.print("16-level greyscale ramp: should run black (left) to white (right)");

    char buf[140];
    epaper.setFont(FONT_12x16);
    epaper.setCursor(40, 215);
    snprintf(buf, sizeof(buf), "I2C devices: %d    Touch GT911: %s    Battery: %u mV %u%%",
             found, touchOk ? "YES" : "no", mv, pct);
    epaper.print(buf);

    epaper.setCursor(40, 250);
    epaper.print("TAP THE SCREEN - taps are printed over serial and drawn below");
    epaper.drawRect(38, 280, 884, 220, 0);

    epaper.fullUpdate();
    Serial.println("fullUpdate done");

    lightSet(150);          // leave it on, steady
    Serial.println("frontlight set to 150/255 and held");
    Serial.println("=== READY - tap the screen ===");
}

void loop() {
    static uint32_t last = 0;
    uint8_t st = 0;
    if (gtRead16(0x814E, &st, 1) && (st & 0x80)) {
        int n = st & 0x0F;
        for (int i = 0; i < n && i < 5; i++) {
            uint8_t p[8];
            if (!gtRead16(0x8150 + i * 8, p, 8)) break;
            int x = p[1] | (p[2] << 8);
            int y = p[3] | (p[4] << 8);
            Serial.printf("TOUCH %d: x=%d y=%d\n", i, x, y);
            epaper.fillCircle(x, y, 10, 0);
        }
        uint8_t z = 0;
        gtWrite16(0x814E, &z, 1);
        if (n && millis() - last > 700) {
            last = millis();
            epaper.partialUpdate(false);
        }
    }
    delay(20);
}
