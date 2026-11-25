#include "disc/disc_module.h"

namespace nc::disc {

ModuleInfo module_info() {
    return ModuleInfo{
        "disc",
        "GD-ROM drive (ATA packet interface) and disc images: GDI, CDI, CUE/BIN",
        ModuleStatus::NotImplemented,
        "No image parsing or drive emulation yet. Game files cannot be loaded.",
    };
}

} // namespace nc::disc
