#include "arizona_neo/game/graphics_failure_adapter.hpp"

#include <utility>

namespace arizona_neo::game {

GraphicsFailureAdapter::GraphicsFailureAdapter(IosGraphicsFailureInterface native) noexcept
    : native_(native) {
    if (native_.set_handler() != nullptr) {
        native_.set_handler()(this, &GraphicsFailureAdapter::trampoline);
    }
}

GraphicsFailureAdapter::~GraphicsFailureAdapter() {
    // Unregister before destruction so the game cannot call a dead context.
    if (native_.set_handler() != nullptr) {
        native_.set_handler()(nullptr, nullptr);
    }
}

bool GraphicsFailureAdapter::ready() const noexcept {
    return native_.set_handler() != nullptr;
}

std::vector<GameCapability> GraphicsFailureAdapter::supported() const {
    if (!ready()) return {};
    return {GameCapability::Events, GameCapability::Lifecycle};
}

std::optional<GameEvent> GraphicsFailureAdapter::poll_event() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) return std::nullopt;
    GameEvent event = std::move(queue_.front());
    queue_.pop_front();
    return event;
}

void GraphicsFailureAdapter::trampoline(void* context) noexcept {
    if (context == nullptr) return;
    static_cast<GraphicsFailureAdapter*>(context)->on_native_callback();
}

void GraphicsFailureAdapter::on_native_callback() noexcept {
    // No C++ exception may cross the C ABI boundary. A fixed-capacity queue
    // keeps memory allocation out of the callback path after startup.
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.size() >= 32) return;
    queue_.push_back(GameEvent{kEventName, "confirmed_graphics_failure"});
}

} // namespace arizona_neo::game
