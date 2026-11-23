#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace arizona_neo::core {

// A deterministic, single-threaded scheduler driven by explicit tick() calls.
//
// No hidden worker threads and no platform timers. Time advances only when
// tick(delta) is called, which makes execution reproducible and testable.
class Scheduler {
public:
    using TaskId = std::uint64_t;
    using Callback = std::function<void()>;
    using Duration = std::chrono::milliseconds;

    static constexpr TaskId kInvalidTaskId = 0;

    Scheduler() = default;

    // Schedule a one-shot task after `delay`.
    TaskId schedule_after(Duration delay, Callback callback);

    // Schedule a periodic task. `initial_delay` defaults to `period`.
    // Returns kInvalidTaskId if period <= 0 (to avoid infinite loops).
    TaskId schedule_periodic(Duration period, Callback callback, Duration initial_delay = Duration{0});

    // Convenience: schedule_after with a period (period > 0 => periodic).
    TaskId schedule_after(Duration delay, Callback callback, Duration period);

    // Cancel a task. Returns true if it was pending.
    bool cancel(TaskId id);

    // Advance time by `delta` and run all due tasks.
    // Callbacks are invoked outside any internal lock. Exceptions thrown by
    // a callback are caught and do not stop other tasks.
    void tick(Duration delta);

    std::size_t pending_count() const;

private:
    struct Task {
        TaskId id = kInvalidTaskId;
        Duration remaining{0};
        Duration period{0};
        bool periodic = false;
        bool cancelled = false;
        Callback callback;
    };

    mutable std::mutex mutex_;
    std::vector<Task> tasks_;
    TaskId next_id_ = 1;
};

} // namespace arizona_neo::core
