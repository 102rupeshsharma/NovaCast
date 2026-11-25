#include "common/error.h"

namespace nc {

const char* error_code_name(ErrorCode code) {
    switch (code) {
    case ErrorCode::InvalidArgument:
        return "invalid argument";
    case ErrorCode::FileNotFound:
        return "file not found";
    case ErrorCode::FileTooLarge:
        return "file too large";
    case ErrorCode::IoError:
        return "I/O error";
    case ErrorCode::InvalidFormat:
        return "invalid format";
    case ErrorCode::Unsupported:
        return "unsupported";
    case ErrorCode::NotImplemented:
        return "not implemented";
    case ErrorCode::InitFailed:
        return "initialization failed";
    case ErrorCode::Internal:
        return "internal error";
    }
    return "unknown error";
}

std::string Error::to_string() const {
    std::string text = error_code_name(code);
    if (!message.empty()) {
        text += ": ";
        text += message;
    }
    return text;
}

} // namespace nc
