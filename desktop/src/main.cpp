// NovaCast entry point.

#include "command_line.h"
#include "sdl_window.h"
#include "common/log.h"
#include "common/module_info.h"
#include "system/emulator.h"
#include "system/scheduler.h"
#include "cpu/sh4.h"
#include "memory/bus.h"

#include <SDL.h>

#include <chrono>
#include <cstdio>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

void print_status(const nc::Emulator& emulator) {
    std::printf("NovaCast %s - hardware module status\n\n", NOVACAST_VERSION);
    for (const nc::ModuleInfo& m : emulator.modules()) {
        std::printf("  %-11s %-16s %.*s\n", std::string(m.id).c_str(), nc::module_status_name(m.status),
                    static_cast<int>(m.hardware.size()), m.hardware.data());
        std::printf("  %-11s %-16s %.*s\n", "", "", static_cast<int>(m.notes.size()), m.notes.data());
    }
    std::printf("\nCan run Dreamcast software: %s\n", emulator.can_run_software() ? "yes" : "no");
}

// Runs the loaded boot ROM without video and prints where the CPU ended up.
int run_headless(nc::Emulator& emulator, nc::u64 cycles) {
    const auto start = std::chrono::steady_clock::now();
    if (auto r = emulator.run_for(cycles); !r) {
        LOG_ERROR("app", "%s", r.error().to_string().c_str());
        return 1;
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    const nc::cpu::Sh4& cpu = emulator.cpu();
    const auto& s = cpu.state();
    std::printf("Ran %llu SH-4 cycles (%.3f emulated seconds) in %.3f s host time\n",
                static_cast<unsigned long long>(emulator.scheduler().now()),
                static_cast<double>(emulator.scheduler().now()) / nc::Scheduler::kCyclesPerSecond, seconds);
    if (seconds > 0)
        std::printf("Interpreter speed: %.1f MIPS\n", static_cast<double>(cpu.instructions_executed()) / seconds / 1e6);
    std::printf("PC=%08X PR=%08X SR=%08X VBR=%08X sleeping=%s\n", s.pc, s.pr, s.sr, s.vbr,
                cpu.sleeping() ? "yes" : "no");
    for (int i = 0; i < 16; i += 4)
        std::printf("R%-2d=%08X R%-2d=%08X R%-2d=%08X R%-2d=%08X\n", i, s.r[i], i + 1, s.r[i + 1], i + 2,
                    s.r[i + 2], i + 3, s.r[i + 3]);
    std::printf("Accesses to unmapped addresses: %llu\n",
                static_cast<unsigned long long>(emulator.bus().unmapped_access_count()));
    std::printf("Note: Holly, PVR, AICA, GD-ROM and the SH-4 peripherals are not emulated yet,\n"
                "so a real boot ROM is expected to stop making progress early.\n");
    return 0;
}

int run(int argc, char** argv) {
    std::vector<std::string_view> args(argv + 1, argv + argc);
    auto parsed = nc::app::parse_command_line(args);
    if (!parsed) {
        std::fprintf(stderr, "novacast: %s\n\n%s", parsed.error().message.c_str(), nc::app::usage_text().c_str());
        return 2;
    }
    const nc::app::Options& options = parsed.value();

    if (options.show_help) {
        std::printf("%s", nc::app::usage_text().c_str());
        return 0;
    }
    if (options.show_version) {
        std::printf("NovaCast %s\n", NOVACAST_VERSION);
        return 0;
    }

    nc::log_set_level(options.log_level);
    if (options.log_file) {
        if (auto r = nc::log_open_file(*options.log_file); !r) {
            std::fprintf(stderr, "novacast: %s\n", r.error().to_string().c_str());
            return 1;
        }
    }

    auto created = nc::Emulator::create();
    if (!created) {
        LOG_ERROR("app", "%s", created.error().to_string().c_str());
        return 1;
    }
    std::unique_ptr<nc::Emulator> emulator = std::move(created).value();

    if (options.show_status) {
        print_status(*emulator);
        return 0;
    }

    LOG_INFO("app", "NovaCast %s starting", NOVACAST_VERSION);
    for (const nc::ModuleInfo& m : emulator->modules()) {
        if (m.status != nc::ModuleStatus::Implemented)
            LOG_WARN("app", "module '%.*s' is %s", static_cast<int>(m.id.size()), m.id.data(),
                     nc::module_status_name(m.status));
    }

    if (options.bios) {
        if (auto r = emulator->load_bios(*options.bios); !r) {
            LOG_ERROR("app", "cannot load boot ROM: %s", r.error().to_string().c_str());
            return 1;
        }
        if (options.flash) {
            if (auto r = emulator->load_flash(*options.flash); !r) {
                LOG_ERROR("app", "cannot load flash ROM: %s", r.error().to_string().c_str());
                return 1;
            }
        }
        emulator->reset();
        if (options.run_cycles)
            return run_headless(*emulator, *options.run_cycles);
        LOG_WARN("app", "boot ROM loaded but not run: there is no video output yet (use --run-cycles)");
    }

    if (options.game) {
        if (auto r = emulator->load_game(*options.game); !r) {
            LOG_ERROR("app", "cannot start game: %s", r.error().to_string().c_str());
            return 1;
        }
    }

    nc::app::WindowOptions window;
    window.title = std::string("NovaCast ") + NOVACAST_VERSION + " - no emulation core yet";
    window.hidden = options.smoke_test;
    window.vsync = !options.smoke_test;
    window.max_frames = options.smoke_test ? std::optional<nc::u64>(3) : options.max_frames;

    auto result = nc::app::run_window(window);
    if (!result) {
        LOG_ERROR("app", "%s", result.error().to_string().c_str());
        return 1;
    }
    LOG_INFO("app", "shut down cleanly after %llu frames", static_cast<unsigned long long>(result.value().frames));
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    SDL_SetMainReady();
    try {
        const int code = run(argc, argv);
        nc::log_close_file();
        return code;
    } catch (const std::exception& e) {
        LOG_ERROR("app", "fatal: unhandled exception: %s", e.what());
    } catch (...) {
        LOG_ERROR("app", "fatal: unhandled unknown exception");
    }
    nc::log_close_file();
    return 1;
}
