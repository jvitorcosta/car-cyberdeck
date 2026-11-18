// A fake Vgate iCar Pro 2S on the City, driven through the same pieces the link task uses
// (obd::allowed, obd::setupOk, obd::parseSupported, obdplan::Plan / Health, obd::parse) and
// published like obdlink::publish (obdwire::slot), until the HUD's values come out of
// obdwire::fromReadings. No Bluetooth: FakeElm answers one command line as the adapter would,
// with what it sends before its '>' prompt ('\r' already turned into '\n', like obdlink's ask()).
#include <unity.h>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "../../src/util/car/obd.h"
#include "../../src/util/car/obdplan.h"
#include "../../src/util/car/obdwire.h"

using obd::Pid;
using obd::Reply;
using obdplan::Link;

void setUp() {}
void tearDown() {}

namespace {
// What the City answered on 2026-10-19 (tools/obd_sim.py's CITY_PIDS): no 05 / 0F (67 / 68).
const uint8_t CITY[] = {0x01, 0x03, 0x04, 0x06, 0x07, 0x0B, 0x0C, 0x0D, 0x0E, 0x10, 0x11, 0x13,
                        0x15, 0x1C, 0x1F, 0x21, 0x23, 0x2E, 0x30, 0x31, 0x33, 0x34, 0x41, 0x42,
                        0x43, 0x44, 0x45, 0x46, 0x47, 0x49, 0x4A, 0x4C, 0x52, 0x56, 0x67, 0x68, 0x6C};

struct Car {
    bool engine = true;    // false: ignition on, engine off (the ECUs say NO DATA)
    float rpm = 2450.0f, kmh = 62.0f, coolC = 88.0f, intakeC = 34.0f;
    int throttleRaw = 47;  // 47 / 255 = 18.4 %
    int loadRaw = 105;     // 105 / 255 = 41.2 %
    float volts = 14.1f;
};

class FakeElm {
public:
    Car car;
    bool cutNext = false;   // the next mode 01 reply is cut short (a dropped notification)
    int sent = 0;
    std::string log;        // every command, for the read-only check

    std::string answer(const char* cmd) {
        ++sent;
        log += cmd;
        std::string c(cmd);
        if (!c.empty() && c.back() == '\r') c.pop_back();
        std::string echo = echo_ ? c + "\n" : "";
        if (c == "ATZ") {
            echo_ = true;   // a reset turns the echo back on
            return echo + "\n\nELM327 v2.3\n";
        }
        if (c == "ATE0") {
            echo_ = false;
            return echo + "OK";
        }
        if (c.rfind("AT", 0) == 0 && c != "ATRV") return echo + "OK";
        if (c == "ATRV") return echo + fmt("%.1fV", car.volts);
        if (c.size() == 4 && c.rfind("01", 0) == 0) {
            std::string pre = echo;
            if (!searched_) {   // the first request after ATSP0
                searched_ = true;
                pre += "SEARCHING...\n";
            }
            const int pid = (int)strtol(c.c_str() + 2, nullptr, 16);
            std::string r = mode01(pid);
            if (cutNext && r.size() > 5) {   // only "41 0C" (or "410D3") arrived
                cutNext = false;
                r = r.substr(0, 5);
            }
            return pre + r;
        }
        if (c == "03" || c == "07") return echo + (car.engine ? (c == "03" ? "43 00" : "47 00") : "NO DATA");
        return echo + "?";
    }

private:
    bool echo_ = true, searched_ = false;

