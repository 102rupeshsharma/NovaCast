#include "common/fileio.h"
#include "test_helpers.h"

#include <gtest/gtest.h>

#include <vector>

namespace nc {
namespace {

TEST(FileIo, RoundTrip) {
    test::TempDir dir;
    const auto path = dir / "data.bin";
    const std::vector<u8> bytes{0x00, 0x01, 0xFE, 0xFF, 0x80};
    ASSERT_TRUE(write_file(path, bytes).ok());

    auto read = read_file(path);
    ASSERT_TRUE(read.ok()) << read.error().to_string();
    EXPECT_EQ(read.value(), bytes);
}

TEST(FileIo, EmptyFile) {
    test::TempDir dir;
    const auto path = dir / "empty.bin";
    ASSERT_TRUE(write_file(path, {}).ok());
    auto read = read_file(path);
    ASSERT_TRUE(read.ok());
    EXPECT_TRUE(read.value().empty());
}

TEST(FileIo, MissingFile) {
    test::TempDir dir;
    auto read = read_file(dir / "nope.bin");
    ASSERT_FALSE(read.ok());
    EXPECT_EQ(read.error().code, ErrorCode::FileNotFound);
}

TEST(FileIo, DirectoryIsNotAFile) {
    test::TempDir dir;
    auto read = read_file(dir.path());
    ASSERT_FALSE(read.ok());
    EXPECT_EQ(read.error().code, ErrorCode::FileNotFound);
}

TEST(FileIo, EnforcesSizeLimit) {
    test::TempDir dir;
    const auto path = dir / "big.bin";
    ASSERT_TRUE(write_file(path, std::vector<u8>(100, 0xAA)).ok());

    auto read = read_file(path, 99);
    ASSERT_FALSE(read.ok());
    EXPECT_EQ(read.error().code, ErrorCode::FileTooLarge);
    EXPECT_TRUE(read_file(path, 100).ok());
}

TEST(FileIo, WriteFailsInMissingDirectory) {
    test::TempDir dir;
    auto r = write_file(dir / "no-such-dir" / "x.bin", std::vector<u8>{1});
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::IoError);
}

TEST(FileIo, NonAsciiPath) {
    test::TempDir dir;
    const auto path = dir.path() / std::filesystem::path(u8"ゲーム.bin");
    ASSERT_TRUE(write_file(path, std::vector<u8>{9}).ok());
    auto read = read_file(path);
    ASSERT_TRUE(read.ok()) << read.error().to_string();
    EXPECT_EQ(read.value().size(), 1u);
}

TEST(FileIo, LowerExtension) {
    EXPECT_EQ(lower_extension("Game.GDI"), ".gdi");
    EXPECT_EQ(lower_extension("dir.d/track01.Bin"), ".bin");
    EXPECT_EQ(lower_extension("noext"), "");
}

} // namespace
} // namespace nc
