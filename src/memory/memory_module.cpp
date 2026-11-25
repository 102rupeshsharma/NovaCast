#include "memory/memory_module.h"

namespace nc::memory {

ModuleInfo module_info() {
    return ModuleInfo{
        "memory",
        "16 MB main RAM, 8 MB VRAM, boot ROM, flash ROM and the system address map",
        ModuleStatus::Partial,
        "Address map with RAM, VRAM (32/64-bit paths), boot ROM and flash reads. Missing: flash "
        "programming, device register blocks (Holly, PVR, AICA, G1/G2).",
    };
}

} // namespace nc::memory
