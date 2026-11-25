#include "common/error.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <variant>

namespace nc {
namespace {

TEST(Result, HoldsValue) {
    Result<int> r = 42;
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(static_cast<bool>(r));
    EXPECT_EQ(r.value(), 42);
}

TEST(Result, HoldsError) {
    Result<int> r = make_error(ErrorCode::InvalidFormat, "bad header");
    ASSERT_FALSE(r.ok());
    EXPECT_EQ(r.error().code, ErrorCode::InvalidFormat);
    EXPECT_EQ(r.error().message, "bad header");
    EXPECT_THROW((void)r.value(), std::bad_variant_access);
}

TEST(Result, SupportsMoveOnlyValues) {
    Result<std::unique_ptr<int>> r = std::make_unique<int>(7);
    ASSERT_TRUE(r.ok());
    std::unique_ptr<int> owned = std::move(r).value();
    ASSERT_NE(owned, nullptr);
    EXPECT_EQ(*owned, 7);
}

TEST(Result, VoidSuccessAndFailure) {
    Result<void> ok;
    EXPECT_TRUE(ok.ok());

    Result<void> failed = make_error(ErrorCode::IoError, "disk full");
    ASSERT_FALSE(failed.ok());
    EXPECT_EQ(failed.error().code, ErrorCode::IoError);
}

TEST(Error, ToStringIncludesCodeAndMessage) {
    EXPECT_EQ(make_error(ErrorCode::NotImplemented, "GD-ROM").to_string(), "not implemented: GD-ROM");
    EXPECT_EQ(make_error(ErrorCode::Internal, "").to_string(), "internal error");
}

TEST(Error, EveryCodeHasAName) {
    for (ErrorCode code : {ErrorCode::InvalidArgument, ErrorCode::FileNotFound, ErrorCode::FileTooLarge,
                           ErrorCode::IoError, ErrorCode::InvalidFormat, ErrorCode::Unsupported,
                           ErrorCode::NotImplemented, ErrorCode::InitFailed, ErrorCode::Internal}) {
        EXPECT_STRNE(error_code_name(code), "unknown error");
    }
}

} // namespace
} // namespace nc
