#include "arizona_neo/core/event_bus.hpp"
#include "arizona_neo/game/game_bridge.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace arizona_neo;

namespace {

int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << (msg) << "\n"; ++failures; } } while (0)

// Test double only. It models an adapter contract, not a real game API.
// No native symbol is called and no game data is invented.
class TestGameInterface final : public game::GameInterface {
public:
    bool ready() const noexcept override { return ready_; }
    std::vector<game::GameCapability> supported() const override {
        return {game::GameCapability::Events, game::GameCapability::Lifecycle};
    }
    std::optional<game::GameEvent> poll_event() override {
        if (events.empty()) return std::nullopt;
        auto event = events.front();
        events.erase(events.begin());
        return event;
    }
    std::string lifecycle_state() const override { return "running"; }

    bool ready_ = true;
    std::vector<game::GameEvent> events;
};

game::GameCapabilities events_capabilities() {
    auto caps = game::default_capabilities();
    caps.set(game::GameCapability::Events, game::CapabilityStatus::Available,
             "test double with confirmed adapter contract");
    return caps;
}

} // namespace

int main() {
    core::EventBus bus;
    game::GameBridge bridge(bus);

    CHECK(!bridge.ready(), "null interface is not ready");
    CHECK(!bridge.pump(), "null interface delivers no events");
    CHECK(bridge.lifecycle_state() == "unavailable", "null lifecycle is unavailable");
    CHECK(!bridge.capability_available(game::GameCapability::Events),
          "default capabilities do not expose events");

    TestGameInterface adapter;
    adapter.events.push_back({game::GameBridge::kEventName, "test-event-1"});
    adapter.events.push_back({game::GameBridge::kEventName, "test-event-2"});

    // Even a ready adapter cannot deliver while the capability is not Available.
    bridge.attach(&adapter);
    CHECK(bridge.ready(), "ready adapter is detected");
    CHECK(!bridge.pump(), "capability gate blocks unconfirmed events");

    // With an explicit Available capability, events reach EventBus.
    game::GameBridge enabled(bus, events_capabilities());
    enabled.attach(&adapter);
    CHECK(enabled.ready(), "enabled bridge is ready");
    int delivered = 0;
    const auto token = bus.subscribe(game::GameBridge::kEventName,
                                     [&](const std::string&) { ++delivered; });
    const auto count = enabled.pump_all(8);
    CHECK(count == 2, "both queued events were delivered");
    CHECK(delivered == 2, "EventBus received game events");
    CHECK(enabled.pump() == false, "no events remain after pump_all");

    // Boundary rule: readiness AND capability are both required.
    game::NativeCallBoundary boundary;
    CHECK(boundary.allow(true, true), "boundary allows confirmed use");
    CHECK(!boundary.allow(true, false), "boundary rejects missing capability");
    CHECK(!boundary.allow(false, true), "boundary rejects unready adapter");

    enabled.detach();
    CHECK(!enabled.ready(), "detach restores null interface");
    CHECK(enabled.pump() == false, "detached bridge delivers nothing");
    CHECK(bus.unsubscribe(token), "test subscription removed");

    bool all_unavailable = true;
    const auto baseline_caps = game::default_capabilities();
    for (const auto& entry : baseline_caps.all()) {
        if (entry.second.status != game::CapabilityStatus::Unavailable &&
            entry.second.status != game::CapabilityStatus::PendingConfirmedInterface) {
            all_unavailable = false;
        }
    }
    CHECK(all_unavailable, "baseline registry never reports available capabilities");

    if (failures == 0) std::cout << "game_bridge tests: PASSED\n";
    else std::cerr << "game_bridge tests: " << failures << " FAILED\n";
    return failures == 0 ? 0 : 1;
}
