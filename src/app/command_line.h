#pragma once

#include "common/error.h"
#include "common/log.h"
#include "common/types.h"

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace nc::app {

struct Options {
    bool show_help = false;
    bool show_version = false;
    bool show_status = false;  // Print module status and exit.
    bool smoke_test = false;   // Hidden window, a few frames, then exit (used by tests).
    std::optional<u64> max_frames;
    LogLevel log_level = LogLevel::Info;
    std::optional<std::filesystem::path> log_file;
    std::optional<std::filesystem::path> game;
    std::optional<std::filesystem::path> bios;
    std::optional<std::filesystem::path> flash;
    std::optional<u64> run_cycles; // Run the BIOS headless for N cycles and report
};

// `args` excludes the program name.
Result<Options> parse_command_line(std::span<const std::string_view> args);

std::string usage_text();

} // namespace nc::app
