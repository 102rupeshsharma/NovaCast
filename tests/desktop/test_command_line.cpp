#include "command_line.h"

#include <gtest/gtest.h>

#include <initializer_list>
#include <string_view>
#include <vector>

namespace nc::app {
namespace {

Result<Options> parse(std::initializer_list<std::string_view> args) {
    const std::vector<std::string_view> list(args);
    return parse_command_line(list);
}

TEST(CommandLine, Defaults) {
    auto r = parse({});
    ASSERT_TRUE(r.ok());
    const Options& o = r.value();
    EXPECT_FALSE(o.show_help);
    EXPECT_FALSE(o.show_status);
    EXPECT_FALSE(o.smoke_test);
    EXPECT_FALSE(o.max_frames.has_value());
    EXPECT_FALSE(o.game.has_value());
    EXPECT_EQ(o.log_level, LogLevel::Info);
}

TEST(CommandLine, Flags) {
    auto r = parse({"--help", "--version", "--status", "--smoke-test"});
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(r.value().show_help);
    EXPECT_TRUE(r.value().show_version);
    EXPECT_TRUE(r.value().show_status);
    EXPECT_TRUE(r.value().smoke_test);
}

TEST(CommandLine, ValuesAndGame) {
    auto r = parse({"--frames", "120", "--log-level", "debug", "--log-file", "out.log", "game.gdi"});
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(r.value().max_frames, 120u);
    EXPECT_EQ(r.value().log_level, LogLevel::Debug);
    EXPECT_EQ(r.value().log_file, std::filesystem::path("out.log"));
    EXPECT_EQ(r.value().game, std::filesystem::path("game.gdi"));
}

TEST(CommandLine, Errors) {
    for (auto args : std::vector<std::vector<std::string_view>>{
             {"--bogus"},
             {"--frames"},
             {"--frames", "0"},
             {"--frames", "12x"},
             {"--frames", "-3"},
             {"--log-level", "loud"},
             {"--log-file"},
             {"a.gdi", "b.gdi"},
         }) {
        auto r = parse_command_line(args);
        EXPECT_FALSE(r.ok()) << args.front();
        if (!r.ok()) {
            EXPECT_EQ(r.error().code, ErrorCode::InvalidArgument);
        }
    }
}

TEST(CommandLine, BiosOptions) {
    auto r = parse({"--bios", "dc_boot.bin", "--flash", "dc_flash.bin", "--run-cycles", "200000000"});
    ASSERT_TRUE(r.ok()) << r.error().message;
    EXPECT_EQ(r.value().bios, std::filesystem::path("dc_boot.bin"));
    EXPECT_EQ(r.value().flash, std::filesystem::path("dc_flash.bin"));
    EXPECT_EQ(r.value().run_cycles, 200000000u);
}

TEST(CommandLine, BiosOptionDependencies) {
    EXPECT_FALSE(parse({"--run-cycles", "10"}).ok()) << "--run-cycles needs --bios";
    EXPECT_FALSE(parse({"--flash", "f.bin"}).ok()) << "--flash needs --bios";
    EXPECT_FALSE(parse({"--bios", "b.bin", "--run-cycles", "0"}).ok());
    EXPECT_FALSE(parse({"--bios"}).ok());
}

TEST(CommandLine, UsageMentionsLimitations) {
    EXPECT_NE(usage_text().find("games cannot be run"), std::string::npos);
}

} // namespace
} // namespace nc::app
