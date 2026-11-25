#include "common/module_info.h"

namespace nc {

const char* module_status_name(ModuleStatus status) {
    switch (status) {
    case ModuleStatus::NotImplemented:
        return "not implemented";
    case ModuleStatus::Partial:
        return "partial";
    case ModuleStatus::Implemented:
        return "implemented";
    }
    return "?";
}

} // namespace nc
