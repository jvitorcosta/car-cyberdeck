// Standalone PS3 (DualShock 3) controller test on the yellow deck (CYD). Not the firmware. The
// DS3 speaks Bluetooth Classic only, so this runs Bluedroid (esp32-ps3), not NimBLE: it can't
// share a firmware with the LED panel's BLE code. The controller connects to the address stored in
// it (its "master"); write the deck's Bluetooth address there over USB first (68:09:47:F8:25:9E
// for this deck: base MAC + 2). Then press PS on the controller.
//   pio run -e ps3test -t upload -t monitor
#include <Arduino.h>
#include <Ps3Controller.h>

static void onInput() {
    const auto& d = Ps3.event.button_down;
    struct { bool down; const char* name; } presses[] = {
        {d.cross, "cross"},   {d.circle, "circle"}, {d.square, "square"}, {d.triangle, "triangle"},
        {d.up, "up"},         {d.down, "down"},     {d.left, "left"},     {d.right, "right"},
        {d.l1, "L1"},         {d.r1, "R1"},         {d.l2, "L2"},         {d.r2, "R2"},
        {d.start, "start"},   {d.select, "select"}, {d.ps, "PS"},
    };
    for (const auto& p : presses) {
        if (p.down) Serial.printf("[ps3] %s\n", p.name);
    }
}

static void onConnect() { Serial.println("[ps3] connected"); }

void setup() {
    Serial.begin(115200);
    Ps3.attach(onInput);
    Ps3.attachOnConnect(onConnect);
    Ps3.begin();
    Serial.printf("\n[ps3] waiting for the controller (press PS); deck address %s\n", Ps3.getAddress().c_str());
}

void loop() {
    static bool was = false;
    const bool is = Ps3.isConnected();
    if (was && !is) Serial.println("[ps3] disconnected");
    was = is;
    delay(50);
}
