#pragma once
#include "../util/menunav.h"
// The old deck's own remote page for her menu (its touch layer is faulty): GET / serves the
// page, POST /press?b=<name>&k=<REMOTE_TOKEN> queues a button press. Off without REMOTE_TOKEN.
// Spec: docs/superpowers/specs/2026-10-14-avatar-home-design.md (Revision: the phone).
namespace phone {
void begin();                  // web server on its own task, once WiFi is up
menunav::Button take();        // the latest queued press, once; None when there is none
}
