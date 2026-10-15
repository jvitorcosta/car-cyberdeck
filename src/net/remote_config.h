#pragma once
#include "../config.h"
// Phone-remote and two-deck settings, with the defaults an older config.h leaves out. Shared by
// the new deck (net/remote, net/wifi) and the companion (companion/link, companion/phone).

#ifndef REMOTE_TOKEN
#define REMOTE_TOKEN ""   // older config.h: remote off
#endif

// The new deck's name on the local network (http://cardeck.local/); the companion looks it up
// off the hotspot. Each deck's own name: http://<REMOTE_NAME>.local/ (mDNS / Bonjour), different
// per build so all of them can be on at once.
#define DECK_MDNS_NAME "cardeck"
#define COMPANION_MDNS_NAME "cardeck-companion"
#ifndef REMOTE_NAME
#ifdef BOARD_S3
#define REMOTE_NAME DECK_MDNS_NAME
#else
#define REMOTE_NAME "cardeck-cyd"
#endif
#endif

// Fixed last octet on the iPhone hotspot (util/hotspotip): 172.20.10.10 for the S3, .11 for the
// CYD, so Home Screen bookmarks keep working. The companion finds the S3 at COMPANION_DECK.
#define DECK_HOTSPOT_OCTET 10
#define CYD_HOTSPOT_OCTET 11
#ifndef COMPANION_DECK
#define COMPANION_DECK "172.20.10.10"   // the S3's fixed address on the iPhone hotspot
#endif
