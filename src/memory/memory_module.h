#pragma once

// Memory module.
//
// Will contain: main RAM (16 MB), VRAM (8 MB, 32- and 64-bit access paths),
// boot ROM and flash ROM, and the physical address decoder that routes CPU
// and DMA accesses to RAM or to device registers.
//
// Current state: no memory map exists yet. See docs/PROGRESS.md.

#include "common/module_info.h"

namespace nc::memory {

ModuleInfo module_info();

} // namespace nc::memory
