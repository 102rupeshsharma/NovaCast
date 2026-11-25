#include "common/fileio.h"
#include "common/log.h"
#include "test_helpers.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace nc {
namespace {

struct Captured {
    LogLevel level;
    std::string tag;
    std::string message;
};

void capture_sink(LogLevel level, const char* tag, const char* message, void* user) {
    static_cast<std::vector<Captured>*>(user)->push_back({level, tag, message});
}

// Installs a capturing sink and restores global log state afterwards.
class LogTest : public ::testing::Test {
protected:
    void SetUp() override {
        saved_level_ = log_level();
        log_set_sink(capture_sink, &captured_);
    }
    void TearDown() override {
        log_set_sink(nullptr, nullptr);
        log_close_file();
        log_set_level(saved_level_);
    }

    std::vector<Captured> captured_;
    LogLevel saved_level_ = LogLevel::Info;
};

TEST_F(LogTest, FormatsMessages) {
    log_set_level(LogLevel::Trace);
    LOG_INFO("cpu", "pc=%08X value=%d", 0xA0000000u, -5);
    ASSERT_EQ(captured_.size(), 1u);
    EXPECT_EQ(captured_[0].level, LogLevel::Info);
    EXPECT_EQ(captured_[0].tag, "cpu");
    EXPECT_EQ(captured_[0].message, "pc=A0000000 value=-5");
}

TEST_F(LogTest, FiltersBelowLevel) {
    log_set_level(LogLevel::Warn);
    LOG_DEBUG("t", "hidden");
    LOG_INFO("t", "hidden");
    LOG_WARN("t", "shown");
    LOG_ERROR("t", "shown");
    ASSERT_EQ(captured_.size(), 2u);
    EXPECT_EQ(captured_[0].level, LogLevel::Warn);
    EXPECT_EQ(captured_[1].level, LogLevel::Error);
}

TEST_F(LogTest, OffSilencesEverything) {
    log_set_level(LogLevel::Off);
    LOG_ERROR("t", "hidden");
    EXPECT_TRUE(captured_.empty());
}

TEST_F(LogTest, DoesNotEvaluateArgumentsWhenFiltered) {
    log_set_level(LogLevel::Error);
    int calls = 0;
    auto count = [&calls] { return ++calls; };
    LOG_DEBUG("t", "%d", count());
    EXPECT_EQ(calls, 0);
}

TEST_F(LogTest, TruncatesLongMessagesSafely) {
    log_set_level(LogLevel::Info);
    const std::string huge(5000, 'x');
    LOG_INFO("t", "%s", huge.c_str());
    ASSERT_EQ(captured_.size(), 1u);
    EXPECT_EQ(captured_[0].message.size(), 1023u);
}

TEST_F(LogTest, WritesToFile) {
    test::TempDir dir;
    const auto path = dir / "novacast.log";
    log_set_level(LogLevel::Info);
    ASSERT_TRUE(log_open_file(path).ok());
    LOG_INFO("disc", "mounted %s", "test.gdi");
    log_close_file();

    auto data = read_file(path);
    ASSERT_TRUE(data.ok()) << data.error().to_string();
    const std::string text(data.value().begin(), data.value().end());
    EXPECT_NE(text.find("disc: mounted test.gdi"), std::string::npos) << text;
    EXPECT_NE(text.find("[info ]"), std::string::npos) << text;
}

TEST_F(LogTest, OpenFileFailsForBadPath) {
    test::TempDir dir;
    auto r = log_open_file(dir / "missing-dir" / "x.log");
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::IoError);
}

TEST(LogLevelNames, RoundTrip) {
    for (LogLevel level : {LogLevel::Trace, LogLevel::Debug, LogLevel::Info, LogLevel::Warn, LogLevel::Error,
                           LogLevel::Off}) {
        auto parsed = parse_log_level(log_level_name(level));
        ASSERT_TRUE(parsed.has_value());
        EXPECT_EQ(*parsed, level);
    }
    EXPECT_FALSE(parse_log_level("verbose").has_value());
}

} // namespace
} // namespace nc
