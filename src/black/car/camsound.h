#pragma once
#include "../../util/car/cameras.h"
#include "../../util/car/camlist.h"
// The camera alerts' beeps on the black deck's speaker. camsvc's stage changes (onStage, registered
// by main.cpp) go into a small queue; one task (3 KB stack, idle on the queue) asks util/camcue what
// to play, pauses a playing clip (greeting::pauseFor), writes the notes as sine tones on the clip's
// output or its own (greeting::beginTones), and hands the output back. While the stage stays Alarm it
// wakes for the 1.5 s repeat, checking the live stage and the mute switch each time.
namespace camsound {
void begin();   // the queue and the task; call once, before camsvc::begin()
// camsvc::onStage's callback: queues the change and returns (the GPS task's 4 KB stack, any task).
void onStage(cameras::Stage from, cameras::Stage to, camlist::Kind kind);
}  // namespace camsound
