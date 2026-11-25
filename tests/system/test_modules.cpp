#include "audio/audio_module.h"
#include "controller/controller_module.h"
#include "cpu/cpu_module.h"
#include "disc/disc_module.h"
#include "gpu/gpu_module.h"
#include "memory/memory_module.h"

#include <gtest/gtest.h>

#include <map>
#include <set>
#include <string>
#include <vector>

namespace nc {
namespace {

std::vector<ModuleInfo> all_modules() {
    return {cpu::module_info(),    gpu::module_info(),  audio::module_info(),
            memory::module_info(), disc::module_info(), controller::module_info()};
}

TEST(Modules, DescribeThemselves) {
    std::set<std::string> ids;
    for (const ModuleInfo& m : all_modules()) {
        EXPECT_FALSE(m.id.empty());
        EXPECT_FALSE(m.hardware.empty()) << m.id;
        EXPECT_FALSE(m.notes.empty()) << m.id;
        ids.insert(std::string(m.id));
    }
    EXPECT_EQ(ids, (std::set<std::string>{"cpu", "gpu", "audio", "memory", "disc", "controller"}));
}

// Guards against overstating progress. Each module's status is pinned here;
// when a module gains real, tested emulation, update its status and this
// test together.
TEST(Modules, StatusMatchesImplementation) {
    const std::map<std::string, ModuleStatus> expected = {
        {"cpu", ModuleStatus::Partial},         // interpreter, no MMU/peripherals
        {"memory", ModuleStatus::Partial},      // address map, no flash writes/devices
        {"gpu", ModuleStatus::NotImplemented},
        {"audio", ModuleStatus::NotImplemented},
        {"disc", ModuleStatus::NotImplemented},
        {"controller", ModuleStatus::NotImplemented},
    };
    for (const ModuleInfo& m : all_modules())
        EXPECT_EQ(m.status, expected.at(std::string(m.id))) << m.id;
}

TEST(Modules, NoModuleClaimsToBeComplete) {
    for (const ModuleInfo& m : all_modules())
        EXPECT_NE(m.status, ModuleStatus::Implemented) << m.id;
}

TEST(Modules, StatusNames) {
    EXPECT_STREQ(module_status_name(ModuleStatus::NotImplemented), "not implemented");
    EXPECT_STREQ(module_status_name(ModuleStatus::Partial), "partial");
    EXPECT_STREQ(module_status_name(ModuleStatus::Implemented), "implemented");
}

} // namespace
} // namespace nc
