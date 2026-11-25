#pragma once

#include <string_view>

namespace nc {

// How far a hardware module has been implemented. A module's status is only
// raised after its tests show the behaviour working.
enum class ModuleStatus {
    NotImplemented, // No emulation code exists yet.
    Partial,        // Some behaviour is emulated; gaps are listed in `notes`.
    Implemented,    // Behaviour needed by software is emulated and tested.
};

const char* module_status_name(ModuleStatus status);

struct ModuleInfo {
    std::string_view id;       // Short identifier, e.g. "cpu".
    std::string_view hardware; // The real hardware this module will emulate.
    ModuleStatus status = ModuleStatus::NotImplemented;
    std::string_view notes;    // What exists, what is missing, what comes next.
};

} // namespace nc
