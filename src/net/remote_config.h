#pragma once
#include "../config.h"
// Phone-remote and two-deck settings, with the defaults an older config.h leaves out. Shared by
// the black deck (net/remote, net/wifi) and the yellow deck (ledpanel).

#ifndef REMOTE_TOKEN
#define REMOTE_TOKEN ""   // older config.h: remote off
#endif

// The password for firmware updates over WiFi (net/ota), apart from REMOTE_TOKEN: that one travels
// in plain-HTTP URLs, this one never leaves the PC but for espota's challenge (tools/ota_auth.py).
#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""   // older config.h: updates over WiFi off
#endif

// The black deck's name on the local network (http://cardeck.local/, mDNS / Bonjour); the yellow
// deck looks it up off the hotspot. REMOTE_NAME renames the deck.
#define DECK_MDNS_NAME "cardeck"
#ifndef REMOTE_NAME
#define REMOTE_NAME DECK_MDNS_NAME
#endif

// Fixed last octet on the iPhone hotspot (util/hotspotip): 172.20.10.10 for the black deck (S3),
// .11 for the yellow deck (-D YELLOW_DECK), so Home Screen bookmarks keep working. The yellow deck
// finds the black deck at BLACK_DECK_ADDR.
#define DECK_HOTSPOT_OCTET 10
#define YELLOW_HOTSPOT_OCTET 11
#ifndef BLACK_DECK_ADDR
#ifdef COMPANION_DECK
#define BLACK_DECK_ADDR COMPANION_DECK   // its name in a config.h from before 2026-10-28
#else
#define BLACK_DECK_ADDR "172.20.10.10"   // the S3's fixed address on the iPhone hotspot
#endif
#endif
