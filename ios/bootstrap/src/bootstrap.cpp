#include "neo_bootstrap.h"

#include "arizona_neo/core/event_bus.hpp"
#include "arizona_neo/core/filesystem.hpp"
#include "arizona_neo/core/scheduler.hpp"
#include "arizona_neo/core/script_manager.hpp"
#include "arizona_neo/core/version.hpp"
#include "arizona_neo/runtime/lua_jit_host.hpp"
#if defined(ARIZONA_NEO_WITH_LUAJIT)
#include "arizona_neo/runtime/neo_loader_host.hpp"
#endif

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>

#if defined(__APPLE__)
#include <os/log.h>
#endif

namespace {

constexpr const char* kBootstrapVersion = "1.0.0-stage13.1";
constexpr const char* kDiagnosticFile = "Library/Caches/neo_bootstrap.marker";

std::atomic<bool> g_library_loaded{false};

// Writes a small diagnostic record inside the application container. All work
// is C-only and allocation-free so it is safe from a dyld initializer.
void write_diagnostic(const char* event, const char* detail) {
    const char* home = std::getenv("HOME");
    char path[512];
    if (home == nullptr || home[0] == '\0') {
        std::snprintf(path, sizeof(path), "/tmp/neo_bootstrap.marker");
    } else {
        std::snprintf(path, sizeof(path), "%s/%s", home, kDiagnosticFile);
    }
    FILE* file = std::fopen(path, "a");
    if (file == nullptr) return;
    std::fprintf(file, "neo_bootstrap event=%s detail=%s version=%s\n",
                 event != nullptr ? event : "unknown",
                 detail != nullptr ? detail : "none",
                 kBootstrapVersion);
    std::fclose(file);
}

void diagnostic_log(const char* event) {
#if defined(__APPLE__)
    os_log_info(OS_LOG_DEFAULT, "neo_bootstrap: %{public}s", event);
#else
    (void)event;
#endif
}

__attribute__((constructor)) __attribute__((used))
void neo_bootstrap_library_loaded() {
    // Only record the fact that dyld executed this library's initializer.
    // No LuaJIT, Core or host objects are created here.
    g_library_loaded.store(true);
    write_diagnostic("DYLIB_LOADED", "constructor");
    diagnostic_log("DYLIB_LOADED");
}

const char* state_name(neo_bootstrap_state_t state) {
    switch (state) {
        case NEO_BOOTSTRAP_NOT_STARTED: return "NOT_STARTED";
        case NEO_BOOTSTRAP_INITIALIZING: return "INITIALIZING";
        case NEO_BOOTSTRAP_READY_HOST_PENDING: return "BOOTSTRAP_READY_HOST_PENDING";
        case NEO_BOOTSTRAP_RUNTIME_READY: return "RUNTIME_READY";
        case NEO_BOOTSTRAP_FAILED: return "FAILED";
        case NEO_BOOTSTRAP_STOPPED: return "STOPPED";
    }
    return "UNKNOWN";
}

void copy_text(char* destination, std::size_t size, const char* source) {
    if (size == 0) return;
    if (source == nullptr) {
        destination[0] = '\0';
        return;
    }
    std::strncpy(destination, source, size - 1);
    destination[size - 1] = '\0';
}

struct BootstrapState {
    std::mutex mutex;
    neo_bootstrap_state_t state = NEO_BOOTSTRAP_NOT_STARTED;
    std::string error;
    unsigned long long init_duration_ms = 0;
    bool shutdown_called = false;
    bool runtime_available = false;
    bool game_capabilities_available = false;
    bool library_loaded = false;

    std::unique_ptr<arizona_neo::core::EventBus> bus;
    std::unique_ptr<arizona_neo::core::Scheduler> scheduler;
    std::unique_ptr<arizona_neo::core::ScriptManager> manager;
    std::unique_ptr<arizona_neo::core::SandboxedFilesystem> fs;
    std::unique_ptr<arizona_neo::runtime::LuaJITHost> host;
#if defined(ARIZONA_NEO_WITH_LUAJIT)
    std::unique_ptr<arizona_neo::runtime::NeoLoaderHost> neo_host;
#endif

    void reset_runtime() {
#if defined(ARIZONA_NEO_WITH_LUAJIT)
        neo_host.reset();
#endif
        host.reset();
        fs.reset();
        manager.reset();
        scheduler.reset();
        bus.reset();
    }
};

BootstrapState& state() {
    static BootstrapState instance;
    return instance;
}

int fail(BootstrapState& bootstrap, const char* message) {
    bootstrap.state = NEO_BOOTSTRAP_FAILED;
    bootstrap.error = message != nullptr ? message : "unknown error";
    write_diagnostic("FAILED", bootstrap.error.c_str());
    diagnostic_log("FAILED");
    bootstrap.reset_runtime();
    return NEO_BOOTSTRAP_INTERNAL_ERROR;
}

} // namespace

