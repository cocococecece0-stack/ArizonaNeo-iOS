#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#include <dispatch/dispatch.h>
#include <os/log.h>

#include "neo_bootstrap.h"
#include "neo_frame_probe.h"
#include "arizona_neo/runtime/frame_probe_state.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kProbeClass = "_TtC7Arizona6GLView";
constexpr const char* kProbeSelector = "tick";
constexpr const char* kExpectedTypes = "v16@0:8";
constexpr int kMaxDiscoveryAttempts = 20;
constexpr int64_t kDiscoveryDelayNs = 100'000'000; // 100 ms

std::atomic<bool> g_started{false};
std::atomic<bool> g_installed{false};
std::atomic<bool> g_failed{false};
std::atomic<bool> g_already_installed_logged{false};
std::atomic<IMP> g_original_tick{nullptr};
arizona_neo::runtime::FrameProbeState g_state;

void marker(const char* event, const char* detail) {
    neo_bootstrap_write_marker(event, detail);
}

void log_install_failure(const char* reason) {
    g_failed.store(true, std::memory_order_release);
    char detail[192];
    std::snprintf(detail, sizeof(detail), "class=%s selector=%s reason=%s",
                  kProbeClass, kProbeSelector, reason != nullptr ? reason : "unknown");
    marker("FRAME_PROBE_INSTALL_FAILED", detail);
    os_log_error(OS_LOG_DEFAULT, "neo_frame_probe install failed: %{public}s",
                 detail);
}

void neo_frame_probe_tick(id self, SEL command) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
    std::uint64_t count = 0;
    if (g_state.should_log(now_ns, count)) {
        // Unified logging is the only per-second sink. No file I/O, no Lua,
        // no allocation and no UIKit calls happen on the render thread.
        os_log_info(OS_LOG_DEFAULT,
                    "neo_frame_probe FRAME_TICK count=%llu timestamp_ns=%lld",
                    static_cast<unsigned long long>(count),
                    static_cast<long long>(now_ns));
    }

    IMP original = g_original_tick.load(std::memory_order_acquire);
    if (original != nullptr) {
        reinterpret_cast<void (*)(id, SEL)>(original)(self, command);
    }
}

// Returns true when discovery is finished (success or permanent failure).
// Returns false only when the class is not registered yet and a bounded retry
// may still succeed.
bool try_install() {
    if (g_installed.load(std::memory_order_acquire)) {
        if (!g_already_installed_logged.exchange(true, std::memory_order_acq_rel)) {
            marker("FRAME_PROBE_ALREADY_INSTALLED", "GLView.tick swizzle already active");
        }
        return true;
    }
    if (g_failed.load(std::memory_order_acquire)) {
        return true;
    }

    Class cls = objc_getClass(kProbeClass);
    if (cls == nullptr) {
        return false;
    }

    SEL selector = sel_registerName(kProbeSelector);
    Method method = class_getInstanceMethod(cls, selector);
    if (method == nullptr) {
        log_install_failure("selector not found");
        return true;
    }

    const char* types = method_getTypeEncoding(method);
    if (types == nullptr || std::strcmp(types, kExpectedTypes) != 0) {
        char reason[96];
        std::snprintf(reason, sizeof(reason), "type encoding mismatch: %s",
                      types != nullptr ? types : "(null)");
        log_install_failure(reason);
        return true;
    }
    if (method_getNumberOfArguments(method) != 2) {
        log_install_failure("unexpected argument count");
        return true;
    }

    IMP original = method_getImplementation(method);
    if (original == nullptr) {
        log_install_failure("original IMP is null");
        return true;
    }

    // Make the original IMP visible before the method starts dispatching to
    // the replacement, so the original behavior is preserved immediately.
    g_original_tick.store(original, std::memory_order_release);
    IMP previous = method_setImplementation(
        method, reinterpret_cast<IMP>(&neo_frame_probe_tick));
    if (previous != original) {
        if (previous != nullptr) {
            method_setImplementation(method, previous);
        }
        g_original_tick.store(nullptr, std::memory_order_release);
        log_install_failure("method_setImplementation raced with another hook");
        return true;
    }

    if (method_getImplementation(method) != reinterpret_cast<IMP>(&neo_frame_probe_tick)) {
        method_setImplementation(method, original);
        g_original_tick.store(nullptr, std::memory_order_release);
        log_install_failure("swizzle read-back failed");
        return true;
    }

    g_installed.store(true, std::memory_order_release);
    char detail[224];
    std::snprintf(detail, sizeof(detail),
                  "class=%s selector=%s types=%s original=%p replacement=%p",
                  kProbeClass, kProbeSelector, types,
                  reinterpret_cast<void*>(original),
                  reinterpret_cast<void*>(&neo_frame_probe_tick));
    marker("FRAME_PROBE_INSTALLED", detail);
    os_log_info(OS_LOG_DEFAULT, "neo_frame_probe installed: %{public}s", detail);
    return true;
}

void schedule_attempt(int attempt) {
    if (g_installed.load(std::memory_order_acquire) ||
        g_failed.load(std::memory_order_acquire)) {
        return;
    }
    if (try_install()) {
        return;
    }
    if (attempt >= kMaxDiscoveryAttempts) {
        log_install_failure("class not registered after bounded retries");
        return;
    }
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW, kDiscoveryDelayNs),
        dispatch_get_main_queue(), ^{
            schedule_attempt(attempt + 1);
        });
}

void probe_start_callback(void*) {
    neo_ios_frame_probe_start();
}

} // namespace

extern "C" void neo_ios_frame_probe_start(void) {
    if (g_started.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    marker("FRAME_PROBE_START", "bounded main-queue discovery scheduled");
    dispatch_async(dispatch_get_main_queue(), ^{
        schedule_attempt(0);
    });
}

extern "C" void neo_ios_frame_probe_schedule_start(void) {
    dispatch_async_f(dispatch_get_main_queue(), nullptr, &probe_start_callback);
}

extern "C" int neo_ios_frame_probe_is_installed(void) {
    return g_installed.load(std::memory_order_acquire) ? 1 : 0;
}

extern "C" uint64_t neo_ios_frame_probe_tick_count(void) {
    return g_state.count();
}
