#pragma once

#include "common/error.h"
#include "common/types.h"

#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace nc {

// A path rendered as UTF-8, for messages and logs.
std::string path_to_utf8(const std::filesystem::path& path);

// Reads a whole file. Fails with FileNotFound, FileTooLarge or IoError.
Result<std::vector<u8>> read_file(const std::filesystem::path& path,
                                  u64 max_size = 64ull * 1024 * 1024);

// Creates or truncates `path` and writes `data` to it.
Result<void> write_file(const std::filesystem::path& path, std::span<const u8> data);

// Lower-case extension including the dot, e.g. ".gdi". Empty if there is none.
std::string lower_extension(const std::filesystem::path& path);

} // namespace nc
