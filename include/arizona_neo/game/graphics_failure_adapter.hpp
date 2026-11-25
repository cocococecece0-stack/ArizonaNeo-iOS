#pragma once

#include "arizona_neo/game/game_interface.hpp"
#include "arizona_neo/game/ios_native_interface.hpp"

#include <deque>
#include <atomic>
#include <mutex>
#include <string>

namespace arizona_neo::game {

// iOS adapter for the confirmed graphics-failure callback interface.
// It registers a C ABI trampoline with the game-provided setter and converts
// each callback into a queued GameEvent. No game memory is read and no
// unverified symbol is resolved.
class GraphicsFailureAdapter final : public GameInterface {
public:
    explicit GraphicsFailureAdapter(IosGraphicsFailureInterface native) noexcept;
    ~GraphicsFailureAdapter() override;

    GraphicsFailureAdapter(const GraphicsFailureAdapter&) = delete;
    GraphicsFailureAdapter& operator=(const GraphicsFailureAdapter&) = delete;

    bool ready() const noexcept override;
    std::vector<GameCapability> supported() const override;

    // Unregisters the native callback and drops queued events. Safe to call
    // more than once; callbacks racing with shutdown are ignored.
    void shutdown() noexcept;

    std::optional<GameEvent> poll_event() override;
    std::string lifecycle_state() const override { return "running"; }

    static constexpr const char* kEventName = "game.graphics_failure";

private:
    static void trampoline(void* context) noexcept;
    void on_native_callback() noexcept;

    IosGraphicsFailureInterface native_;
    std::atomic<bool> active_{false};
    std::mutex mutex_;
    std::deque<GameEvent> queue_;
};

} // namespace arizona_neo::game
