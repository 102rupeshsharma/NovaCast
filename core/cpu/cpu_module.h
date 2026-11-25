#pragma once

// SH-4 CPU module.
//
// Will contain: the SH-4 interpreter (integer, FPU, privileged instructions),
// exception and interrupt handling, the on-chip peripherals (TMU, INTC, DMAC,
// CCN, SCIF, store queues), and later the dynamic recompiler.
//
// Current state: no CPU emulation exists yet. See docs/PROGRESS.md.

#include "common/module_info.h"

namespace nc::cpu {

ModuleInfo module_info();

} // namespace nc::cpu