    static std::string fmt(const char* f, ...) {
        char b[64];
        va_list a;
        va_start(a, f);
        vsnprintf(b, sizeof b, f, a);
        va_end(a);
        return b;
    }
    static bool city(int pid) {
        for (uint8_t p : CITY)
            if (p == pid) return true;
        return false;
    }
    static std::string support(int block) {
        uint32_t bits = 0;
        bool more = false;
        for (uint8_t p : CITY) {
            if (p > block && p <= block + 0x20) bits |= 1u << (31 - (p - block - 1));
            if (p > block + 0x20) more = true;
        }
        if (more) bits |= 1u;
        return fmt("41 %02X %02X %02X %02X %02X", block, (unsigned)(bits >> 24) & 0xFF,
                   (unsigned)(bits >> 16) & 0xFF, (unsigned)(bits >> 8) & 0xFF, (unsigned)bits & 0xFF);
    }
    std::string mode01(int pid) {
        if (!car.engine) return "NO DATA";
        if (pid % 0x20 == 0 && pid <= 0x60) {
            // the engine and the gearbox both answer the scan, on their own lines
            return pid == 0 ? support(0) + "\n41 00 98 18 00 01" : support(pid);
        }
        if (!city(pid)) return "NO DATA";
        const int rpm4 = (int)(car.rpm * 4);
        switch (pid) {
            case 0x0C: return fmt("41 0C %02X %02X", (rpm4 >> 8) & 0xFF, rpm4 & 0xFF);
            case 0x0D: return fmt("410D%02X", (int)car.kmh);   // some replies come without spaces
            case 0x11: return fmt("41 11 %02X", car.throttleRaw);
            case 0x04: return fmt("41 04 %02X", car.loadRaw);
            case 0x67: return fmt("41 67 01 %02X 00", (int)car.coolC + 40);
            case 0x68: return fmt("41 68 01 %02X 00 00", (int)car.intakeC + 40);
            default: return fmt("41 %02X 00 00", pid);
        }
    }
};

// The link's session, minus the radio: what obdlink.cpp's setup / supportScan / ask loop does.
struct Session {
    FakeElm& elm;
    obdplan::Plan plan;
    obdplan::Health health;
    float value[(int)Pid::COUNT] = {};
    bool have[(int)Pid::COUNT] = {};
    uint32_t atMs[(int)Pid::COUNT] = {};
    uint32_t now = 1000;
    int errors = 0;

    explicit Session(FakeElm& e) : elm(e) {}

    std::string ask(const char* cmd) {
        TEST_ASSERT_TRUE_MESSAGE(obd::allowed(cmd), cmd);   // the one gate to the car
        now += 60;                                          // a BLE round trip
        return elm.answer(cmd);
    }
    bool setup() {
        for (const char* c : {"ATZ\r", "ATE0\r", "ATL0\r", "ATS0\r", "ATH0\r", "ATSP0\r"})
            if (!obd::setupOk(c, ask(c).c_str())) return false;
        return true;
    }
    bool scan() {
        uint32_t bits[4] = {};
        for (int b = 0; b < 4; ++b) {
            const std::string raw = ask(obd::supportRequest(b));
            if (!obd::parseSupported(b, raw.c_str(), &bits[b])) {
                if (b == 0) return false;
                for (int k = b; k < 4; ++k) bits[k] = 0xFFFFFFFFu;
                break;
            }
            if (!(bits[b] & 1u)) break;
        }
        plan.setSupported(bits);
        plan.codesDone(now);   // the codes aren't this test's business
        return true;
    }
    uint32_t batteryAt = 0;
    void step() {
        // obdlink's session(): until the engine runs the battery ~9 in 10 (the crank dip), then
        // ATRV at least every 4 s on top of the plan
        const float rpm = have[(int)Pid::Rpm] ? value[(int)Pid::Rpm] : NAN;
        const bool waiting = health.state() == Link::NoEngine || !(rpm > 0.0f);
        plan.crankWatch(waiting);
        const Pid p = !waiting && now - batteryAt >= 4000 ? Pid::Battery : plan.next(now);
        if (p == Pid::Battery) batteryAt = now;
        const std::string raw = ask(obd::request(p));
        float v = 0.0f;
        const Reply r = obd::parse(p, raw.c_str(), &v);
        if (r == Reply::Error) ++errors;
        plan.done(p, r, now);
        health.reply(p, r);
        if (r == Reply::Ok) {   // obdlink::publish
            const Pid s = obdwire::slot(p);
            value[(int)s] = v;
            have[(int)s] = true;
            atMs[(int)s] = now;
        }
    }
    obdwire::Live live() const { return obdwire::fromReadings(health.state(), value, have, atMs, now); }
    void runMs(uint32_t ms) {
        const uint32_t end = now + ms;
        while (now < end) step();
    }
};
}  // namespace

void test_setup_answers_through_echo_and_banner() {
    FakeElm elm;
    Session s(elm);
    TEST_ASSERT_TRUE(s.setup());
}

void test_supported_scan_reads_both_ecus() {
    FakeElm elm;
    Session s(elm);
    TEST_ASSERT_TRUE(s.setup());
    TEST_ASSERT_TRUE(s.scan());   // SEARCHING... on the first block, two ECUs on 0100
}

