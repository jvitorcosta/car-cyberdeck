#include "http_config.h"
#include <mbedtls/x509.h>
#include "ca_bundle.inc"

namespace netcfg {
void spotify(WiFiClientSecure& c) {
    c.setCACertBundle(CA_BUNDLE);
    c.setHandshakeTimeout(TLS_HANDSHAKE_S);
}

bool certRejected(WiFiClientSecure& c, const char* host) {
    const int e = c.lastError(nullptr, 0);
    // The bundle checker returns FATAL_ERROR for an unknown root; mbedTLS reports other
    // chain failures (wrong host name, bad signature) as CERT_VERIFY_FAILED.
    if (e != MBEDTLS_ERR_X509_CERT_VERIFY_FAILED && e != MBEDTLS_ERR_X509_FATAL_ERROR) return false;
    Serial.printf("[tls] certificate rejected: %s (error -0x%04X)\n", host, -e);
    return true;
}
}
