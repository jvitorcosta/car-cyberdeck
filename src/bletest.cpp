// Standalone control test for the car's two ELK-family LED controllers. Not the firmware.
// Connects to both, dumps their GATT tables, then cycles red, green, blue, off, on, 4 s per step,
// sending both known ELK command variants. Turn the iPhone's Bluetooth off first (Settings).
//   pio run -e bletest -t upload -t monitor
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <TFT_eSPI.h>

static TFT_eSPI tft;

struct Controller {
    const char* name;
    const char* addr;
    NimBLEClient* client = nullptr;
    NimBLERemoteCharacteristic* tx = nullptr;
};

static Controller controllers[] = {
    {"ELK-BLEDDM (exterior?)", "be:68:93:1d:22:00"},
    {"MELK-OA25PQ (interior?)", "be:16:b6:00:9b:ff"},
};

static void printHex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) Serial.printf("%02X ", data[i]);
}

static bool connect(Controller& c) {
    c.client = NimBLEDevice::createClient();
    c.client->setConnectTimeout(5);
    NimBLEAddress addr(std::string(c.addr), BLE_ADDR_PUBLIC);
    bool ok = false;
    for (int attempt = 0; attempt < 3 && !ok; attempt++) ok = c.client->connect(addr);
    if (!ok) {
        Serial.printf("[bletest] %s: connect failed\n", c.name);
        return false;
    }
    Serial.printf("[bletest] %s: connected\n", c.name);
    for (NimBLERemoteService* svc : *c.client->getServices(true)) {
        Serial.printf("  service %s\n", svc->getUUID().toString().c_str());
        for (NimBLERemoteCharacteristic* ch : *svc->getCharacteristics(true)) {
            Serial.printf("    char %s %s%s%s\n", ch->getUUID().toString().c_str(),
                          ch->canWrite() ? "write " : "", ch->canWriteNoResponse() ? "write-no-resp " : "",
                          ch->canNotify() ? "notify" : "");
            // ELK controllers take commands on FFF3 (service FFF0); fall back to the first writable one
            if (ch->getUUID() == NimBLEUUID(static_cast<uint16_t>(0xFFF3))) c.tx = ch;
            else if (!c.tx && (ch->canWrite() || ch->canWriteNoResponse())) c.tx = ch;
        }
    }
    if (c.tx) Serial.printf("  -> commands go to %s\n", c.tx->getUUID().toString().c_str());
    return c.tx != nullptr;
}

static void send(Controller& c, const uint8_t* cmd, size_t len) {
    if (!c.tx || !c.client->isConnected()) return;
    bool ok = c.tx->writeValue(cmd, len, !c.tx->canWriteNoResponse());
    Serial.printf("  %s <- ", c.name);
    printHex(cmd, len);
    Serial.println(ok ? "" : " (write failed)");
}

// Screen: the step's colour as a big swatch, its name and RGB, and each controller's link state.
static void draw(const char* label, uint8_t r, uint8_t g, uint8_t b) {
    uint16_t fill = tft.color565(r, g, b);
    uint16_t ink = (r * 3 + g * 6 + b) / 10 > 140 ? TFT_BLACK : TFT_WHITE;
    tft.fillScreen(TFT_BLACK);
    tft.fillRoundRect(10, 10, 300, 150, 12, fill);
    tft.drawRoundRect(10, 10, 300, 150, 12, TFT_DARKGREY);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(ink, fill);
    tft.drawString(label, 160, 70, 4);
    char rgb[32];
    snprintf(rgb, sizeof(rgb), "R %u  G %u  B %u", r, g, b);
    tft.drawString(rgb, 160, 110, 2);
    tft.setTextDatum(ML_DATUM);
    int y = 185;
    for (const Controller& c : controllers) {
        bool up = c.client && c.client->isConnected();
        tft.fillCircle(20, y, 6, up ? TFT_GREEN : TFT_RED);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(c.name, 34, y, 2);
        y += 28;
    }
}

// Sends one step to both controllers in both ELK command variants (00/04 header and 07 header).
static void step(const char* label, const uint8_t (&a)[9], const uint8_t (&b)[9], uint8_t r = 255,
                 uint8_t g = 255, uint8_t bl = 255) {
    Serial.printf("\n[bletest] === %s ===\n", label);
    draw(label, r, g, bl);
    for (Controller& c : controllers) {
        send(c, a, sizeof(a));
        delay(50);
        send(c, b, sizeof(b));
    }
    delay(4000);
}

static void color(const char* label, uint8_t r, uint8_t g, uint8_t b) {
    const uint8_t v1[9] = {0x7E, 0x00, 0x05, 0x03, r, g, b, 0x00, 0xEF};
    const uint8_t v2[9] = {0x7E, 0x07, 0x05, 0x03, r, g, b, 0x10, 0xEF};
    step(label, v1, v2, r, g, b);
}

void setup() {
    Serial.begin(115200);
    delay(500);
    tft.init();
    tft.setRotation(1);
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    draw("connecting...", 40, 40, 40);
    Serial.println("\n[bletest] connecting...");
    NimBLEDevice::init("");
    for (Controller& c : controllers) connect(c);

    static const uint8_t on1[9] = {0x7E, 0x00, 0x04, 0xF0, 0x00, 0x01, 0xFF, 0x00, 0xEF};
    static const uint8_t on2[9] = {0x7E, 0x04, 0x04, 0xF0, 0x00, 0x01, 0xFF, 0x00, 0xEF};
    static const uint8_t off1[9] = {0x7E, 0x00, 0x04, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xEF};
    static const uint8_t off2[9] = {0x7E, 0x04, 0x04, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xEF};

    step("ON", on1, on2);
    color("RED", 0xFF, 0x00, 0x00);
    color("GREEN", 0x00, 0xFF, 0x00);
    color("BLUE", 0x00, 0x00, 0xFF);
    step("OFF", off1, off2, 0, 0, 0);
    step("ON", on1, on2);
    color("WHITE", 0xFF, 0xFF, 0xFF);

    for (Controller& c : controllers) {
        if (c.client) c.client->disconnect();
    }
    draw("done - RESET to rerun", 40, 40, 40);
    Serial.println("\n[bletest] done. Press RESET to run again.");
}

void loop() { delay(1000); }
