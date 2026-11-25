#include "common/fileio.h"

#include <cctype>
#include <fstream>
#include <system_error>

namespace nc {

std::string path_to_utf8(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
}

Result<std::vector<u8>> read_file(const std::filesystem::path& path, u64 max_size) {
    const std::string name = path_to_utf8(path);

    std::error_code ec;
    const bool is_file = std::filesystem::is_regular_file(path, ec);
    if (ec || !is_file)
        return make_error(ErrorCode::FileNotFound, "'" + name + "' does not exist or is not a file");

    const u64 size = std::filesystem::file_size(path, ec);
    if (ec)
        return make_error(ErrorCode::IoError, "cannot get size of '" + name + "': " + ec.message());
    if (size > max_size) {
        return make_error(ErrorCode::FileTooLarge, "'" + name + "' is " + std::to_string(size) +
                                                       " bytes; the limit is " + std::to_string(max_size));
    }

    std::ifstream file(path, std::ios::binary);
    if (!file)
        return make_error(ErrorCode::IoError, "cannot open '" + name + "'");

    std::vector<u8> data(static_cast<std::size_t>(size));
    if (size != 0) {
        file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
        if (static_cast<u64>(file.gcount()) != size)
            return make_error(ErrorCode::IoError, "short read on '" + name + "'");
    }
    return data;
}

Result<void> write_file(const std::filesystem::path& path, std::span<const u8> data) {
    const std::string name = path_to_utf8(path);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
        return make_error(ErrorCode::IoError, "cannot create '" + name + "'");
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    file.flush();
    if (!file)
        return make_error(ErrorCode::IoError, "write failed on '" + name + "'");
    return {};
}

std::string lower_extension(const std::filesystem::path& path) {
    std::string ext = path_to_utf8(path.extension());
    for (char& c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

} // namespace nc
