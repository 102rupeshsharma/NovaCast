#include "common/log.h"

#include "common/fileio.h"

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <mutex>

namespace nc {

namespace {

struct LogState {
    std::mutex mutex;
    LogSink sink = nullptr;
    void* sink_user = nullptr;
    std::ofstream file;
    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
};

LogState& state() {
    static LogState instance;
    return instance;
}

std::atomic<int> g_level{static_cast<int>(LogLevel::Info)};

} // namespace

const char* log_level_name(LogLevel level) {
    switch (level) {
    case LogLevel::Trace:
        return "trace";
    case LogLevel::Debug:
        return "debug";
    case LogLevel::Info:
        return "info";
    case LogLevel::Warn:
        return "warn";
    case LogLevel::Error:
        return "error";
    case LogLevel::Off:
        return "off";
    }
    return "?";
}

std::optional<LogLevel> parse_log_level(std::string_view text) {
    for (int i = 0; i <= static_cast<int>(LogLevel::Off); ++i) {
        const auto level = static_cast<LogLevel>(i);
        if (text == log_level_name(level))
            return level;
    }
    return std::nullopt;
}

void log_set_level(LogLevel level) {
    g_level.store(static_cast<int>(level), std::memory_order_relaxed);
}

LogLevel log_level() {
    return static_cast<LogLevel>(g_level.load(std::memory_order_relaxed));
}

void log_set_sink(LogSink sink, void* user) {
    LogState& s = state();
    std::lock_guard lock(s.mutex);
    s.sink = sink;
    s.sink_user = user;
}

Result<void> log_open_file(const std::filesystem::path& path) {
    LogState& s = state();
    std::lock_guard lock(s.mutex);
    if (s.file.is_open())
        s.file.close();
    s.file.clear();
    s.file.open(path, std::ios::out | std::ios::trunc);
    if (!s.file.is_open())
        return make_error(ErrorCode::IoError, "cannot create log file '" + path_to_utf8(path) + "'");
    return {};
}

void log_close_file() {
    LogState& s = state();
    std::lock_guard lock(s.mutex);
    if (s.file.is_open())
        s.file.close();
}

void log_write(LogLevel level, const char* tag, const char* fmt, ...) {
    if (static_cast<int>(level) < static_cast<int>(log_level()) || level == LogLevel::Off)
        return;

    char message[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    LogState& s = state();
    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - s.start).count();

    char line[1200];
    std::snprintf(line, sizeof(line), "[%9.3f] [%-5s] %s: %s", seconds, log_level_name(level), tag,
                  message);

    std::lock_guard lock(s.mutex);
    if (s.sink)
        s.sink(level, tag, message, s.sink_user);
    else
        std::fprintf(stderr, "%s\n", line);

    if (s.file.is_open())
        s.file << line << '\n' << std::flush;
}

} // namespace nc
