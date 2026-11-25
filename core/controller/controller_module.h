#pragma once

// Controller module (Maple bus).
//
// Will contain: the Maple bus protocol and DMA, the standard controller
// (buttons, analog stick, triggers), and the VMU (storage and LCD). This
// module receives abstract button/axis state; it never talks to SDL or any
// other host input API directly.
//
// Current state: no Maple bus emulation exists yet. See docs/PROGRESS.md.

#include "common/module_info.h"

namespace nc::controller {

ModuleInfo module_info();

} // namespace nc::controller
