// Standalone Xbox controller test on the yellow deck (CYD). Not the firmware. Pairs with an Xbox
// Series X|S / One (model 1708+) controller over BLE and logs every report: buttons, D-pad,
// sticks, triggers. Put the controller in pairing mode (hold the pair button until the logo flashes
// fast) and make sure no PC or phone is holding it.
//   pio run -e xboxtest -t upload -t monitor
#include <Arduino.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"            // the library's and NimBLE 2's headers;
#pragma GCC diagnostic ignored "-Wdouble-promotion"  // src/ builds with -Werror
#include <XboxSeriesXControllerESP32_asukiaaa.hpp>
#pragma GCC diagnostic pop

static XboxSeriesXControllerESP32_asukiaaa::Core xbox;  // first controller found

void setup() {
    Serial.begin(115200);
    Serial.println("\n[xbox] scanning for a controller in pairing mode...");
    xbox.begin();
}

void loop() {
    xbox.onLoop();
    static bool was = false;
    const bool is = xbox.isConnected();
    if (is != was) {
        was = is;
        Serial.printf("[xbox] %s\n", is ? "connected" : "disconnected");
    }
    if (is && !xbox.isWaitingForFirstNotification()) {
        static String last;
        const String now = xbox.xboxNotif.toString();
        if (now != last) {  // only changes: sticks drift a little, buttons are clean
            last = now;
            Serial.print(now);
        }
    } else if (!is && xbox.getCountFailedConnection() > 2) {
        ESP.restart();  // the library's advice after repeated failed connects
    }
    delay(20);
}
