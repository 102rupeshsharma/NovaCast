#include "controller/controller_module.h"

namespace nc::controller {

ModuleInfo module_info() {
    return ModuleInfo{
        "controller",
        "Maple bus: standard controller (buttons, analog stick, triggers) and VMU memory card",
        ModuleStatus::NotImplemented,
        "No Maple bus protocol or devices yet. Host gamepads are only detected by the app.",
    };
}

} // namespace nc::controller
