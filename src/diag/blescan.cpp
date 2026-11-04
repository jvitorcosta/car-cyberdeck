// Standalone BLE scanner for the car's LED controllers (Lotus Lantern, Magic Carlight). Not the
// firmware. Scans for 20 s, lists every advertiser, then connects to each one that isn't a phone or
// PC and dumps its
// GATT table (services, characteristics, properties, readable values). Press RESET to scan again.
// Close the LED apps (or turn the iPhone's Bluetooth off) first: a connected controller stops
// advertising.
//   pio run -e blescan -t upload -t monitor
#include <Arduino.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"            // NimBLE 2's headers;
#pragma GCC diagnostic ignored "-Wdouble-promotion"  // src/ builds with -Werror
#include <NimBLEDevice.h>
#pragma GCC diagnostic pop

#include <map>
#include <string>

static constexpr uint32_t SCAN_SECONDS = 20;
// The GATT dump of the car's devices is done (no LED controller accepted a connection); off for now.
static constexpr bool DUMP_GATT = false;

// Apple (0x004C) and Microsoft (0x0006) advertisers are phones and PCs, not LED controllers.
static bool isPhoneOrPc(const NimBLEAdvertisedDevice* dev) {
    if (!dev->haveManufacturerData()) return false;
    std::string mfr = dev->getManufacturerData();
    if (mfr.size() < 2) return false;
    uint16_t company = static_cast<uint8_t>(mfr[0]) | (static_cast<uint8_t>(mfr[1]) << 8);
    return company == 0x004C || company == 0x0006;
}

static void printHex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) Serial.printf("%02X ", data[i]);
}

static void printProps(const NimBLERemoteCharacteristic* ch) {
    if (ch->canRead()) Serial.print("read ");
    if (ch->canWrite()) Serial.print("write ");
    if (ch->canWriteNoResponse()) Serial.print("write-no-resp ");
    if (ch->canNotify()) Serial.print("notify ");
    if (ch->canIndicate()) Serial.print("indicate ");
}

static void dumpGatt(const NimBLEAdvertisedDevice* dev) {
    Serial.printf("\n=== %s  [%s] ===\n", dev->getName().c_str(), dev->getAddress().toString().c_str());
    NimBLEClient* client = NimBLEDevice::createClient();
    client->setConnectTimeout(5000);  // NimBLE 2: milliseconds
    bool connected = false;
    for (int attempt = 0; attempt < 3 && !connected; attempt++) connected = client->connect(dev);
    if (!connected) {
        Serial.println("  connect failed (connected to a phone? out of range?)");
        NimBLEDevice::deleteClient(client);
        return;
    }
    for (NimBLERemoteService* svc : client->getServices(true)) {
        Serial.printf("  service %s\n", svc->getUUID().toString().c_str());
        for (NimBLERemoteCharacteristic* ch : svc->getCharacteristics(true)) {
            Serial.printf("    char %s  handle %u  ", ch->getUUID().toString().c_str(), ch->getHandle());
            printProps(ch);
            if (ch->canRead()) {
                auto value = ch->readValue();
                Serial.print(" = ");
                printHex(reinterpret_cast<const uint8_t*>(value.data()), value.length());
            }
            Serial.println();
        }
    }
    client->disconnect();
    NimBLEDevice::deleteClient(client);
}

// Prints an advertiser's raw payload whenever it differs from the last one seen from that address.
class SniffCallbacks : public NimBLEScanCallbacks {
    std::map<std::string, std::string> last_;

    void onResult(const NimBLEAdvertisedDevice* dev) override {
        if (isPhoneOrPc(dev)) return;
        std::string addr = dev->getAddress().toString();
        const std::vector<uint8_t>& raw = dev->getPayload();
        std::string payload(raw.begin(), raw.end());
        auto it = last_.find(addr);
        if (it != last_.end() && it->second == payload) return;
        if (last_.size() > 200) last_.clear();
        last_[addr] = payload;
        Serial.printf("%8lu  %s  %s rssi %4d  ", static_cast<unsigned long>(millis()), addr.c_str(),
                      dev->isConnectable() ? "conn " : "nconn", dev->getRSSI());
        printHex(raw.data(), raw.size());
        Serial.println();
    }
};

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n[blescan] scanning...");
    NimBLEDevice::init("");
    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setActiveScan(true);  // ask for scan responses: many controllers only send the name there
    scan->setInterval(100);
    scan->setWindow(99);
    NimBLEScanResults results = scan->getResults(SCAN_SECONDS * 1000, false);  // blocks

    Serial.printf("\n[blescan] %d devices\n", results.getCount());
    for (const NimBLEAdvertisedDevice* dev : results) {
        Serial.printf("%s  rssi %4d  name '%s'", dev->getAddress().toString().c_str(), dev->getRSSI(),
                      dev->getName().c_str());
        for (size_t s = 0; s < dev->getServiceUUIDCount(); s++)
            Serial.printf("  svc %s", dev->getServiceUUID(s).toString().c_str());
        if (dev->haveManufacturerData()) {
            std::string mfr = dev->getManufacturerData();
            Serial.print("  mfr ");
            printHex(reinterpret_cast<const uint8_t*>(mfr.data()), mfr.length());
        }
        Serial.println();
    }

    // Some controllers advertise without a name, so dump everything except phones and PCs.
    if (DUMP_GATT) {
        for (const NimBLEAdvertisedDevice* dev : results) {
            if (!isPhoneOrPc(dev)) dumpGatt(dev);
        }
    }
    Serial.println("\n[blescan] done. Sniffing advertisements (printed when they change)...");

    // Sniff: some controllers take their commands from the phone's advertisements, not a connection.
    scan->clearResults();
    scan->setScanCallbacks(new SniffCallbacks(), true);
    scan->setDuplicateFilter(false);
    scan->setActiveScan(false);
    scan->start(0, false);
}

void loop() { delay(1000); }
