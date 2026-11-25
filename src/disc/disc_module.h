#pragma once

// Disc module.
//
// Will contain: disc image readers (GDI first, then CDI and CUE/BIN, each
// validated and rejected cleanly when malformed), the TOC model, and the
// GD-ROM drive (ATA/packet command interface, PIO and DMA transfers).
//
// Current state: no disc image parsing exists yet. See docs/PROGRESS.md.

#include "common/module_info.h"

namespace nc::disc {

ModuleInfo module_info();

} // namespace nc::disc
