#include "gpu/gpu_module.h"

namespace nc::gpu {

ModuleInfo module_info() {
    return ModuleInfo{
        "gpu",
        "NEC/VideoLogic PowerVR2 (CLX2) inside the Holly ASIC: tile accelerator, ISP/TSP, video output",
        ModuleStatus::NotImplemented,
        "No registers, display lists or rendering yet.",
    };
}

} // namespace nc::gpu
