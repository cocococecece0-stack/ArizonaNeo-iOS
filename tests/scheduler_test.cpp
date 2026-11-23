#include "arizona_neo/core/scheduler.hpp"

#include <functional>
#include <iostream>
#include <string>
#include <vector>

using namespace arizona_neo::core;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << (msg) << "\n"; ++failures; } } while (0)

int main() {
    // delayed call
    {
        Scheduler s;
        int count = 0;
        s.schedule_after(std::chrono::milliseconds(10), [&]() { ++count; });
        s.tick(std::chrono::milliseconds(5));
        CHECK(count == 0, "not yet fired");
        s.tick(std::chrono::milliseconds(5));
        CHECK(count == 1, "fired after delay");
        s.tick(std::chrono::milliseconds(5));
        CHECK(count == 1, "one-shot not repeated");
    }

    // cancel
    {
        Scheduler s;
        int count = 0;
        auto id = s.schedule_after(std::chrono::milliseconds(5), [&]() { ++count; });
        CHECK(s.cancel(id), "cancel returns true");
        CHECK(!s.cancel(id), "cancel twice false");
        s.tick(std::chrono::milliseconds(10));
        CHECK(count == 0, "cancelled not fired");
    }

    // periodic
    {
        Scheduler s;
        int count = 0;
        s.schedule_periodic(std::chrono::milliseconds(5), [&]() { ++count; });
        s.tick(std::chrono::milliseconds(5));
        CHECK(count == 1, "periodic first");
        s.tick(std::chrono::milliseconds(5));
        CHECK(count == 2, "periodic second");
        s.tick(std::chrono::milliseconds(3));
        CHECK(count == 2, "not yet third");
    }

    // multiple tasks
    {
        Scheduler s;
        std::vector<int> order;
        s.schedule_after(std::chrono::milliseconds(5), [&]() { order.push_back(1); });
        s.schedule_after(std::chrono::milliseconds(5), [&]() { order.push_back(2); });
        s.tick(std::chrono::milliseconds(5));
        CHECK(order.size() == 2, "both fired");
        CHECK(order[0] == 1 && order[1] == 2, "order preserved");
    }

    // callback exception
    {
        Scheduler s;
        int count = 0;
        s.schedule_after(std::chrono::milliseconds(1), [&]() { throw std::runtime_error("x"); });
        s.schedule_after(std::chrono::milliseconds(1), [&]() { ++count; });
        s.tick(std::chrono::milliseconds(1));
        CHECK(count == 1, "second runs after exception");
    }

    // determinism: same deltas same result
    {
        Scheduler a, b;
        int ca = 0, cb = 0;
        a.schedule_after(std::chrono::milliseconds(3), [&]() { ++ca; });
        b.schedule_after(std::chrono::milliseconds(3), [&]() { ++cb; });
        a.tick(std::chrono::milliseconds(1)); a.tick(std::chrono::milliseconds(1)); a.tick(std::chrono::milliseconds(1));
        b.tick(std::chrono::milliseconds(3));
        CHECK(ca == cb && ca == 1, "determinism");
    }

    // periodic zero period rejected
    {
        Scheduler s;
        auto id = s.schedule_periodic(std::chrono::milliseconds(0), []() {});
        CHECK(id == Scheduler::kInvalidTaskId, "zero period rejected");
    }

    // pending_count
    {
        Scheduler s;
        s.schedule_after(std::chrono::milliseconds(10), []() {});
        CHECK(s.pending_count() == 1, "pending count");
        s.tick(std::chrono::milliseconds(10));
        CHECK(s.pending_count() == 0, "pending after fire");
    }

    // self-reschedule does not infinite loop in same tick
    {
        Scheduler s;
        int count = 0;
        std::function<void()> cb = [&]() {
            ++count;
            if (count < 3) s.schedule_after(std::chrono::milliseconds(0), cb);
        };
        s.schedule_after(std::chrono::milliseconds(0), cb);
        s.tick(std::chrono::milliseconds(0));
        CHECK(count == 1, "only first runs in tick");
    }

    if (failures == 0) std::cout << "scheduler tests: PASSED\n";
    else std::cerr << "scheduler tests: " << failures << " FAILED\n";
    return failures == 0 ? 0 : 1;
}
