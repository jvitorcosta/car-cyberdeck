#pragma once
#include <cstdint>
// Who may play a clip from the phone remote (net/remote), and when. Pure.
//   wrong or missing key -> BadKey (403; checked first, so a stranger learns nothing else)
//   a clip is playing    -> Busy (409; queued clips would each hold a task stack on the CYD)
//   < MIN_GAP_MS since the last accepted play -> TooSoon (429)
// No key configured (REMOTE_TOKEN empty) turns the remote off: every request is BadKey.
// Accept::SignedOnly (between decks: the OBD deck's /car and /fill) refuses the bearer key itself:
// keyOk() is always false, so only a signature (common/net/reqauth) passes; enabled() still follows
// the key, which signatures are checked against.
namespace remotegate {
constexpr uint32_t MIN_GAP_MS = 3000;
enum class Result { Play, BadKey, Busy, TooSoon };
enum class Accept { KeyOrSigned, SignedOnly };

class Gate {
public:
    explicit Gate(const char* key, Accept a = Accept::KeyOrSigned) : key_(key), signedOnly_(a == Accept::SignedOnly) {}
    bool enabled() const { return key_ && key_[0]; }
    // Key check only (GET /state): never counts as a play. Same constant-time compare.
    bool keyOk(const char* key) const;
    Result request(const char* key, bool clipPlaying, uint32_t nowMs);
private:
    const char* key_;
    bool signedOnly_;
    bool played_ = false;
    uint32_t lastPlayMs_ = 0;
};
}
