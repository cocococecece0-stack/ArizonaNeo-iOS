#include "arizona_neo/core/event_bus.hpp"

namespace arizona_neo::core {

EventBus::SubscriptionToken EventBus::subscribe(std::string event_name, Handler handler) {
    if (!handler) return SubscriptionToken{0};
    std::lock_guard<std::mutex> lock(mutex_);
    SubscriptionToken tok{next_token_++};
    entries_.push_back(Entry{tok.id, std::move(event_name), std::move(handler)});
    return tok;
}

bool EventBus::unsubscribe(SubscriptionToken token) {
    if (!token.valid()) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = entries_.begin(); it != entries_.end(); ++it) {
        if (it->token == token.id) {
            entries_.erase(it);
            return true;
        }
    }
    return false;
}

void EventBus::publish(const std::string& event_name) {
    // Snapshot handlers under lock.
    std::vector<Handler> handlers;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& e : entries_) {
            if (e.event_name == event_name) {
                handlers.push_back(e.handler);
            }
        }
    }
    for (const auto& h : handlers) {
        try {
            h(event_name);
        } catch (...) {
            // Isolate handler exceptions.
        }
    }
}

std::size_t EventBus::subscriber_count(const std::string& event_name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t n = 0;
    for (const auto& e : entries_) {
        if (e.event_name == event_name) ++n;
    }
    return n;
}

} // namespace arizona_neo::core
