#include "arizona_neo/game/graphics_failure_adapter.hpp"

#include <utility>

namespace arizona_neo::game {

GraphicsFailureAdapter::GraphicsFailureAdapter(IosGraphicsFailureInterface native) noexcept
    : native_(native) {
    if (native_.set_handler() != nullptr) {
        active_.store(true);
        native_.set_handler()(this, &GraphicsFailureAdapter::trampoline);
    }
}

GraphicsFailureAdapter::~GraphicsFailureAdapter() {
    shutdown();
}

void GraphicsFailureAdapter::shutdown() noexcept {
    // Order matters: stop accepting callbacks before unregistering and before
    // destroying the context. A callback already in flight sees active_==false
    // and returns without touching the queue.
    active_.store(false);
    if (native_.set_handler() != nullptr) {
        native_.set_handler()(nullptr, nullptr);
    }
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.clear();
}

bool GraphicsFailureAdapter::ready() const noexcept {
    return native_.set_handler() != nullptr && active_.load();
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
    auto* adapter = static_cast<GraphicsFailureAdapter*>(context);
    if (!adapter->active_.load()) return;
    adapter->on_native_callback();
}

void GraphicsFailureAdapter::on_native_callback() noexcept {
    // No C++ exception may cross the C ABI boundary. A fixed-capacity queue
    // keeps memory allocation out of the callback path after startup.
    if (!active_.load()) return;
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.size() >= 32) return;
    queue_.push_back(GameEvent{kEventName, "confirmed_graphics_failure"});
}

} // namespace arizona_neo::game
