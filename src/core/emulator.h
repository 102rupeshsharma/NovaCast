#pragma once

#include "common/error.h"
#include "common/module_info.h"
#include "common/types.h"

#include <array>
#include <filesystem>
#include <memory>
#include <span>

namespace nc {

namespace cpu {
class Sh4;
}
namespace memory {
class Bus;
}
class Scheduler;

// The emulated Dreamcast as a whole: owns the hardware modules, wires them
// together and drives them from one clock (the Scheduler).
//
// Instances are created with create(), are neither copyable nor movable, and
// release everything they own in their destructor.
class Emulator {
public:
    static constexpr std::size_t kModuleCount = 6;

    static Result<std::unique_ptr<Emulator>> create();
    ~Emulator();

    Emulator(const Emulator&) = delete;
    Emulator& operator=(const Emulator&) = delete;
    Emulator(Emulator&&) = delete;
    Emulator& operator=(Emulator&&) = delete;

    std::span<const ModuleInfo> modules() const { return modules_; }

    // True only once every module needed to run software is implemented.
    bool can_run_software() const;

    // Loads a 2 MB boot ROM image. Rejects missing files, wrong sizes and
    // blank (all 0x00 or all 0xFF) images.
    Result<void> load_bios(const std::filesystem::path& path);
    Result<void> load_bios(std::span<const u8> image);
    // Loads a 128 KB flash image (optional; an erased chip is used otherwise).
    Result<void> load_flash(const std::filesystem::path& path);

    // Power-on reset: clears RAM/VRAM, resets the CPU (PC = 0xA0000000, the
    // start of the boot ROM) and rewinds the clock.
    void reset();

    // Runs the system for `cycles` SH-4 cycles, firing scheduled events.
    // CPU slices stop at the next event deadline, but can overrun it by one
    // cycle when a delayed branch and its slot straddle it; events receive
    // that overrun as `cycles_late`. Fails with InvalidArgument if no boot
    // ROM has been loaded.
    Result<void> run_for(u64 cycles);

    // Validates `path` as a disc image. Disc loading is not implemented yet,
    // so a valid-looking file returns NotImplemented.
    Result<void> load_game(const std::filesystem::path& path);

    cpu::Sh4& cpu() { return *cpu_; }
    memory::Bus& bus() { return *bus_; }
    Scheduler& scheduler() { return *scheduler_; }

private:
    class CpuBus;

    Emulator();

    std::array<ModuleInfo, kModuleCount> modules_;
    std::unique_ptr<Scheduler> scheduler_;
    std::unique_ptr<memory::Bus> bus_;
    std::unique_ptr<CpuBus> cpu_bus_;
    std::unique_ptr<cpu::Sh4> cpu_;
    u64 target_time_ = 0; // Absolute cycle count run_for() is working towards
};

} // namespace nc
