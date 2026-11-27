#include "arizona_neo/runtime/frame_probe_state.hpp"

#include <atomic>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

namespace {

int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << (msg) << "\n"; ++failures; } } while (0)

using arizona_neo::runtime::FrameProbeState;

} // namespace

int main() {
    FrameProbeState state;
    CHECK(state.count() == 0, "initial tick count is zero");
    CHECK(state.last_log_ns() == FrameProbeState::kNever, "initial log timestamp is unset");

    std::uint64_t count = 0;
    CHECK(state.should_log(1'000'000'000, count), "first tick logs");
    CHECK(count == 1, "first tick count");
    CHECK(!state.should_log(1'500'000'000, count), "tick within one second is throttled");
    CHECK(count == 2, "throttled tick still increments the counter");
    CHECK(state.should_log(2'000'000'000, count), "tick at the one-second boundary logs");
    CHECK(count == 3, "boundary tick count");
    CHECK(!state.should_log(2'999'999'999, count), "tick just before boundary is throttled");
    CHECK(state.should_log(3'000'000'000, count), "next boundary logs");
    CHECK(count == 5, "counter survives throttling");

    state.reset();
    CHECK(state.count() == 0, "reset clears count");
    CHECK(state.should_log(42, count), "reset allows the next tick to log");
    CHECK(count == 1, "post-reset tick count");

    // Concurrent ticks must all be counted; the rate limiter must not lose
    // increments or crash. We do not require a specific number of log windows.
    state.reset();
    constexpr int kThreads = 4;
    constexpr int kTicksPerThread = 10'000;
    std::vector<std::thread> threads;
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&state, i]() {
            std::uint64_t local = 0;
            const std::int64_t base = static_cast<std::int64_t>(i) * 10'000'000;
            for (int j = 0; j < kTicksPerThread; ++j) {
                state.should_log(base + j, local);
            }
        });
    }
    for (auto& thread : threads) thread.join();
    CHECK(state.count() == static_cast<std::uint64_t>(kThreads * kTicksPerThread),
          "all concurrent ticks are counted");

    if (failures == 0) std::cout << "frame probe state tests: PASSED\n";
    else std::cerr << "frame probe state tests: " << failures << " FAILED\n";
    return failures == 0 ? 0 : 1;
}