void test_the_hud_values_arrive() {
    FakeElm elm;
    Session s(elm);
    TEST_ASSERT_TRUE(s.setup());
    TEST_ASSERT_TRUE(s.scan());
    s.runMs(25000);   // the slow values come round every ~22 s
    const obdwire::Live l = s.live();
    TEST_ASSERT_EQUAL_INT((int)Link::Live, (int)l.link);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 2450.0f, l.rpm);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 62.0f, l.kmh);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 88.0f, l.coolC);     // from 0167: the City has no 0105
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 34.0f, l.intakeC);   // from 0168
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 18.4f, l.throttle);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 41.2f, l.load);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 14.1f, l.volts);
    TEST_ASSERT_EQUAL_INT(0, s.errors);
    // read-only: never mode 04, never headers on
    size_t from = 0;
    for (size_t cr; (cr = elm.log.find('\r', from)) != std::string::npos; from = cr + 1) {
        const std::string c = elm.log.substr(from, cr - from);
        TEST_ASSERT_TRUE_MESSAGE(c == "03" || c == "07" || c.rfind("01", 0) == 0 || c.rfind("AT", 0) == 0, c.c_str());
        TEST_ASSERT_TRUE(c != "04" && c != "ATH1");
    }
}

void test_rpm_and_speed_follow_the_car() {
    FakeElm elm;
    Session s(elm);
    s.setup();
    s.scan();
    s.runMs(3000);
    elm.car.rpm = 5800.0f;
    elm.car.kmh = 118.0f;
    s.runMs(1000);   // the fast set comes round within a second
    const obdwire::Live l = s.live();
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 5800.0f, l.rpm);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 118.0f, l.kmh);
}

void test_a_cut_reply_is_dropped_not_misread() {
    FakeElm elm;
    Session s(elm);
    s.setup();
    s.scan();
    s.runMs(2000);
    elm.car.rpm = 3000.0f;
    elm.cutNext = true;
    for (int i = 0; i < 5 && elm.cutNext; ++i) s.step();   // the next mode 01 reply is cut
    TEST_ASSERT_FALSE(elm.cutNext);
    TEST_ASSERT_EQUAL_INT(1, s.errors);   // read as an error, never as a number
    TEST_ASSERT_FALSE(s.health.lost());   // one error isn't a lost adapter
    s.runMs(1000);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 3000.0f, s.live().rpm);
}

void test_engine_off_reads_key_on_and_values_go_stale() {
    FakeElm elm;
    Session s(elm);
    s.setup();
    s.scan();
    s.runMs(3000);
    TEST_ASSERT_EQUAL_INT((int)Link::Live, (int)s.live().link);
    elm.car.engine = false;   // the City stalls: the ECUs stop answering, the adapter doesn't
    elm.car.volts = 12.4f;
    s.runMs(4000);
    const obdwire::Live l = s.live();
    TEST_ASSERT_EQUAL_INT((int)Link::NoEngine, (int)l.link);
    TEST_ASSERT_TRUE(std::isnan(l.rpm));     // past STALE_FAST_MS
    TEST_ASSERT_TRUE(std::isnan(l.kmh));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 12.4f, l.volts);   // ATRV still answers
}

void test_the_wire_carries_the_session() {
    FakeElm elm;
    Session s(elm);
    s.setup();
    s.scan();
    s.runMs(25000);
    obdwire::Live l = s.live();
    l.seq = 7;
    char json[obdwire::MAX_JSON];
    TEST_ASSERT_TRUE(obdwire::encode(l, json, sizeof json) > 0);
    obdwire::Live back;
    TEST_ASSERT_TRUE(obdwire::decode(json, &back));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 2450.0f, back.rpm);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 88.0f, back.coolC);
    TEST_ASSERT_EQUAL_UINT32(7, back.seq);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_setup_answers_through_echo_and_banner);
    RUN_TEST(test_supported_scan_reads_both_ecus);
    RUN_TEST(test_the_hud_values_arrive);
    RUN_TEST(test_rpm_and_speed_follow_the_car);
    RUN_TEST(test_a_cut_reply_is_dropped_not_misread);
    RUN_TEST(test_engine_off_reads_key_on_and_values_go_stale);
    RUN_TEST(test_the_wire_carries_the_session);
    return UNITY_END();
}
