#include "reqauth.h"
#include <Arduino.h>
#include <WebServer.h>
#include <esp_system.h>
#include "clock.h"
#include "remote_config.h"
#include "../../util/wire/hmac256.h"

namespace reqauth {
static bool s_ok = false;                     // HMAC's self-test passed (begin)
static sigreq::Verifier s_verifier(REMOTE_TOKEN);   // the server task's only

void begin() {
    s_ok = hmac256::selfTest();
    if (!s_ok) Serial.println("[auth] HMAC self-test FAILED: signed requests off");
}

bool pass(WebServer& s, const remotegate::Gate& gate, const char* path, const char* wrongKeyText) {
    if (s.hasArg("k") && gate.keyOk(s.arg("k").c_str())) return true;
    const char* why = nullptr;   // stays null without a signature: the usual 403, not worth a log line
    if (!s.hasArg("sig")) {
        // neither key nor signature
    } else if (!s_ok || !gate.enabled()) {
        why = "signing off";
    } else {
        // Every param but WebServer's own "plain" (a POST body), as the server decoded it.
        static String names[sigreq::MAX_PARAMS + 4], values[sigreq::MAX_PARAMS + 4];   // the server task's only
        sigreq::Param params[sigreq::MAX_PARAMS + 4];
        const int total = s.args();
        int count = 0;
        bool tooMany = false;
        for (int i = 0; i < total; ++i) {
            if (s.argName(i) == "plain") continue;
            if (count == (int)(sizeof params / sizeof params[0])) { tooMany = true; break; }
            names[count] = s.argName(i);
            values[count] = s.arg(i);
            params[count] = {names[count].c_str(), values[count].c_str()};
            ++count;
        }
        int64_t now = 0;
        const bool synced = netclock::unixNow(&now);
        const String t = s.arg("t"), n = s.arg("n"), sig = s.arg("sig");
        const sigreq::Verdict v = tooMany ? sigreq::Verdict::Malformed
                                          : s_verifier.check(s.method() == HTTP_GET ? "GET" : "POST", path, params,
                                                             count, s.hasArg("t") ? t.c_str() : nullptr,
                                                             s.hasArg("n") ? n.c_str() : nullptr, sig.c_str(), synced, now);
        if (v == sigreq::Verdict::Ok) return true;
        if (v == sigreq::Verdict::NoClock) {
            Serial.printf("[auth] %s signed before the clock is set: refused\n", path);
            s.send(503, "text/plain", "clock not set");
            return false;
        }
        why = sigreq::name(v);
    }
    if (why) Serial.printf("[auth] %s from %s refused: %s\n", path, s.client().remoteIP().toString().c_str(), why);
    s.send(403, "text/plain", wrongKeyText);
    return false;
}

bool signGet(const char* path, const sigreq::Param* params, int count, char* out, size_t n) {
    if (n) out[0] = 0;
    int64_t now = 0;
    if (!s_ok || !netclock::unixNow(&now)) return false;
    const uint64_t nonce = (uint64_t)esp_random() << 32 | esp_random();
    return sigreq::sign(REMOTE_TOKEN, "GET", path, params, count, now, nonce, out, n);
}
}
