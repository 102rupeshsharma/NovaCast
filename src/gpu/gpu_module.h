#pragma once

// PowerVR2 GPU module.
//
// Will contain: the PVR register file and video sync generator, the tile
// accelerator (display-list parsing), texture decoding, and an ISP/TSP model
// that feeds a host renderer (OpenGL first, Vulkan later) through a backend
// interface.
//
// Current state: no GPU emulation exists yet. See docs/PROGRESS.md.

#include "common/module_info.h"

namespace nc::gpu {

ModuleInfo module_info();

} // namespace nc::gpu
