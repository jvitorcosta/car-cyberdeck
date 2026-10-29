#pragma once
#include <cstddef>
#include <cstdint>
// What the OBD deck makes of the link's readings: the health Watch (carhealth), this power-on's trip
// (tripsum) and its log on flash (triplog, carfs), the fuel book (fuelbook), and the /car snapshot
// (carwire) built from them once a second. NVS namespace "car": the boot sequence "tseq", the last
// trip "last" (its summary + cost + the carhealth Memo, so a power cut mid-trip is judged at the next
// boot), the fuel book "fuel" and the last trouble-code counts "codes" (a code is "new" against them).
// The loop task runs begin / second / checkpoint; the server task only calls carJson and fill.
namespace carstate {
// setup(), before the tasks: boot sequence +1, the log's recover + cap + open, the Memo restored and
// judged (endTrip), the last trip and the fuel book loaded.
void begin();
// Once a second (loop): a trip sample from the fresh readings, the Watch, the fuel burnt, a closed
// minute to the log and the book's km/L, then the snapshot (seq + 1).
void second();
// Every 30 s (loop), and before an update reboots the deck: the log flushed, the last-trip record
// and the fuel book written when they changed.
void checkpoint();
// The latest snapshot as carwire JSON (GET /car). Returns its length (0: none yet).
size_t carJson(char* out, size_t n);
// GET /fill: a full tank, at pricePerL when it's inside fuelbook's range (NAN: none said). Saved to
// NVS at once. Writes the book's price after it ("6.49", or "none") to priceOut.
void fill(float pricePerL, char* priceOut, size_t n);
// The [car] line (every 10 s).
void logLine();
}
