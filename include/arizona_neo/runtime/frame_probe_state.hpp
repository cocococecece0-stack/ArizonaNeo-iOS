#pragma once

#include <atomic>
#include <cstdint>
#include <limits>

namespace arizona_neo::runtime {

// Thread-safe, allocation-free state for the iOS frame probe.
//
// The probe runs inside a CADisplayLink callback. It must never perform file
// I/O, allocate, or call into the Lua runtime there. This helper only counts
// ticks and decides whether the caller may emit one rate-limited log entry.
class FrameProbeState {
public:
    static constexpr std::int64_t kLogIntervalNs = 1'000'000'000;
    static constexpr std::int64_t kNever = std::numeric_limits<std::int64_t>::min();

    // Records one tick. Returns true when this tick is the first tick in a new
    // one-second window; `count` receives the cumulative tick count.
    bool should_log(std::int64_t now_ns, std::uint64_t& count) noexcept {
        count = count_.fetch_add(1, std::memory_order_relaxed) + 1;
        std::int64_t last = last_log_ns_.load(std::memory_order_relaxed);
        if (last != kNever && now_ns - last < kLogIntervalNs) {
            return false;
        }
        if (!last_log_ns_.compare_exchange_strong(
                last, now_ns, std::memory_order_acq_rel, std::memory_order_relaxed)) {
            return false;
        }
        return true;
    }

    std::uint64_t count() const noexcept {
        return count_.load(std::memory_order_acquire);
    }

    std::int64_t last_log_ns() const noexcept {
        return last_log_ns_.load(std::memory_order_acquire);
    }

    void reset() noexcept {
        count_.store(0, std::memory_order_release);
        last_log_ns_.store(kNever, std::memory_order_release);
    }

private:
    std::atomic<std::uint64_t> count_{0};
    std::atomic<std::int64_t> last_log_ns_{kNever};
};

} // namespace arizona_neo::runtime