extern "C" {

NEO_BOOTSTRAP_API int neo_bootstrap_initialize(void) {
    auto& bootstrap = state();
    std::lock_guard<std::mutex> lock(bootstrap.mutex);
    try {
        if (bootstrap.state == NEO_BOOTSTRAP_READY_HOST_PENDING ||
            bootstrap.state == NEO_BOOTSTRAP_RUNTIME_READY) {
            return NEO_BOOTSTRAP_OK; // idempotent
        }
        if (bootstrap.state == NEO_BOOTSTRAP_INITIALIZING) {
            return NEO_BOOTSTRAP_INVALID_STATE;
        }

        const auto started = std::chrono::steady_clock::now();
        bootstrap.state = NEO_BOOTSTRAP_INITIALIZING;
        write_diagnostic("BOOTSTRAP_INITIALIZING", "neo_bootstrap_initialize");
        diagnostic_log("BOOTSTRAP_INITIALIZING");
        bootstrap.error.clear();
        bootstrap.shutdown_called = false;
        bootstrap.game_capabilities_available = false;
        bootstrap.library_loaded = g_library_loaded;

        bootstrap.bus = std::make_unique<arizona_neo::core::EventBus>();
        bootstrap.scheduler = std::make_unique<arizona_neo::core::Scheduler>();
        bootstrap.manager = std::make_unique<arizona_neo::core::ScriptManager>();

        const auto root = std::filesystem::temp_directory_path() / "arizona_neo_bootstrap";
        bootstrap.fs = std::make_unique<arizona_neo::core::SandboxedFilesystem>(root, true);

#if defined(ARIZONA_NEO_WITH_LUAJIT)
        bootstrap.host = std::make_unique<arizona_neo::runtime::LuaJITHost>();
        if (!bootstrap.host->initialize()) {
            return fail(bootstrap, bootstrap.host->last_error().c_str());
        }
        bootstrap.neo_host = std::make_unique<arizona_neo::runtime::NeoLoaderHost>(
            *bootstrap.host, *bootstrap.bus, *bootstrap.scheduler,
            *bootstrap.manager, *bootstrap.fs);
        bootstrap.runtime_available = true;
#else
        bootstrap.runtime_available = false;
#endif

        bootstrap.state = NEO_BOOTSTRAP_READY_HOST_PENDING;
        write_diagnostic("HOST_PENDING", "runtime available, game capabilities unavailable");
        diagnostic_log("HOST_PENDING");
        const auto elapsed = std::chrono::steady_clock::now() - started;
        bootstrap.init_duration_ms = static_cast<unsigned long long>(
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
        return NEO_BOOTSTRAP_OK;
    } catch (const std::exception& error) {
        return fail(bootstrap, error.what());
    } catch (...) {
        return fail(bootstrap, "unknown C++ exception in neo_bootstrap_initialize");
    }
}

NEO_BOOTSTRAP_API int neo_bootstrap_shutdown(void) {
    auto& bootstrap = state();
    std::lock_guard<std::mutex> lock(bootstrap.mutex);
    try {
        if (bootstrap.state == NEO_BOOTSTRAP_NOT_STARTED ||
            bootstrap.state == NEO_BOOTSTRAP_STOPPED) {
            return NEO_BOOTSTRAP_OK;
        }
        bootstrap.shutdown_called = true;
        write_diagnostic("SHUTDOWN", "neo_bootstrap_shutdown");
        diagnostic_log("SHUTDOWN");
        bootstrap.game_capabilities_available = false;
        bootstrap.runtime_available = false;
        bootstrap.reset_runtime();
        bootstrap.state = NEO_BOOTSTRAP_STOPPED;
        return NEO_BOOTSTRAP_OK;
    } catch (...) {
        bootstrap.state = NEO_BOOTSTRAP_FAILED;
        bootstrap.error = "unknown C++ exception in neo_bootstrap_shutdown";
        return NEO_BOOTSTRAP_INTERNAL_ERROR;
    }
}

NEO_BOOTSTRAP_API int neo_bootstrap_get_status(neo_bootstrap_status_t* out_status) {
    if (out_status == nullptr) return NEO_BOOTSTRAP_INVALID_ARGUMENT;
    auto& bootstrap = state();
    std::lock_guard<std::mutex> lock(bootstrap.mutex);
    try {
        std::memset(out_status, 0, sizeof(*out_status));
        out_status->state = bootstrap.state;
        copy_text(out_status->state_name, sizeof(out_status->state_name), state_name(bootstrap.state));
        copy_text(out_status->bootstrap_version, sizeof(out_status->bootstrap_version), kBootstrapVersion);
        copy_text(out_status->core_version, sizeof(out_status->core_version),
                  std::string(arizona_neo::core::version()).c_str());
        copy_text(out_status->error, sizeof(out_status->error), bootstrap.error.c_str());
        out_status->init_duration_ms = bootstrap.init_duration_ms;
        out_status->shutdown_called = bootstrap.shutdown_called ? 1 : 0;
        out_status->runtime_available = bootstrap.runtime_available ? 1 : 0;
        out_status->game_capabilities_available = bootstrap.game_capabilities_available ? 1 : 0;
        out_status->dylib_loaded = g_library_loaded.load() ? 1 : 0;
        out_status->bootstrap_initializing = bootstrap.state == NEO_BOOTSTRAP_INITIALIZING ? 1 : 0;
        out_status->bootstrap_initialized =
            (bootstrap.state == NEO_BOOTSTRAP_READY_HOST_PENDING ||
             bootstrap.state == NEO_BOOTSTRAP_RUNTIME_READY) ? 1 : 0;
        out_status->host_pending = bootstrap.state == NEO_BOOTSTRAP_READY_HOST_PENDING ? 1 : 0;
        out_status->runtime_ready_flag = bootstrap.state == NEO_BOOTSTRAP_RUNTIME_READY ? 1 : 0;
        out_status->failed_flag = bootstrap.state == NEO_BOOTSTRAP_FAILED ? 1 : 0;
        out_status->stopped_flag = bootstrap.state == NEO_BOOTSTRAP_STOPPED ? 1 : 0;
        return NEO_BOOTSTRAP_OK;
    } catch (...) {
        return NEO_BOOTSTRAP_INTERNAL_ERROR;
    }
}

NEO_BOOTSTRAP_API const char* neo_bootstrap_get_version(void) {
    return kBootstrapVersion;
}

NEO_BOOTSTRAP_API int neo_bootstrap_get_marker_path(char* out_path, unsigned long long capacity) {
    if (out_path == nullptr || capacity == 0) return NEO_BOOTSTRAP_INVALID_ARGUMENT;
    const char* home = std::getenv("HOME");
    int written = 0;
    if (home == nullptr || home[0] == '\0') {
        written = std::snprintf(out_path, static_cast<std::size_t>(capacity),
                                "/tmp/neo_bootstrap.marker");
    } else {
        written = std::snprintf(out_path, static_cast<std::size_t>(capacity),
                                "%s/%s", home, kDiagnosticFile);
    }
    if (written < 0 || static_cast<unsigned long long>(written) >= capacity) {
        return NEO_BOOTSTRAP_INVALID_ARGUMENT;
    }
    return NEO_BOOTSTRAP_OK;
}

NEO_BOOTSTRAP_API int neo_bootstrap_attach_host(const neo_bootstrap_host_interfaces_t* interfaces) {
    if (interfaces == nullptr || interfaces->graphics_failure_set_handler == nullptr) {
        return NEO_BOOTSTRAP_INVALID_ARGUMENT;
    }
    auto& bootstrap = state();
    std::lock_guard<std::mutex> lock(bootstrap.mutex);
    if (bootstrap.state != NEO_BOOTSTRAP_READY_HOST_PENDING &&
        bootstrap.state != NEO_BOOTSTRAP_RUNTIME_READY) {
        return NEO_BOOTSTRAP_INVALID_STATE;
    }
#if defined(ARIZONA_NEO_WITH_LUAJIT)
    try {
        arizona_neo::runtime::NeoLoaderHost::NativeInterfaces native;
        native.graphics_failure_set_handler = interfaces->graphics_failure_set_handler;
        if (!bootstrap.neo_host || !bootstrap.neo_host->start(native)) {
            bootstrap.error = "native host interface rejected by NeoLoaderHost";
            return NEO_BOOTSTRAP_INVALID_STATE;
        }
        bootstrap.game_capabilities_available = true;
        write_diagnostic("RUNTIME_READY", "confirmed NativeInterfaces attached");
        diagnostic_log("RUNTIME_READY");
        bootstrap.state = NEO_BOOTSTRAP_RUNTIME_READY;
        return NEO_BOOTSTRAP_OK;
    } catch (const std::exception& error) {
        bootstrap.error = error.what();
        bootstrap.state = NEO_BOOTSTRAP_FAILED;
        return NEO_BOOTSTRAP_INTERNAL_ERROR;
    } catch (...) {
        bootstrap.error = "unknown C++ exception in neo_bootstrap_attach_host";
        bootstrap.state = NEO_BOOTSTRAP_FAILED;
        return NEO_BOOTSTRAP_INTERNAL_ERROR;
    }
#else
    bootstrap.error = "bootstrap built without LuaJIT runtime";
    return NEO_BOOTSTRAP_INVALID_STATE;
#endif
}

} // extern "C"
