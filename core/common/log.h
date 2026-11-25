#pragma once

#include "common/error.h"

#include <filesystem>
#include <optional>
#include <string_view>

namespace nc {

enum class LogLevel : int { Trace = 0, Debug, Info, Warn, Error, Off };

const char* log_level_name(LogLevel level);
std::optional<LogLevel> parse_log_level(std::string_view text);

// Called with every message that passes the level filter. Replaces the
// default stderr output while installed. Must be thread safe if logging is
// used from more than one thread.
using LogSink = void (*)(LogLevel level, const char* tag, const char* message, void* user);

void log_set_level(LogLevel level);
LogLevel log_level();

// Pass nullptr to restore the default stderr output.
void log_set_sink(LogSink sink, void* user);

// Additionally writes every message to a file. Replaces any previous log file.
Result<void> log_open_file(const std::filesystem::path& path);
void log_close_file();

// printf-style. Messages longer than 1 KiB are truncated. Thread safe.
void log_write(LogLevel level, const char* tag, const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;

} // namespace nc

// The level check happens before the arguments are evaluated.
#define NC_LOG(lvl, tag, ...)                                                      \
    do {                                                                           \
        if (static_cast<int>(lvl) >= static_cast<int>(::nc::log_level()))          \
            ::nc::log_write(lvl, tag, __VA_ARGS__);                                \
    } while (0)

#define LOG_TRACE(tag, ...) NC_LOG(::nc::LogLevel::Trace, tag, __VA_ARGS__)
#define LOG_DEBUG(tag, ...) NC_LOG(::nc::LogLevel::Debug, tag, __VA_ARGS__)
#define LOG_INFO(tag, ...) NC_LOG(::nc::LogLevel::Info, tag, __VA_ARGS__)
#define LOG_WARN(tag, ...) NC_LOG(::nc::LogLevel::Warn, tag, __VA_ARGS__)
#define LOG_ERROR(tag, ...) NC_LOG(::nc::LogLevel::Error, tag, __VA_ARGS__)
