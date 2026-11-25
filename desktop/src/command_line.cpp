#include "command_line.h"

#include <charconv>

namespace nc::app {

namespace {

Result<std::string_view> take_value(std::span<const std::string_view> args, std::size_t& i) {
    if (i + 1 >= args.size())
        return make_error(ErrorCode::InvalidArgument, "option '" + std::string(args[i]) + "' needs a value");
    return args[++i];
}

Result<u64> take_positive(std::span<const std::string_view> args, std::size_t& i) {
    const std::string option(args[i]);
    auto value = take_value(args, i);
    if (!value)
        return value.error();
    const std::string_view text = value.value();
    u64 number = 0;
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), number);
    if (ec != std::errc{} || end != text.data() + text.size() || number == 0) {
        return make_error(ErrorCode::InvalidArgument,
                          option + " expects a positive integer, got '" + std::string(text) + "'");
    }
    return number;
}

} // namespace

Result<Options> parse_command_line(std::span<const std::string_view> args) {
    Options options;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg = args[i];

        if (arg == "-h" || arg == "--help") {
            options.show_help = true;
        } else if (arg == "--version") {
            options.show_version = true;
        } else if (arg == "--status") {
            options.show_status = true;
        } else if (arg == "--smoke-test") {
            options.smoke_test = true;
        } else if (arg == "--frames") {
            auto value = take_positive(args, i);
            if (!value)
                return value.error();
            options.max_frames = value.value();
        } else if (arg == "--run-cycles") {
            auto value = take_positive(args, i);
            if (!value)
                return value.error();
            options.run_cycles = value.value();
        } else if (arg == "--bios" || arg == "--flash") {
            auto value = take_value(args, i);
            if (!value)
                return value.error();
            (arg == "--bios" ? options.bios : options.flash) = std::filesystem::path(std::string(value.value()));
        } else if (arg == "--log-level") {
            auto value = take_value(args, i);
            if (!value)
                return value.error();
            const auto level = parse_log_level(value.value());
            if (!level) {
                return make_error(ErrorCode::InvalidArgument,
                                  "unknown log level '" + std::string(value.value()) +
                                      "' (use trace, debug, info, warn, error or off)");
            }
            options.log_level = *level;
        } else if (arg == "--log-file") {
            auto value = take_value(args, i);
            if (!value)
                return value.error();
            options.log_file = std::filesystem::path(std::string(value.value()));
        } else if (arg.size() > 1 && arg.front() == '-') {
            return make_error(ErrorCode::InvalidArgument, "unknown option '" + std::string(arg) + "'");
        } else {
            if (options.game)
                return make_error(ErrorCode::InvalidArgument, "only one game file can be given");
            options.game = std::filesystem::path(std::string(arg));
        }
    }
    if (options.run_cycles && !options.bios)
        return make_error(ErrorCode::InvalidArgument, "--run-cycles needs --bios");
    if (options.flash && !options.bios)
        return make_error(ErrorCode::InvalidArgument, "--flash needs --bios");
    return options;
}

std::string usage_text() {
    return "Usage: novacast [options] [game.gdi|game.cdi|game.cue]\n"
           "\n"
           "Options:\n"
           "  -h, --help           Show this help and exit\n"
           "      --version        Show the version and exit\n"
           "      --status         Show which hardware modules are implemented and exit\n"
           "      --frames N       Close the window after N frames\n"
           "      --log-level L    trace, debug, info, warn, error or off (default: info)\n"
           "      --log-file PATH  Also write the log to PATH\n"
           "      --smoke-test     Open a hidden window, draw a few frames and exit\n"
           "      --bios PATH      Dreamcast boot ROM (2 MB) dumped from your own console\n"
           "      --flash PATH     Dreamcast flash ROM (128 KB), optional\n"
           "      --run-cycles N   Run the boot ROM headless for N SH-4 cycles and print the CPU state\n"
           "\n"
           "Only the SH-4 CPU and the memory map are emulated so far. There is no video,\n"
           "audio, GD-ROM or controller emulation yet, so games cannot be run.\n";
}

} // namespace nc::app
