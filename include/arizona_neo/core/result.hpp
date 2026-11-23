#pragma once

#include <string>
#include <utility>

namespace arizona_neo::core {

// Unified error code for portable core components.
enum class CoreError {
    None = 0,
    InvalidArgument,
    InvalidPath,
    PathTraversal,
    SymlinkEscape,
    NotFound,
    AccessDenied,
    IsDirectory,
    NotDirectory,
    RootViolation,
    IoError,
    Cancelled,
    RuntimeUnavailable,
    InvalidTransition,
    Duplicate,
    NotFoundScript,
    Unknown
};

inline const char* to_string(CoreError e) noexcept {
    switch (e) {
        case CoreError::None: return "None";
        case CoreError::InvalidArgument: return "InvalidArgument";
        case CoreError::InvalidPath: return "InvalidPath";
        case CoreError::PathTraversal: return "PathTraversal";
        case CoreError::SymlinkEscape: return "SymlinkEscape";
        case CoreError::NotFound: return "NotFound";
        case CoreError::AccessDenied: return "AccessDenied";
        case CoreError::IsDirectory: return "IsDirectory";
        case CoreError::NotDirectory: return "NotDirectory";
        case CoreError::RootViolation: return "RootViolation";
        case CoreError::IoError: return "IoError";
        case CoreError::Cancelled: return "Cancelled";
        case CoreError::RuntimeUnavailable: return "RuntimeUnavailable";
        case CoreError::InvalidTransition: return "InvalidTransition";
        case CoreError::Duplicate: return "Duplicate";
        case CoreError::NotFoundScript: return "NotFoundScript";
        case CoreError::Unknown: return "Unknown";
    }
    return "Unknown";
}

// Result<T>: explicit success/failure without exceptions.
template <typename T>
struct Result {
    bool ok = false;
    CoreError error = CoreError::None;
    std::string message;
    T value{};

    static Result success(T v) {
        Result r;
        r.ok = true;
        r.error = CoreError::None;
        r.value = std::move(v);
        return r;
    }

    static Result failure(CoreError e, std::string msg = {}) {
        Result r;
        r.ok = false;
        r.error = e;
        r.message = std::move(msg);
        return r;
    }
};

template <>
struct Result<void> {
    bool ok = false;
    CoreError error = CoreError::None;
    std::string message;

    static Result success() {
        Result r;
        r.ok = true;
        r.error = CoreError::None;
        return r;
    }

    static Result failure(CoreError e, std::string msg = {}) {
        Result r;
        r.ok = false;
        r.error = e;
        r.message = std::move(msg);
        return r;
    }
};

} // namespace arizona_neo::core
