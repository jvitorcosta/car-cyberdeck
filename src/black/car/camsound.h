#pragma once
#include "../../util/car/camseq.h"
// The camera alerts' sound on the black deck's speaker: the chime and voice lines (data/camvoice.bin
// in the media pack, read where it is mapped, never copied) and the beeps. camsvc's events (onStage,
// registered by main.cpp) go into a small queue; one task (3 KB stack, idle on the queue) asks
// util/camseq what to play, pauses a playing clip (greeting::pauseFor), writes the steps on the
// clip's output or its own (greeting::beginTones; a voice clip resampled from 16 kHz when that output
// runs at another rate) and hands the output back. Between two 128-sample writes it takes in new
// events and stops for a higher step (the alarm over an alert line, an alert over a GPS line), for
// mute (every voice line; the alarm's beeps play on) and for the alarm ending (the live stage is read
// every few writes too). While the stage stays Alarm it wakes for each 1.5 s slot, checking the live
// stage each time, and keeps its own output open between them until the alarm ends or a clip wants
// the output.
namespace camsound {
void begin();   // the queue and the task; call once, before camsvc::begin()
// camsvc::onStage's callback: queues the event and returns (the GPS task's 4 KB stack, any task).
void onStage(const camseq::Event& e);
// A mark's answer (net/remote's /cam/mark, any task): the chirp when saved, the low beep when not.
void onMark(bool saved);
}  // namespace camsound
