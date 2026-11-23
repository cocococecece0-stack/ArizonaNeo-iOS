#include "arizona_neo/core/event_bus.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace arizona_neo::core;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << (msg) << "\n"; ++failures; } } while (0)

int main() {
    // multiple subscribers
    {
        EventBus bus;
        int a = 0, b = 0;
        bus.subscribe("e", [&](const std::string&) { ++a; });
        bus.subscribe("e", [&](const std::string&) { ++b; });
        bus.publish("e");
        CHECK(a == 1 && b == 1, "multiple subscribers");
        CHECK(bus.subscriber_count("e") == 2, "subscriber_count");
    }

    // unsubscribe
    {
        EventBus bus;
        int count = 0;
        auto tok = bus.subscribe("e", [&](const std::string&) { ++count; });
        CHECK(bus.unsubscribe(tok), "unsubscribe ok");
        CHECK(!bus.unsubscribe(tok), "unsubscribe twice false");
        bus.publish("e");
        CHECK(count == 0, "unsubscribed not called");
    }

    // subscribe during dispatch not called in same dispatch
    {
        EventBus bus;
        int count = 0;
        bus.subscribe("e", [&](const std::string&) {
            bus.subscribe("e", [&](const std::string&) { ++count; });
        });
        bus.publish("e");
        CHECK(count == 0, "new sub not called in same dispatch");
        bus.publish("e");
        CHECK(count == 1, "new sub called next dispatch");
    }

    // unsubscribe during dispatch
    {
        EventBus bus;
        int count = 0;
        EventBus::SubscriptionToken tok{};
        tok = bus.subscribe("e", [&](const std::string&) {
            bus.unsubscribe(tok);
            ++count;
        });
        bus.publish("e");
        CHECK(count == 1, "handler ran before unsubscribe");
        // second publish: handler removed
        bus.publish("e");
        CHECK(count == 1, "unsubscribed handler not called");
    }

    // handler exception isolation
    {
        EventBus bus;
        int count = 0;
        bus.subscribe("e", [&](const std::string&) { throw std::runtime_error("x"); });
        bus.subscribe("e", [&](const std::string&) { ++count; });
        bus.publish("e");
        CHECK(count == 1, "second runs after exception");
    }

    // different events
    {
        EventBus bus;
        int a = 0, b = 0;
        bus.subscribe("a", [&](const std::string&) { ++a; });
        bus.subscribe("b", [&](const std::string&) { ++b; });
        bus.publish("a");
        CHECK(a == 1 && b == 0, "event isolation");
    }

    if (failures == 0) std::cout << "event_bus tests: PASSED\n";
    else std::cerr << "event_bus tests: " << failures << " FAILED\n";
    return failures == 0 ? 0 : 1;
}
