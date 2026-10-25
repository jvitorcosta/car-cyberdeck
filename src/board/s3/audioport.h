#pragma once
// The black deck's one I2S port (I2S_NUM_0) and the ES8311 behind it, shared by the speaker
// (sink_es8311.cpp) and the mic (mic_es8311.cpp). Master, TX and RX always (full duplex: TX
// clocks the codec, RX carries the mic), 16-bit stereo, MCLK x384, DMA 4 x 256 each way.
// Ref-counted and safe to call from any task; it doesn't arbitrate playback (greeting's s_output).
namespace audioport {
// The first user installs the port at rate and brings the codec up (DAC at 0 dB, the ADC as the
// codec's defaults leave it). Later users share it at the same rate; false if it runs at another
// rate, or the install or the codec failed (logged).
bool acquire(int rate);
void release();   // the last user uninstalls the port
}
