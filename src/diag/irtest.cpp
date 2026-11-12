// Standalone IR receiver test on the yellow deck (CYD). Not the firmware. A KY-022 (VS1838B, 38 kHz)
// on the CN1 connector: 3.3V to its middle pin, IO27 to S, GND to "-" (IO22 unused). Press each
// button on a remote: the screen shows its protocol, address and command, and serial logs a line
// per press, so the codes can be mapped to the panel's actions later.
//   pio run -e irtest -t upload -t monitor
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#include <IRremote.hpp>  // header-only: the irtest env drops src/'s -Werror for it

#include "pins.h"

static const int PIN_IR = 27;  // CN1, next to the RGB LED

static TFT_eSPI s_tft;
static uint32_t s_presses = 0;

static void drawWaiting() {
    s_tft.fillScreen(TFT_BLACK);
    s_tft.setTextColor(TFT_GREEN, TFT_BLACK);
    s_tft.drawString("IR TEST", 10, 10, 4);
    s_tft.setTextColor(TFT_WHITE, TFT_BLACK);
    s_tft.drawString("Point the remote at the", 10, 80, 2);
    s_tft.drawString("receiver and press a button", 10, 100, 2);
    s_tft.drawString("KY-022 on CN1, signal on IO27", 10, 210, 2);
}

static void drawPress(const IRData& d, bool repeat) {
    char line[48];
    const char* proto = reinterpret_cast<const char*>(getProtocolString(d.protocol));

    s_tft.fillScreen(TFT_BLACK);
    s_tft.setTextColor(TFT_GREEN, TFT_BLACK);
    s_tft.drawString("IR TEST", 10, 10, 4);
    snprintf(line, sizeof line, "#%lu", static_cast<unsigned long>(s_presses));
    s_tft.drawRightString(line, 310, 10, 4);

    s_tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    s_tft.drawString("COMMAND", 10, 60, 2);
    snprintf(line, sizeof line, "0x%02X", d.command);
    s_tft.drawString(line, 10, 80, 7);  // big 7-segment digits

    s_tft.setTextColor(TFT_WHITE, TFT_BLACK);
    snprintf(line, sizeof line, "%s  addr 0x%04X%s", proto, d.address, repeat ? "  (held)" : "");
    s_tft.drawString(line, 10, 150, 4);
    snprintf(line, sizeof line, "raw 0x%08lX", static_cast<unsigned long>(d.decodedRawData));
    s_tft.drawString(line, 10, 185, 2);
    if (d.protocol == UNKNOWN) {
        s_tft.setTextColor(TFT_RED, TFT_BLACK);
        s_tft.drawString("unknown protocol: try again closer", 10, 210, 2);
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_BL, OUTPUT);
    digitalWrite(PIN_BL, HIGH);
    s_tft.init();
    s_tft.invertDisplay(true);  // CYD panel: colors are inverted without this
    s_tft.setRotation(1);       // landscape 320x240
    drawWaiting();

    IrReceiver.begin(PIN_IR, DISABLE_LED_FEEDBACK);
    Serial.printf("\n[ir] listening on GPIO %d; press buttons on the remote\n", PIN_IR);
}

void loop() {
    if (!IrReceiver.decode()) {
        delay(5);
        return;
    }
    const IRData& d = IrReceiver.decodedIRData;
    const bool repeat = d.flags & IRDATA_FLAGS_IS_REPEAT;
    if (!repeat) s_presses++;
    Serial.printf("[ir] %s addr 0x%04X cmd 0x%02X raw 0x%08lX%s\n",
                  reinterpret_cast<const char*>(getProtocolString(d.protocol)), d.address, d.command,
                  static_cast<unsigned long>(d.decodedRawData), repeat ? " (held)" : "");
    drawPress(d, repeat);
    IrReceiver.resume();
}
