#include "neo_bootstrap.h"

#include <atomic>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

namespace {

int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << (msg) << "\n"; ++failures; } } while (0)

} // namespace

int main() {
    neo_bootstrap_status_t status;

    CHECK(neo_bootstrap_get_status(nullptr) == NEO_BOOTSTRAP_INVALID_ARGUMENT,
          "null status rejected");
    CHECK(std::strcmp(neo_bootstrap_get_version(), "1.0.0-stage11") == 0,
          "bootstrap version exported");
    CHECK(neo_bootstrap_get_status(&status) == NEO_BOOTSTRAP_OK, "status before init");
    CHECK(status.state == NEO_BOOTSTRAP_NOT_STARTED, "initial state NOT_STARTED");
    CHECK(std::strcmp(status.state_name, "NOT_STARTED") == 0, "state name exported");
    CHECK(status.shutdown_called == 0, "shutdown flag false before init");

    CHECK(neo_bootstrap_initialize() == NEO_BOOTSTRAP_OK, "first initialize");
    CHECK(neo_bootstrap_get_status(&status) == NEO_BOOTSTRAP_OK, "status after init");
    CHECK(status.state == NEO_BOOTSTRAP_READY_HOST_PENDING, "host-pending state");
#if defined(ARIZONA_NEO_WITH_LUAJIT)
    CHECK(status.runtime_available == 1, "portable runtime available");
#else
    CHECK(status.runtime_available == 0, "runtime unavailable without LuaJIT");
#endif
    CHECK(status.game_capabilities_available == 0, "no false game capabilities");
    CHECK(std::strcmp(status.bootstrap_version, "1.0.0-stage11") == 0, "bootstrap version in status");
    CHECK(status.core_version[0] != '\0', "core version in status");
    CHECK(status.error[0] == '\0', "no error after successful init");
    CHECK(status.init_duration_ms < 60000, "init duration recorded");

    CHECK(neo_bootstrap_initialize() == NEO_BOOTSTRAP_OK, "repeated initialize is idempotent");
    CHECK(neo_bootstrap_get_status(&status) == NEO_BOOTSTRAP_OK, "status after repeated init");
    CHECK(status.state == NEO_BOOTSTRAP_READY_HOST_PENDING, "state unchanged on repeated init");

    neo_bootstrap_host_interfaces_t interfaces;
    std::memset(&interfaces, 0, sizeof(interfaces));
    CHECK(neo_bootstrap_attach_host(nullptr) == NEO_BOOTSTRAP_INVALID_ARGUMENT,
          "null host interfaces rejected");
    CHECK(neo_bootstrap_attach_host(&interfaces) == NEO_BOOTSTRAP_INVALID_ARGUMENT,
          "null function pointer rejected");
    CHECK(neo_bootstrap_get_status(&status) == NEO_BOOTSTRAP_OK, "status after rejected host");
    CHECK(status.state == NEO_BOOTSTRAP_READY_HOST_PENDING, "state unchanged after rejected host");
    CHECK(status.game_capabilities_available == 0, "still no false game capability");

    // Concurrent status readers and repeated initialize calls must be safe.
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&errors]() {
            for (int j = 0; j < 200; ++j) {
                neo_bootstrap_status_t local;
                if (neo_bootstrap_get_status(&local) != NEO_BOOTSTRAP_OK) ++errors;
                if (neo_bootstrap_initialize() != NEO_BOOTSTRAP_OK) ++errors;
            }
        });
    }
    for (auto& thread : threads) thread.join();
    CHECK(errors.load() == 0, "concurrent C ABI calls are thread-safe");

    CHECK(neo_bootstrap_shutdown() == NEO_BOOTSTRAP_OK, "shutdown");
    CHECK(neo_bootstrap_get_status(&status) == NEO_BOOTSTRAP_OK, "status after shutdown");
    CHECK(status.state == NEO_BOOTSTRAP_STOPPED, "STOPPED state");
    CHECK(status.shutdown_called == 1, "shutdown fact recorded");
    CHECK(status.runtime_available == 0, "runtime released");
    CHECK(status.game_capabilities_available == 0, "game capability cleared");

    CHECK(neo_bootstrap_shutdown() == NEO_BOOTSTRAP_OK, "repeated shutdown is safe");
    CHECK(neo_bootstrap_initialize() == NEO_BOOTSTRAP_OK, "reinitialize after shutdown");
    CHECK(neo_bootstrap_get_status(&status) == NEO_BOOTSTRAP_OK, "status after reinit");
    CHECK(status.state == NEO_BOOTSTRAP_READY_HOST_PENDING, "reinit returns to host pending");
    CHECK(status.shutdown_called == 0, "shutdown flag reset on reinit");
    neo_bootstrap_shutdown();

    if (failures == 0) std::cout << "ios_bootstrap tests: PASSED\n";
    else std::cerr << "ios_bootstrap tests: " << failures << " FAILED\n";
    return failures == 0 ? 0 : 1;
}
