#include "arizona_neo/core/event_bus.hpp"
#include "arizona_neo/game/game_bridge.hpp"
#include "arizona_neo/game/graphics_failure_adapter.hpp"
#include "arizona_neo/game/ios_native_interface.hpp"

#include <iostream>

using namespace arizona_neo;

namespace {

int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << (msg) << "\n"; ++failures; } } while (0)

// Test implementation of the confirmed C ABI:
//   void set_handler(void* context, void (*handler)(void* context))
// The real game's _game_set_graphics_failure_handler stores exactly these two
// values and _game_notify_graphics_failure calls handler(context).
void* g_context = nullptr;
void (*g_handler)(void*) = nullptr;

void fake_set_handler(void* context, void (*handler)(void*)) {
    g_context = context;
    g_handler = handler;
}

void fake_game_notify() {
    if (g_handler != nullptr && g_context != nullptr) {
        g_handler(g_context);
    }
}

} // namespace

int main() {
    core::EventBus bus;
    game::GameBridge bridge(bus);

    // Capability cannot be promoted without a ready adapter.
    CHECK(!bridge.confirm_capability(game::GameCapability::Events, "no adapter"),
          "unready bridge rejects capability promotion");

    game::IosGraphicsFailureInterface native(&fake_set_handler);
    {
        game::GraphicsFailureAdapter adapter(native);
        CHECK(adapter.ready(), "confirmed setter accepted adapter registration");
        CHECK(g_context == &adapter, "game holds adapter context");
        CHECK(g_handler != nullptr, "game holds trampoline callback");

        bridge.attach(&adapter);
        CHECK(bridge.ready(), "bridge sees ready adapter");
        CHECK(bridge.confirm_capability(
                  game::GameCapability::Events,
                  "llvm-otool: _game_set_graphics_failure_handler / _game_notify_graphics_failure"),
              "confirmed capability promoted only after attach");
        CHECK(bridge.capability_available(game::GameCapability::Events),
              "events capability is available");

        int delivered = 0;
        const auto token = bus.subscribe(game::GameBridge::kEventName,
                                         [&](const std::string&) { ++delivered; });

        fake_game_notify();
        CHECK(bridge.pump(), "native callback queued an event and bridge delivered it");
        CHECK(delivered == 1, "EventBus received graphics failure event");
        CHECK(!bridge.pump(), "queue is empty after delivery");

        fake_game_notify();
        fake_game_notify();
        CHECK(bridge.pump_all(4) == 2, "multiple native callbacks are queued and delivered");
        CHECK(delivered == 3, "EventBus received all three callbacks");

        CHECK(bus.unsubscribe(token), "subscription removed");
        bridge.detach();
        CHECK(!bridge.ready(), "detach restores null interface");
    }

    // Destructor unregistered the callback: no use-after-free on later notify.
    CHECK(g_context == nullptr && g_handler == nullptr, "adapter unregistered itself");
    fake_game_notify();

    game::IosGraphicsFailureInterface missing(nullptr);
    game::GraphicsFailureAdapter no_adapter(missing);
    CHECK(!no_adapter.ready(), "missing native interface is not ready");
    CHECK(no_adapter.supported().empty(), "missing interface exposes no capabilities");

    if (failures == 0) std::cout << "native_interface tests: PASSED\n";
    else std::cerr << "native_interface tests: " << failures << " FAILED\n";
    return failures == 0 ? 0 : 1;
}
