#pragma once
#include "menunav.h"
// The companion's phone remote: a button name from POST /press?b=<name> -> the menu button it
// presses. PURE, host-tested. Spec: docs/superpowers/specs/2026-10-14-avatar-home-design.md
// (Revision: the phone drives her menu).
namespace phonepress {
// "lyrics" "sounds" "trip" "back" "howl" "vroom" "reset" (exact, lower case); anything else: None.
menunav::Button parse(const char* name);
}
