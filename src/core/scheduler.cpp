#include "arizona_neo/core/scheduler.hpp"

#include <algorithm>

namespace arizona_neo::core {

Scheduler::TaskId Scheduler::schedule_after(Duration delay, Callback callback) {
    if (!callback) return kInvalidTaskId;
    std::lock_guard<std::mutex> lock(mutex_);
    Task t;
    t.id = next_id_++;
    t.remaining = delay;
    t.period = Duration{0};
    t.periodic = false;
    t.cancelled = false;
    t.callback = std::move(callback);
    tasks_.push_back(std::move(t));
    return tasks_.back().id;
}

Scheduler::TaskId Scheduler::schedule_periodic(Duration period, Callback callback, Duration initial_delay) {
    if (!callback) return kInvalidTaskId;
    if (period.count() <= 0) return kInvalidTaskId;
    std::lock_guard<std::mutex> lock(mutex_);
    Task t;
    t.id = next_id_++;
    t.remaining = (initial_delay.count() > 0) ? initial_delay : period;
    t.period = period;
    t.periodic = true;
    t.cancelled = false;
    t.callback = std::move(callback);
    tasks_.push_back(std::move(t));
    return tasks_.back().id;
}

Scheduler::TaskId Scheduler::schedule_after(Duration delay, Callback callback, Duration period) {
    if (period.count() > 0) {
        return schedule_periodic(period, std::move(callback), delay);
    }
    return schedule_after(delay, std::move(callback));
}

bool Scheduler::cancel(TaskId id) {
    if (id == kInvalidTaskId) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& t : tasks_) {
        if (t.id == id && !t.cancelled) {
            t.cancelled = true;
            return true;
        }
    }
    return false;
}

void Scheduler::tick(Duration delta) {
    // Determine due task IDs under lock.
    std::vector<TaskId> due_ids;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& t : tasks_) {
            if (t.cancelled) continue;
            if (delta >= t.remaining) {
                t.remaining = Duration{0};
            } else {
                t.remaining -= delta;
            }
            if (t.remaining.count() <= 0) {
                due_ids.push_back(t.id);
            }
        }
    }

    for (TaskId id : due_ids) {
        Callback cb;
        bool periodic = false;
        Duration period{0};
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = std::find_if(tasks_.begin(), tasks_.end(),
                [id](const Task& t) { return t.id == id; });
            if (it == tasks_.end() || it->cancelled) continue;
            cb = it->callback;
            periodic = it->periodic;
            period = it->period;
        }
        if (!cb) continue;
        try {
            cb();
        } catch (...) {
            // Swallow; do not stop other tasks.
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = std::find_if(tasks_.begin(), tasks_.end(),
                [id](const Task& t) { return t.id == id; });
            if (it == tasks_.end()) continue;
            if (it->cancelled) continue;
            if (periodic) {
                it->remaining = period;
            } else {
                it->cancelled = true;
            }
        }
    }

    // Cleanup cancelled tasks.
    std::lock_guard<std::mutex> lock(mutex_);
    tasks_.erase(std::remove_if(tasks_.begin(), tasks_.end(),
        [](const Task& t) { return t.cancelled; }), tasks_.end());
}

std::size_t Scheduler::pending_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t n = 0;
    for (const auto& t : tasks_) if (!t.cancelled) ++n;
    return n;
}

} // namespace arizona_neo::core
