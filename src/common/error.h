#pragma once

#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace nc {

enum class ErrorCode {
    InvalidArgument,
    FileNotFound,
    FileTooLarge,
    IoError,
    InvalidFormat,
    Unsupported,
    NotImplemented,
    InitFailed,
    Internal,
};

const char* error_code_name(ErrorCode code);

struct Error {
    ErrorCode code = ErrorCode::Internal;
    std::string message;

    // "<code name>: <message>"
    std::string to_string() const;
};

inline Error make_error(ErrorCode code, std::string message) {
    return Error{code, std::move(message)};
}

// Either a value or an Error. Recoverable failures are returned this way;
// exceptions are reserved for programming errors and out-of-memory.
template <typename T>
class [[nodiscard]] Result {
public:
    Result(T value) : data_(std::in_place_index<0>, std::move(value)) {}
    Result(Error error) : data_(std::in_place_index<1>, std::move(error)) {}

    bool ok() const noexcept { return data_.index() == 0; }
    explicit operator bool() const noexcept { return ok(); }

    // Accessing the wrong alternative throws std::bad_variant_access.
    T& value() & { return std::get<0>(data_); }
    const T& value() const& { return std::get<0>(data_); }
    T&& value() && { return std::get<0>(std::move(data_)); }

    const Error& error() const { return std::get<1>(data_); }

private:
    std::variant<T, Error> data_;
};

template <>
class [[nodiscard]] Result<void> {
public:
    Result() = default;
    Result(Error error) : error_(std::move(error)) {}

    bool ok() const noexcept { return !error_.has_value(); }
    explicit operator bool() const noexcept { return ok(); }

    // Throws std::bad_optional_access when called on a successful result.
    const Error& error() const { return error_.value(); }

private:
    std::optional<Error> error_;
};

} // namespace nc
