#include "padhid.h"
#include <cstdlib>
#include <cstring>

// Bluedroid's L2CAP data write and its buffer header (stack/bt_types.h's BT_HDR); the stack takes
// the buffer and frees it (osi_free, i.e. free()) whatever the result.
namespace {
struct BtHdr {
    uint16_t event;
    uint16_t len;
    uint16_t offset;
    uint16_t layer_specific;
};
}  // namespace
extern "C" uint8_t L2CA_DataWrite(uint16_t cid, BtHdr* p_data);

namespace padhid {
namespace {
// What esp32-ps3 1.1.0 uses (ps3_l2cap.c, ps3_int.h, ps3.c): the HID control channel's local CID,
// the room L2CAP and HCI put in front, and the DS3's output report layout.
constexpr uint16_t HIDC_CID = 0x40;
constexpr uint16_t L2CAP_OFFSET = 13;  // L2CAP_MIN_OFFSET
constexpr uint8_t SET_REPORT_OUTPUT = 0x50 | 0x02;
constexpr uint8_t REPORT_CONTROL = 0x01;
constexpr int DATA_LEN = 48;
constexpr int RUMBLE_R_DUR = 1, RUMBLE_R_POW = 2, RUMBLE_L_DUR = 3, RUMBLE_L_POW = 4, LED_MASK = 9;
constexpr int LED_ARGS[4] = {25, 20, 15, 10};  // LED 1..4's timing block
constexpr uint8_t LED_ON[5] = {0xff, 0x27, 0x10, 0x00, 0x32};  // steady
constexpr uint8_t L2CA_FAILED = 0;  // L2CAP_DW_FAILED
}  // namespace

bool send(uint8_t leds, uint32_t rumbleMs, uint8_t intensity) {
    uint8_t report[2 + DATA_LEN] = {SET_REPORT_OUTPUT, REPORT_CONTROL};
    uint8_t* d = report + 2;
    if (rumbleMs) {
        const uint32_t units = rumbleMs / 20;
        d[RUMBLE_R_DUR] = d[RUMBLE_L_DUR] = static_cast<uint8_t>(units > 254 ? 254 : units ? units : 1);
        d[RUMBLE_R_POW] = d[RUMBLE_L_POW] = intensity;
    }
    for (int i = 0; i < 4; i++) {
        if (!(leds & (1 << i))) continue;
        d[LED_MASK] |= static_cast<uint8_t>(1 << (i + 1));
        memcpy(d + LED_ARGS[i], LED_ON, sizeof(LED_ON));
    }
    auto* buf = static_cast<BtHdr*>(malloc(sizeof(BtHdr) + L2CAP_OFFSET + sizeof(report)));
    if (!buf) return false;  // no memory: this one report is skipped, never written through NULL
    buf->event = 0;
    buf->layer_specific = 0;
    buf->len = sizeof(report);
    buf->offset = L2CAP_OFFSET;
    memcpy(reinterpret_cast<uint8_t*>(buf + 1) + L2CAP_OFFSET, report, sizeof(report));
    return L2CA_DataWrite(HIDC_CID, buf) != L2CA_FAILED;
}
}  // namespace padhid
