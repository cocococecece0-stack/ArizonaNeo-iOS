#pragma once

#include <optional>
#include <string>
#include <vector>

namespace arizona_neo::game {

// Capabilities are intentionally neutral: they describe what an adapter can
// provide, not how a game symbol is called. No native symbol, address, offset
// or calling convention is assumed here.
enum class GameCapability {
    Lifecycle,
    Events,
    Tick,
    Chat,
    PlayerData,
    VehicleData,
    UI
};

const char* to_string(GameCapability capability) noexcept;

// A game event delivered by an adapter. Payload is text-only: no raw pointers
// or native handles cross into the Lua runtime.
struct GameEvent {
    std::string name;
    std::string payload;
};

// Contract for a native adapter. An adapter must be implemented against a
// confirmed game interface before it can report readiness. This project does
// not ship an adapter that calls unverified symbols.
class GameInterface {
public:
    virtual ~GameInterface() = default;

    virtual bool ready() const noexcept = 0;
    virtual std::vector<GameCapability> supported() const = 0;
    virtual std::optional<GameEvent> poll_event() = 0;
    virtual void pump() {}
    virtual std::string lifecycle_state() const { return "unavailable"; }
};

// Default interface when no confirmed game adapter exists.
class NullGameInterface final : public GameInterface {
public:
    bool ready() const noexcept override { return false; }
    std::vector<GameCapability> supported() const override { return {}; }
    std::optional<GameEvent> poll_event() override { return std::nullopt; }
    std::string lifecycle_state() const override { return "unavailable"; }
};

} // namespace arizona_neo::game
