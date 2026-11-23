#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <string_view>

namespace arizona_neo::core {

// Severity levels for Logger.
enum class LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4
};

// Convert a LogLevel to a stable string. Never throws.
const char* to_string(LogLevel level) noexcept;

struct LogMessage {
    LogLevel level = LogLevel::Info;
    std::string message;
};

// A small, thread-safe, dependency-free logger.
//
// - No global state: each Logger instance owns its own sink and minimum level.
// - The user-supplied sink is never invoked while the internal mutex is held.
// - Exceptions thrown by the sink are caught and reported to stderr (best effort).
class Logger {
public:
    using Sink = std::function<void(const LogMessage&)>;

    Logger() = default;
    explicit Logger(LogLevel min_level) noexcept;

    // Minimum severity that will be forwarded to the sink.
    void set_min_level(LogLevel level) noexcept;
    LogLevel min_level() const noexcept;

    // Replace the sink. Passing an empty function disables output.
    void set_sink(Sink sink);

    // Forward a message if its level >= min_level().
    void log(LogLevel level, std::string_view message);

    void trace(std::string_view message) { log(LogLevel::Trace, message); }
    void debug(std::string_view message) { log(LogLevel::Debug, message); }
    void info(std::string_view message) { log(LogLevel::Info, message); }
    void warning(std::string_view message) { log(LogLevel::Warning, message); }
    void error(std::string_view message) { log(LogLevel::Error, message); }

private:
    mutable std::mutex mutex_;
    LogLevel min_level_ = LogLevel::Info;
    Sink sink_;
};

} // namespace arizona_neo::core
