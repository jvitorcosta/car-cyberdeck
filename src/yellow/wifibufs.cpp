// The yellow deck's WiFi buffers, capped. Arduino's WiFi starts the driver (WiFiGeneric's
// wifiLowLevelInit) with up to 32 receive and 32 send buffers of ~1.6 KB each, taken from the heap
// as packets come and go, and receive aggregation (AMPDU), which holds a burst of frames for
// reordering. With the radio given to Bluetooth first (ledpanel's ESP_COEX_PREFER_BT) frames wait
// longer, and a burst could take the 15-20 KB that Bluedroid's dual mode needs to keep running.
// This deck only sends short GETs and gets small answers, a UDP palette and the watch's requests,
// so 12 of each and no receive aggregation are plenty. The link wraps esp_wifi_init
// (-Wl,--wrap=esp_wifi_init in [env:ledpanel]); the black deck keeps the defaults (its buffers go
// to PSRAM).
#include <esp_wifi.h>

extern "C" esp_err_t __real_esp_wifi_init(const wifi_init_config_t* config);

extern "C" esp_err_t __wrap_esp_wifi_init(const wifi_init_config_t* config) {
    if (!config) return __real_esp_wifi_init(config);
    wifi_init_config_t cfg = *config;
    if (cfg.dynamic_rx_buf_num > 12) cfg.dynamic_rx_buf_num = 12;
    if (cfg.dynamic_tx_buf_num > 12) cfg.dynamic_tx_buf_num = 12;
    cfg.ampdu_rx_enable = 0;
    return __real_esp_wifi_init(&cfg);
}
