#pragma once
// The yellow deck's BLE links to the car's two ELK LED controllers, on Bluedroid's GATT client
// (ESP-IDF's esp_gattc API), which shares the stack with the PS3 controller's Bluetooth Classic.
// One GATT client app is registered once; each light is a direct connection, its write
// characteristic (service FFF0, characteristic FFF3) found once per connection and kept as a
// handle, so a reconnect never frees anything a writer could still be holding.
// connect() blocks (the link task calls it); write() and up() are safe from any task.
#include <cstddef>
#include <cstdint>

namespace elklink {
constexpr int MAX = 2;

// After Bluedroid is enabled (the PS3 controller's begin): registers the GATT client, +9 dBm.
// False when the stack refused (logged); every connect then fails.
bool begin();

// Light `i` is the controller at `mac` ("aa:bb:cc:dd:ee:ff", public address).
void setAddress(int i, const char* mac);

// Connects light i if it isn't, then finds FFF3 if it hasn't. Up to ~35 s for a light out of
// range (the stack's own 30 s connect timeout) plus up to 10 s of discovery. True when up().
bool connect(int i, const char* label);

// Connected, with FFF3 found.
bool up(int i);

// A write without response to FFF3; false when the light isn't up or the stack refused it.
bool write(int i, const uint8_t* data, size_t len);
}
