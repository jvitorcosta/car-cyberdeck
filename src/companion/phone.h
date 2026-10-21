#pragma once
#include "../util/driveview.h"
#include "../util/menunav.h"
// The old deck's own remote page (its touch layer is faulty): GET / serves the page, POST
// /press?b=<name>&k=<REMOTE_TOKEN> queues a button press, POST /view?v=auto|drive|lyrics pins the
// drive view, GET /status says what's on (the page refreshes it every 2 s). Off without
// REMOTE_TOKEN. Specs: docs/superpowers/specs/2026-10-14-avatar-home-design.md (Revision: the
// phone), 2026-10-21-companion-drive-design.md (the drive view and status).
namespace phone {
void begin();                          // web server on its own task, once WiFi is up
menunav::Button take();                // the latest queued press, once; None when there is none
bool takePin(driveview::Pin* out);     // the latest view picked on the phone, once
void setStatus(const char* json);      // what GET /status answers (UI loop; util/carstatus)
}
