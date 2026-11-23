#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace arizona_neo::core {

// A simple, deterministic event bus.
//
// Threading model: all methods are mutex-protected. Handlers are invoked
// outside the internal mutex, so a handler may safely subscribe/unsubscribe
// or publish. Subscriptions created during a publish() are not invoked by
// that in-flight publish (predictable snapshot semantics). Unsubscriptions
// during publish take effect for subsequent publishes.
class EventBus {
public:
    using Handler = std::function<void(const std::string& event_name)>;

    struct SubscriptionToken {
        std::uint64_t id = 0;
        bool valid() const noexcept { return id != 0; }
    };

    EventBus() = default;

    // Subscribe to a named event. Returns a token for later unsubscription.
    SubscriptionToken subscribe(std::string event_name, Handler handler);

    // Unsubscribe by token. Returns true if a subscription was removed.
    bool unsubscribe(SubscriptionToken token);

    // Publish an event to all current subscribers. Handler exceptions are
    // isolated and do not stop other handlers.
    void publish(const std::string& event_name);

    std::size_t subscriber_count(const std::string& event_name) const;

private:
    struct Entry {
        std::uint64_t token = 0;
        std::string event_name;
        Handler handler;
    };

    mutable std::mutex mutex_;
    std::vector<Entry> entries_;
    std::uint64_t next_token_ = 1;
};

} // namespace arizona_neo::core
