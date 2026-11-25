#pragma once

// AICA audio module.
//
// Will contain: the AICA register file, 2 MB wave RAM, the ARM7DI sound CPU,
// the 64-channel PCM/ADPCM sample engine with envelopes and LFOs, the effects
// DSP, and the real-time clock.
//
// Current state: no audio emulation exists yet. See docs/PROGRESS.md.

#include "common/module_info.h"

namespace nc::audio {

ModuleInfo module_info();

} // namespace nc::audio
