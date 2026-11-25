#pragma once

#include "arizona_neo/core/event_bus.hpp"
#include "arizona_neo/game/game_capabilities.hpp"
#include "arizona_neo/game/game_interface.hpp"

#include <functional>
#include <string>

namespace arizona_neo::game {

// Native call boundary: no native call is attempted unless the attached
// adapter explicitly reports readiness AND the capability is Available.
// The boundary itself performs no dynamic loading or symbol resolution.
class NativeCallBoundary {
public:
    bool allow(bool interface_ready, bool capability_available) const noexcept {
        return interface_ready && capability_available;
    }
};

// Deterministic bridge that forwards adapter events into EventBus and exposes
// capability state. It never opens libraries or resolves game symbols.
class GameBridge {
public:
    GameBridge(core::EventBus& bus, GameCapabilities capabilities = default_capabilities());

    void attach(GameInterface* interface) noexcept;
    void detach() noexcept;

    bool ready() const noexcept;
    GameInterface* interface() const noexcept { return interface_; }

    // Polls at most one pending event and publishes it to EventBus.
    bool pump();

    // Delivers all currently pending events, bounded by limit.
    std::size_t pump_all(std::size_t limit = 64);

    const GameCapabilities& capabilities() const noexcept { return capabilities_; }
    bool capability_available(GameCapability capability) const;
    std::string lifecycle_state() const;

    // Events are published under this EventBus name. Payload is text-only.
    static constexpr const char* kEventName = "game.event";

private:
    bool can_use(GameCapability capability) const noexcept;

    core::EventBus& bus_;
    GameCapabilities capabilities_;
    GameInterface* interface_ = nullptr;
    NullGameInterface null_interface_;
    NativeCallBoundary boundary_;
};

} // namespace arizona_neo::game
