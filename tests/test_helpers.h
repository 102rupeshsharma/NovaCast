#pragma once

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <system_error>

namespace nc::test {

// A fresh directory under the system temp folder, removed on destruction.
class TempDir {
public:
    TempDir() {
        static std::atomic<unsigned> counter{0};
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        std::string name = "novacast_test_";
        if (info)
            name += std::string(info->test_suite_name()) + "_" + info->name() + "_";
        name += std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "_" +
                std::to_string(counter++);
        path_ = std::filesystem::temp_directory_path() / name;
        std::filesystem::create_directories(path_);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::filesystem::path& path() const { return path_; }
    std::filesystem::path operator/(const std::string& name) const { return path_ / name; }

private:
    std::filesystem::path path_;
};

} // namespace nc::test
