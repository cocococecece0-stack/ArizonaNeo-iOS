#include "arizona_neo/core/logger.hpp"

#include <iostream>

namespace arizona_neo::core {

const char* to_string(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warning: return "WARNING";
        case LogLevel::Error: return "ERROR";
    }
    return "UNKNOWN";
}

Logger::Logger(LogLevel min_level) noexcept : min_level_(min_level) {}

void Logger::set_min_level(LogLevel level) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    min_level_ = level;
}

LogLevel Logger::min_level() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return min_level_;
}

void Logger::set_sink(Sink sink) {
    std::lock_guard<std::mutex> lock(mutex_);
    sink_ = std::move(sink);
}

void Logger::log(LogLevel level, std::string_view message) {
    Sink sink_copy;
    LogLevel current_min;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        current_min = min_level_;
        if (level < current_min) {
            return;
        }
        sink_copy = sink_;
    }

    if (!sink_copy) {
        return;
    }

    LogMessage msg{level, std::string(message)};
    try {
        sink_copy(msg);
    } catch (const std::exception& e) {
        std::cerr << "Logger sink threw: " << e.what() << "\n";
    } catch (...) {
        std::cerr << "Logger sink threw unknown exception\n";
    }
}

} // namespace arizona_neo::core
