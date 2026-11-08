#include "elklink.h"
#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <esp_bt.h>
#include <esp_bt_defs.h>
#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#include <freertos/event_groups.h>

namespace elklink {
namespace {
// One light. The stack's task writes every field (its events); the link task and the loop read.
struct Link {
    esp_bd_addr_t bda = {};
    volatile bool open = false;       // connected (OPEN_EVT ok, no DISCONNECT_EVT since)
    volatile uint16_t conn = 0;
    volatile uint16_t handle = 0;     // FFF3's value handle on this connection; 0: not found yet
    volatile esp_gatt_status_t status = ESP_GATT_OK;  // the last OPEN_EVT's / SEARCH_CMPL_EVT's
};
Link s_link[MAX];
volatile esp_gatt_if_t s_if = ESP_GATT_IF_NONE;
EventGroupHandle_t s_ev = nullptr;

// Event bits: the app registered, and per light its open and its discovery ended.
constexpr EventBits_t REG = 1 << 0;
constexpr EventBits_t openBit(int i) { return 1 << (1 + i); }
constexpr EventBits_t searchBit(int i) { return 1 << (1 + MAX + i); }

constexpr uint16_t APP_ID = 0x4C;  // any; there is only this one GATT client app
constexpr uint32_t OPEN_WAIT_MS = 35000;    // the stack gives up on its own after 30 s
constexpr uint32_t SEARCH_WAIT_MS = 10000;

int byBda(const esp_bd_addr_t bda) {
    for (int i = 0; i < MAX; i++) {
        if (memcmp(s_link[i].bda, bda, sizeof(esp_bd_addr_t)) == 0) return i;
    }
    return -1;
}

int byConn(uint16_t conn) {
    for (int i = 0; i < MAX; i++) {
        if (s_link[i].open && s_link[i].conn == conn) return i;
    }
    return -1;
}

esp_bt_uuid_t uuid16(uint16_t u) {
    esp_bt_uuid_t id = {};
    id.len = ESP_UUID_LEN_16;
    id.uuid.uuid16 = u;
    return id;
}

void onGattc(esp_gattc_cb_event_t event, esp_gatt_if_t gattcIf, esp_ble_gattc_cb_param_t* p) {
    switch (event) {
    case ESP_GATTC_REG_EVT:
        if (p->reg.app_id == APP_ID && p->reg.status == ESP_GATT_OK) s_if = gattcIf;
        xEventGroupSetBits(s_ev, REG);
        break;
    case ESP_GATTC_OPEN_EVT: {
        const int i = byBda(p->open.remote_bda);
        if (i < 0) break;
        Link& l = s_link[i];
        l.status = p->open.status;
        if (p->open.status == ESP_GATT_OK) {
            l.conn = p->open.conn_id;
            l.handle = 0;
            l.open = true;
        }
        xEventGroupSetBits(s_ev, openBit(i));
        break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
        const int i = byConn(p->search_cmpl.conn_id);
        if (i < 0) break;
        s_link[i].status = p->search_cmpl.status;
        xEventGroupSetBits(s_ev, searchBit(i));
        break;
    }
    case ESP_GATTC_DISCONNECT_EVT: {
        const int i = byBda(p->disconnect.remote_bda);
        if (i < 0) break;
        s_link[i].open = false;
        s_link[i].handle = 0;
        // a discovery waiting on this link ends now, not at its timeout
        xEventGroupSetBits(s_ev, searchBit(i));
        break;
    }
    default:
        break;
    }
}

// FFF3's handle inside FFF0, from the stack's attribute cache after a discovery; 0 when missing.
uint16_t findTx(uint16_t conn) {
    esp_bt_uuid_t svcId = uuid16(0xFFF0), chrId = uuid16(0xFFF3);
    esp_gattc_service_elem_t svc;
    uint16_t n = 1;
    if (esp_ble_gattc_get_service(s_if, conn, &svcId, &svc, &n, 0) != ESP_GATT_OK || n == 0) return 0;
    esp_gattc_char_elem_t chr;
    n = 1;
    if (esp_ble_gattc_get_char_by_uuid(s_if, conn, svc.start_handle, svc.end_handle, chrId, &chr, &n) != ESP_GATT_OK ||
        n == 0) {
        return 0;
    }
    return chr.char_handle;
}
}  // namespace

bool begin() {
    s_ev = xEventGroupCreate();
    esp_err_t rc = esp_ble_gattc_register_callback(onGattc);
    if (rc == ESP_OK) rc = esp_ble_gattc_app_register(APP_ID);
    if (rc != ESP_OK) {
        Serial.printf("[leds] GATT client: %s\n", esp_err_to_name(rc));
        return false;
    }
    xEventGroupWaitBits(s_ev, REG, pdTRUE, pdTRUE, pdMS_TO_TICKS(5000));
    if (s_if == ESP_GATT_IF_NONE) {
        Serial.println("[leds] GATT client: not registered");
        return false;
    }
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);  // +9 dBm, as before
    return true;
}

void setAddress(int i, const char* mac) {
    uint8_t* b = s_link[i].bda;
    if (sscanf(mac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6) {
        memset(b, 0, sizeof(esp_bd_addr_t));
    }
}

bool connect(int i, const char* label) {
    if (s_if == ESP_GATT_IF_NONE) return false;
    Link& l = s_link[i];
    if (!l.open) {
        xEventGroupClearBits(s_ev, openBit(i));
        const esp_err_t rc = esp_ble_gattc_open(s_if, l.bda, BLE_ADDR_TYPE_PUBLIC, true);
        if (rc != ESP_OK) {
            Serial.printf("[leds] %s: open %s\n", label, esp_err_to_name(rc));
            return false;
        }
        xEventGroupWaitBits(s_ev, openBit(i), pdTRUE, pdTRUE, pdMS_TO_TICKS(OPEN_WAIT_MS));
        if (!l.open) {
            Serial.printf("[leds] %s: connect failed (%d)\n", label, static_cast<int>(l.status));
            return false;
        }
    }
    if (!l.handle) {
        const uint16_t conn = l.conn;
        xEventGroupClearBits(s_ev, searchBit(i));
        esp_bt_uuid_t svcId = uuid16(0xFFF0);
        if (esp_ble_gattc_search_service(s_if, conn, &svcId) == ESP_OK) {
            xEventGroupWaitBits(s_ev, searchBit(i), pdTRUE, pdTRUE, pdMS_TO_TICKS(SEARCH_WAIT_MS));
        }
        if (l.open) l.handle = findTx(conn);
        if (!l.handle) {  // connected but discovery failed (e.g. a GATT timeout): drop it, try again later
            Serial.printf("[leds] %s: no FFF3, disconnecting\n", label);
            if (l.open) esp_ble_gattc_close(s_if, conn);
            return false;
        }
    }
    return true;
}

bool up(int i) { return s_link[i].open && s_link[i].handle; }

bool write(int i, const uint8_t* data, size_t len) {
    const Link& l = s_link[i];
    const uint16_t conn = l.conn, handle = l.handle;  // one read each: the stack may be dropping it
    if (!l.open || !handle) return false;
    uint8_t buf[20];
    if (len > sizeof(buf)) return false;
    memcpy(buf, data, len);
    return esp_ble_gattc_write_char(s_if, conn, handle, static_cast<uint16_t>(len), buf, ESP_GATT_WRITE_TYPE_NO_RSP,
                                    ESP_GATT_AUTH_REQ_NONE) == ESP_OK;
}
}  // namespace elklink
