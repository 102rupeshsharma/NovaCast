#include "core/emulator.h"

#include "audio/audio_module.h"
#include "common/fileio.h"
#include "common/log.h"
#include "controller/controller_module.h"
#include "core/scheduler.h"
#include "cpu/cpu_module.h"
#include "cpu/sh4.h"
#include "disc/disc_module.h"
#include "gpu/gpu_module.h"
#include "memory/bus.h"
#include "memory/memory_module.h"

#include <algorithm>
#include <system_error>

namespace nc {

// Connects the CPU's memory interface to the system bus. The CPU and memory
// modules do not know about each other; only the core joins them.
class Emulator::CpuBus final : public cpu::Sh4Memory {
public:
    explicit CpuBus(memory::Bus& bus) : bus_(bus) {}
    u8 read8(u32 address) override { return bus_.read8(address); }
    u16 read16(u32 address) override { return bus_.read16(address); }
    u32 read32(u32 address) override { return bus_.read32(address); }
    void write8(u32 address, u8 value) override { bus_.write8(address, value); }
    void write16(u32 address, u16 value) override { bus_.write16(address, value); }
    void write32(u32 address, u32 value) override { bus_.write32(address, value); }

private:
    memory::Bus& bus_;
};

Result<std::unique_ptr<Emulator>> Emulator::create() {
    // The constructor is private, so make_unique cannot be used here.
    std::unique_ptr<Emulator> emulator(new Emulator());
    LOG_DEBUG("core", "emulator created");
    return emulator;
}

Emulator::Emulator()
    : modules_{cpu::module_info(),    gpu::module_info(),  audio::module_info(),
               memory::module_info(), disc::module_info(), controller::module_info()},
      scheduler_(std::make_unique<Scheduler>()),
      bus_(std::make_unique<memory::Bus>()),
      cpu_bus_(std::make_unique<CpuBus>(*bus_)),
      cpu_(std::make_unique<cpu::Sh4>(*cpu_bus_)) {}

Emulator::~Emulator() {
    LOG_DEBUG("core", "emulator destroyed");
}

bool Emulator::can_run_software() const {
    return std::all_of(modules_.begin(), modules_.end(),
                       [](const ModuleInfo& m) { return m.status == ModuleStatus::Implemented; });
}

Result<void> Emulator::load_bios(std::span<const u8> image) {
    if (image.size() == memory::kBiosSize) {
        const bool all_zero = std::all_of(image.begin(), image.end(), [](u8 b) { return b == 0x00; });
        const bool all_ones = std::all_of(image.begin(), image.end(), [](u8 b) { return b == 0xFF; });
        if (all_zero || all_ones)
            return make_error(ErrorCode::InvalidFormat, "boot ROM image is blank (every byte is the same)");
    }
    return bus_->load_bios(image);
}

Result<void> Emulator::load_bios(const std::filesystem::path& path) {
    auto data = read_file(path, memory::kBiosSize + 1);
    if (!data) {
        if (data.error().code == ErrorCode::FileTooLarge)
            return make_error(ErrorCode::InvalidFormat, "'" + path_to_utf8(path) + "' is larger than a 2 MB boot ROM");
        return data.error();
    }
    if (auto r = load_bios(data.value()); !r)
        return make_error(r.error().code, "'" + path_to_utf8(path) + "': " + r.error().message);
    LOG_INFO("core", "boot ROM loaded from '%s'", path_to_utf8(path).c_str());
    return {};
}

Result<void> Emulator::load_flash(const std::filesystem::path& path) {
    auto data = read_file(path, memory::kFlashSize + 1);
    if (!data) {
        if (data.error().code == ErrorCode::FileTooLarge)
            return make_error(ErrorCode::InvalidFormat, "'" + path_to_utf8(path) + "' is larger than a 128 KB flash ROM");
        return data.error();
    }
    if (auto r = bus_->load_flash(data.value()); !r)
        return make_error(r.error().code, "'" + path_to_utf8(path) + "': " + r.error().message);
    LOG_INFO("core", "flash ROM loaded from '%s'", path_to_utf8(path).c_str());
    return {};
}

void Emulator::reset() {
    target_time_ = 0;
    scheduler_->reset();
    bus_->reset();
    cpu_->reset();
}

Result<void> Emulator::run_for(u64 cycles) {
    if (!bus_->bios_loaded())
        return make_error(ErrorCode::InvalidArgument, "no boot ROM loaded");

    // The target is absolute, so an overrun in one call is absorbed by the
    // next instead of accumulating.
    target_time_ += cycles;
    const u64 end = target_time_;
    while (scheduler_->now() < end) {
        const u64 next = std::min(end, scheduler_->next_deadline());
        const u64 slice = next > scheduler_->now() ? next - scheduler_->now() : 0;
        if (slice != 0)
            scheduler_->advance(cpu_->run(slice));
        scheduler_->run_due();
    }
    return {};
}

Result<void> Emulator::load_game(const std::filesystem::path& path) {
    const std::string name = path_to_utf8(path);

    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec))
        return make_error(ErrorCode::FileNotFound, "'" + name + "' does not exist or is not a file");

    const std::string ext = lower_extension(path);
    if (ext != ".gdi" && ext != ".cdi" && ext != ".cue") {
        return make_error(ErrorCode::Unsupported,
                          "'" + name + "' is not a supported disc image (expected .gdi, .cdi or .cue)");
    }

    return make_error(ErrorCode::NotImplemented,
                      "disc image loading is not implemented yet; '" + name + "' was not loaded");
}

} // namespace nc
