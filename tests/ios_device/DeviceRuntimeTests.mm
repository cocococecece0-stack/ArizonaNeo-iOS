#import "DeviceRuntimeTests.h"

#import <Foundation/Foundation.h>

#include "arizona_neo/runtime/lua_jit_host.hpp"
#include "arizona_neo/runtime/lua_runtime_contract.hpp"
#include "arizona_neo/runtime/portable_script_loader.hpp"
#include "arizona_neo/core/script_manager.hpp"
#include "arizona_neo/runtime/portable_script_runtime.hpp"
#include "NeoSha256Script.h"

#include <chrono>
#include <cstdarg>
#include <exception>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>

using arizona_neo::core::SandboxedFilesystem;
using arizona_neo::runtime::LuaExecutionStatus;
using arizona_neo::runtime::LuaJITHost;
using arizona_neo::runtime::PortableScriptLoader;
using arizona_neo::core::ScriptManager;
using arizona_neo::core::ScriptMetadata;
using arizona_neo::runtime::PortableScriptRuntime;

static NSString *ANDString(const std::string &value) {
    return [NSString stringWithUTF8String:value.c_str()];
}

static void ANDSetMessage(std::string &out, NSString *format, ...) {
    va_list args;
    va_start(args, format);
    NSString *formatted = [[NSString alloc] initWithFormat:format arguments:args];
    va_end(args);
    out = std::string([formatted UTF8String]);
}

@implementation ANDTestResult

- (NSString *)statusString {
    switch (self.status) {
        case ANDTestStatusPass: return @"PASS";
        case ANDTestStatusFail: return @"FAIL";
        case ANDTestStatusBlocked: return @"BLOCKED";
    }
    return @"BLOCKED";
}

- (NSDictionary<NSString *, id> *)reportDictionary {
    return @{
        @"name": self.name,
        @"status": [self statusString],
        @"message": self.message,
        @"duration_ms": @(self.durationMs)
    };
}

@end

@interface DeviceRuntimeTests ()

@property (nonatomic, strong) NSMutableArray<ANDTestResult *> *mutableResults;
@property (nonatomic, copy) NSString *internalOverall;
@property (nonatomic, copy) NSString *internalJit;
@property (nonatomic, copy) NSString *internalFfi;

@end

@implementation DeviceRuntimeTests

- (instancetype)init {
    self = [super init];
    if (self) {
        _mutableResults = [NSMutableArray array];
        _internalOverall = @"BLOCKED";
        _internalJit = @"unknown";
        _internalFfi = @"unknown";
    }
    return self;
}

- (NSArray<ANDTestResult *> *)results {
    return [self.mutableResults copy];
}

- (NSString *)overallStatus {
    return self.internalOverall;
}

- (NSString *)jitStatus {
    return self.internalJit;
}

- (NSString *)ffiStatus {
    return self.internalFfi;
}

- (void)addResult:(NSString *)name
           status:(ANDTestStatus)status
          message:(NSString *)message
         duration:(double)durationMs {
    ANDTestResult *result = [ANDTestResult new];
    result.name = name;
    result.status = status;
    result.message = message;
    result.durationMs = durationMs;
    [self.mutableResults addObject:result];
}

- (void)runAllTests {
    [self.mutableResults removeAllObjects];
    self.internalJit = @"unknown";
    self.internalFfi = @"unknown";

    using Clock = std::chrono::steady_clock;
    auto record = [&](const char *name, std::function<bool(std::string &)> body) {
        const auto start = Clock::now();
        std::string message;
        bool ok = false;
        try {
            ok = body(message);
        } catch (const std::exception &error) {
            ok = false;
            ANDSetMessage(message, @"C++ exception: %s", error.what());
        } catch (...) {
            ok = false;
            message = "unknown C++ exception";
        }
        const double ms = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        [self addResult:ANDString(name)
                 status:(ok ? ANDTestStatusPass : ANDTestStatusFail)
                message:(ok ? @"ok" : ANDString(message))
               duration:ms];
    };

    auto blocked = [&](const char *name, const char *reason) {
        [self addResult:ANDString(name)
                 status:ANDTestStatusBlocked
                message:ANDString(reason)
               duration:0.0];
    };

    LuaJITHost host;
    bool usable = false;

    // 1-3. Initialization lifecycle.
    record("01_initialize", [&](std::string &message) -> bool {
        const bool ok = host.initialize();
        if (!ok) ANDSetMessage(message, @"initialize() returned false: %s", host.last_error().c_str());
        return ok;
    });
    record("02_is_initialized", [&](std::string &message) -> bool {
        const bool ok = host.is_initialized();
        if (!ok) message = "is_initialized() is false after initialize()";
        return ok;
    });
    record("03_repeated_initialize", [&](std::string &message) -> bool {
        const bool ok = host.initialize();
        if (!ok) ANDSetMessage(message, @"repeated initialize() failed: %s", host.last_error().c_str());
        return ok;
    });
    usable = host.is_initialized();

    auto stateTest = [&](const char *name, std::function<bool(std::string &)> body) {
        if (!usable) {
            blocked(name, "LuaJITHost is not initialized; state-dependent test cannot run");
        } else {
            record(name, body);
        }
    };

    // 4-18. Lua execution, globals, standard libraries, FFI, error recovery.
    stateTest("04_lua_arithmetic", [&](std::string &message) -> bool {
        const bool ok = host.execute(
            "local result = 20 + 22\n"
            "assert(result == 42)\n"
            "device_arithmetic = result", "arithmetic");
        if (!ok) ANDSetMessage(message, @"execute failed: %s", host.last_error().c_str());
        if (ok && host.get_global_number("device_arithmetic") != 42.0) {
            message = "global device_arithmetic is not 42";
            return false;
        }
        return ok;
    });

    stateTest("05_number_global", [&](std::string &message) -> bool {
        const bool ok = host.execute("device_number = 123", "number_global");
        if (!ok) ANDSetMessage(message, @"execute failed: %s", host.last_error().c_str());
        if (ok && host.get_global_number("device_number") != 123.0) {
            message = "get_global_number(device_number) is not 123";
            return false;
        }
        return ok;
    });

    stateTest("06_string_global", [&](std::string &message) -> bool {
        const bool ok = host.execute("device_string = 'ArizonaNeo'", "string_global");
        if (!ok) ANDSetMessage(message, @"execute failed: %s", host.last_error().c_str());
        if (ok && host.get_global_string("device_string") != "ArizonaNeo") {
            message = "get_global_string(device_string) is not ArizonaNeo";
            return false;
        }
        return ok;
    });

    stateTest("07_missing_globals", [&](std::string &message) -> bool {
        const double missingNumber = host.get_global_number("__device_missing_global__");
        const std::string missingString = host.get_global_string("__device_missing_global__");
        if (missingNumber != 0.0 || !missingString.empty()) {
            message = "missing global did not return 0.0 / empty string";
            return false;
        }
        if (host.get_global_number(nullptr) != 0.0 || !host.get_global_string(nullptr).empty()) {
            message = "nullptr global name did not return 0.0 / empty string";
            return false;
        }
        return true;
    });

    stateTest("08_standard_libraries", [&](std::string &message) -> bool {
        const bool ok = host.execute(
            "assert(type(print) == 'function')\n"
            "assert(math.floor(4.2) == 4)\n"
            "assert(('abc'):upper() == 'ABC')\n"
            "assert(table.concat({1, 2}, ',') == '1,2')\n"
            "assert(type(require) == 'function')\n"
            "libs_available = 1", "standard_libraries");
        if (!ok) ANDSetMessage(message, @"library check failed: %s", host.last_error().c_str());
        if (ok && host.get_global_number("libs_available") != 1.0) {
            message = "standard library assertion script did not complete";
            return false;
        }
        return ok;
    });

    stateTest("09_jit_status_disabled", [&](std::string &message) -> bool {
        const bool ok = host.execute(
            "local j = require('jit')\n"
            "jit_status_value = j.status() and 1 or 0\n"
            "jit_status_count = select('#', j.status())", "jit_status");
        if (!ok) {
            ANDSetMessage(message, @"jit.status() script failed: %s", host.last_error().c_str());
            return false;
        }
        const double status = host.get_global_number("jit_status_value");
        const double count = host.get_global_number("jit_status_count");
        self.internalJit = (status == 0.0 && count == 1.0) ? @"disabled" : @"enabled_or_unexpected";
        if (status != 0.0 || count != 1.0) {
            ANDSetMessage(message, @"expected JIT disabled (status=0, count=1), got %.0f/%.0f", status, count);
            return false;
        }
        return true;
    });

    stateTest("10_ffi_available", [&](std::string &message) -> bool {
        const bool ok = host.execute("ffi_available = require('ffi') and 1 or 0", "ffi_available");
        if (!ok) ANDSetMessage(message, @"require('ffi') failed: %s", host.last_error().c_str());
        if (ok && host.get_global_number("ffi_available") == 1.0) {
            self.internalFfi = @"enabled";
        }
        return ok && host.get_global_number("ffi_available") == 1.0;
    });

    stateTest("11_ffi_cdef_struct", [&](std::string &message) -> bool {
        const bool ok = host.execute(
            "local ffi = require('ffi')\n"
            "ffi.cdef[[\n"
            "    typedef struct { double x; double y; } NeoTestPoint;\n"
            "]]\n"
            "ffi_point = ffi.new('NeoTestPoint')\n"
            "ffi_point.x = 12\n"
            "ffi_point.y = 30\n"
            "ffi_x = ffi_point.x\n"
            "ffi_y = ffi_point.y", "ffi_struct");
        if (!ok) ANDSetMessage(message, @"FFI cdef/new failed: %s", host.last_error().c_str());
        return ok;
    });

    stateTest("12_ffi_initial_fields", [&](std::string &message) -> bool {
        const double x = host.get_global_number("ffi_x");
        const double y = host.get_global_number("ffi_y");
        if (x != 12.0 || y != 30.0) {
            ANDSetMessage(message, @"expected NeoTestPoint {12, 30}, got {%.0f, %.0f}", x, y);
            return false;
        }
        return true;
    });

    stateTest("13_ffi_modify_fields", [&](std::string &message) -> bool {
        const bool ok = host.execute(
            "ffi_point.x = 7.5\n"
            "ffi_point.y = 34.5\n"
            "ffi_sum = ffi_point.x + ffi_point.y", "ffi_modify");
        if (!ok) ANDSetMessage(message, @"FFI field write failed: %s", host.last_error().c_str());
        if (ok && host.get_global_number("ffi_sum") != 42.0) {
            message = "FFI struct fields did not produce 42";
            return false;
        }
        return ok;
    });

    stateTest("14_runtime_error", [&](std::string &message) -> bool {
        const bool ok = host.execute("error('device boom')", "runtime_error");
        if (ok) message = "execute() returned true for a Lua runtime error";
        return !ok;
    });

    stateTest("15_last_error_text", [&](std::string &message) -> bool {
        const std::string last = host.last_error();
        if (last.find("device boom") == std::string::npos) {
            ANDSetMessage(message, @"last_error() does not contain the Lua error text: %s", last.c_str());
            return false;
        }
        return true;
    });

    stateTest("16_execute_after_error", [&](std::string &message) -> bool {
        const bool ok = host.execute("after_error = 7", "after_error");
        if (!ok) {
            ANDSetMessage(message, @"execute after error failed: %s", host.last_error().c_str());
            return false;
        }
        if (host.get_global_number("after_error") != 7.0) {
            message = "Lua state did not recover after runtime error";
            return false;
        }
        if (!host.last_error().empty()) {
            message = "last_error() was not cleared after a successful execute";
            return false;
        }
        return true;
    });

    stateTest("17_sequential_executes", [&](std::string &message) -> bool {
        bool ok = host.execute("seq_a = 1", "seq_1");
        if (ok) ok = host.execute("seq_b = seq_a + 1", "seq_2");
        if (!ok) {
            ANDSetMessage(message, @"sequential execute failed: %s", host.last_error().c_str());
            return false;
        }
        if (host.get_global_number("seq_b") != 2.0) {
            message = "Lua state was not shared across sequential executes";
            return false;
        }
        return true;
    });

    stateTest("18_stack_hygiene", [&](std::string &message) -> bool {
        const int top = host.stack_top();
        if (top != 0) {
            ANDSetMessage(message, @"stack_top() is %d, expected 0", top);
            return false;
        }
        return true;
    });

    // 19-24. Shutdown and re-initialization are safe even if init failed.
    record("19_shutdown", [&](std::string &message) -> bool {
        host.shutdown();
        if (host.is_initialized()) {
            message = "is_initialized() is true after shutdown()";
            return false;
        }
        return true;
    });

    record("20_repeated_shutdown", [&](std::string &message) -> bool {
        host.shutdown();
        if (host.is_initialized()) {
            message = "is_initialized() is true after a repeated shutdown()";
            return false;
        }
        return true;
    });

    record("21_execute_after_shutdown", [&](std::string &message) -> bool {
        const bool ok = host.execute("return 1", "after_shutdown");
        if (ok) message = "execute() succeeded after shutdown()";
        if (host.last_error().empty()) message = "last_error() is empty after execute() post-shutdown";
        return !ok;
    });

    record("22_reinitialize", [&](std::string &message) -> bool {
        const bool ok = host.initialize();
        if (!ok) ANDSetMessage(message, @"re-initialize failed: %s", host.last_error().c_str());
        return ok;
    });

    record("23_execute_after_reinit", [&](std::string &message) -> bool {
        const bool ok = host.execute("reinit_value = 99", "reinit");
        if (!ok) {
            ANDSetMessage(message, @"execute after re-init failed: %s", host.last_error().c_str());
            return false;
        }
        if (host.get_global_number("reinit_value") != 99.0) {
            message = "re-initialized runtime did not execute Lua correctly";
            return false;
        }
        if (!host.get_global_string("device_string").empty()) {
            message = "re-initialized runtime retained globals from the previous state";
            return false;
        }
        return true;
    });

    record("24_final_shutdown", [&](std::string &message) -> bool {
        host.shutdown();
        if (host.is_initialized() || host.stack_top() != 0) {
            message = "final shutdown left the runtime initialized or the stack dirty";
            return false;
        }
        return true;
    });

    // Stage 7.1.6 A-E: portable runtime bridge and structured result.
    {
        LuaJITHost portable;
        if (!portable.initialize()) {
            blocked("25_library_registration", "portable runtime did not initialize");
            blocked("26_bridge_calls", "portable runtime did not initialize");
            blocked("27_bridge_exception", "portable runtime did not initialize");
            blocked("28_bridge_duplicate", "portable runtime did not initialize");
            blocked("29_syntax_error_status", "portable runtime did not initialize");
            blocked("30_result_contract", "portable runtime did not initialize");
        } else {
            stateTest("25_library_registration", [&](std::string &message) -> bool {
                const bool ok = portable.execute(
                    "assert(type(print) == 'function')\n"
                    "assert(type(coroutine) ~= 'nil')\n"
                    "assert(type(math) ~= 'nil')\n"
                    "assert(type(string) ~= 'nil')\n"
                    "assert(type(table) ~= 'nil')\n"
                    "assert(type(package) ~= 'nil')\n"
                    "assert(require('ffi') ~= nil)\n"
                    "assert(type(jit) ~= 'nil')\n"
                    "assert(io == nil and os == nil and debug == nil and bit == nil)\n"
                    "portable_libs = 1", "portable_libraries");
                if (!ok) {
                    message = "library policy script failed: " + portable.last_error();
                    return false;
                }
                return portable.get_global_number("portable_libs") == 1.0;
            });

            record("26_bridge_calls", [&](std::string &message) -> bool {
                const bool ok = portable.execute(
                    "bridge_version = runtime.version()\n"
                    "bridge_ping = runtime.ping()\n"
                    "bridge_echo = runtime.echo(42)", "bridge_calls");
                if (!ok) {
                    message = "Lua-to-C++ bridge calls failed: " + portable.last_error();
                    return false;
                }
                const std::string version = portable.get_global_string("bridge_version");
                if (version.find("LuaJIT 2.1") == std::string::npos ||
                    portable.get_global_string("bridge_ping") != "pong" ||
                    portable.get_global_number("bridge_echo") != 42.0) {
                    message = "bridge results are incorrect";
                    return false;
                }
                return portable.stack_top() == 0;
            });

            record("27_bridge_exception", [&](std::string &message) -> bool {
                if (!portable.register_function("runtime.throws", [](lua_State*) -> int {
                        throw std::runtime_error("bridge failure");
                    })) {
                    message = "could not register bridge exception function";
                    return false;
                }
                const auto result = portable.execute_detailed("runtime.throws()", "bridge_exception");
                if (result.status != LuaExecutionStatus::RuntimeError ||
                    result.message.find("bridge failure") == std::string::npos || result.stack_top_after != 0) {
                    message = "C++ exception was not converted into a safe Lua runtime error";
                    return false;
                }
                return true;
            });

            record("28_bridge_duplicate", [&](std::string &message) -> bool {
                if (portable.register_function("runtime.ping", [](lua_State*) -> int { return 0; })) {
                    message = "duplicate bridge registration was accepted";
                    return false;
                }
                return portable.execute("dup_probe = runtime.ping()", "duplicate_probe");
            });

            record("29_syntax_error_status", [&](std::string &message) -> bool {
                const auto result = portable.execute_detailed("return 2 +", "syntax_status");
                if (result.status != LuaExecutionStatus::SyntaxError || result.message.empty() ||
                    result.stack_top_after != 0) {
                    message = "syntax error was not classified or stack was not restored";
                    return false;
                }
                return true;
            });

            record("30_result_contract", [&](std::string &message) -> bool {
                const auto success = portable.execute_detailed("result_contract = 1", "result_success");
                const auto runtime = portable.execute_detailed("error('result boom')", "result_runtime");
                if (success.status != LuaExecutionStatus::Success || !success.message.empty() ||
                    success.stack_top_after != 0 ||
                    runtime.status != LuaExecutionStatus::RuntimeError ||
                    runtime.message.find("result boom") == std::string::npos ||
                    runtime.stack_top_after != success.stack_top_after) {
                    message = "structured execution result contract violated";
                    return false;
                }
                return true;
            });

            portable.shutdown();
        }
    }

    // Stage 7.2.1: portable script loading from an app-owned sandbox directory.
    {
        NSString *documents = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory,
                                                                   NSUserDomainMask, YES).firstObject;
        if (documents == nil) {
            blocked("31_portable_script_load", "Documents directory unavailable");
            blocked("32_portable_invalid_path", "Documents directory unavailable");
            blocked("33_portable_unload_reload", "Documents directory unavailable");
            blocked("34_portable_disabled_libs", "Documents directory unavailable");
            blocked("35_sm_load_run", "Documents directory unavailable");
            blocked("36_sm_multiple", "Documents directory unavailable");
            blocked("37_sm_reload", "Documents directory unavailable");
            blocked("38_sm_stop", "Documents directory unavailable");
            blocked("39_sm_unknown", "Documents directory unavailable");
            blocked("40_sm_invalid_path", "Documents directory unavailable");
            blocked("41_sm_stack", "Documents directory unavailable");
            blocked("42_sm_shutdown_reinit", "Documents directory unavailable");
            blocked("43_neo_script_execute", "Documents directory unavailable");
        } else {
            const std::filesystem::path root([[documents stringByAppendingPathComponent:
                                               @"ArizonaNeoDeviceTest/scripts"] UTF8String]);
            record("31_portable_script_load", [&](std::string &message) -> bool {
                LuaJITHost script_host;
                if (!script_host.initialize()) {
                    message = "script host did not initialize";
                    return false;
                }
                SandboxedFilesystem fs(root, true);
                PortableScriptLoader loader(script_host, fs);
                if (!fs.write_file("device.lua", "device_script_ok = 42\n").ok) {
                    message = "cannot write local script";
                    return false;
                }
                if (!loader.load("device", "device.lua").ok ||
                    !loader.execute("device").ok) {
                    message = "local script load/execute failed: " + loader.last_error();
                    return false;
                }
                if (script_host.get_global_number("device_script_ok") != 42.0 ||
                    script_host.stack_top() != 0) {
                    message = "script result or stack hygiene is wrong";
                    return false;
                }
                return true;
            });

            record("32_portable_invalid_path", [&](std::string &message) -> bool {
                LuaJITHost script_host;
                if (!script_host.initialize()) {
                    message = "script host did not initialize";
                    return false;
                }
                SandboxedFilesystem fs(root, true);
                PortableScriptLoader loader(script_host, fs);
                const auto result = loader.load("escape", "../outside.lua");
                if (result.ok) {
                    message = "path traversal was accepted";
                    return false;
                }
                return true;
            });

            record("33_portable_unload_reload", [&](std::string &message) -> bool {
                LuaJITHost script_host;
                if (!script_host.initialize()) {
                    message = "script host did not initialize";
                    return false;
                }
                SandboxedFilesystem fs(root, true);
                PortableScriptLoader loader(script_host, fs);
                if (!fs.write_file("reload.lua", "reload_ok = 1\n").ok ||
                    !loader.load("reload", "reload.lua").ok) {
                    message = "initial load failed";
                    return false;
                }
                if (!loader.unload("reload").ok) {
                    message = "unload failed";
                    return false;
                }
                if (loader.is_active("reload")) {
                    message = "unloaded script still active";
                    return false;
                }
                if (!loader.load("reload", "reload.lua").ok ||
                    !loader.reload("reload").ok) {
                    message = "reload failed: " + loader.last_error();
                    return false;
                }
                return script_host.stack_top() == 0;
            });

            record("34_portable_disabled_libs", [&](std::string &message) -> bool {
                LuaJITHost script_host;
                if (!script_host.initialize()) {
                    message = "script host did not initialize";
                    return false;
                }
                SandboxedFilesystem fs(root, true);
                PortableScriptLoader loader(script_host, fs);
                if (!fs.write_file("no_libs_device.lua",
                        "assert(io == nil and os == nil and debug == nil and bit == nil)\n"
                        "device_no_libs = 1\n").ok) {
                    message = "cannot write no-libs script";
                    return false;
                }
                if (!loader.load("no_libs_device", "no_libs_device.lua").ok) {
                    message = "no-libs script failed: " + loader.last_error();
                    return false;
                }
                return script_host.get_global_number("device_no_libs") == 1.0;
            });

            // Stage 7.2.3: ScriptManager integration over PortableScriptLoader.
            record("35_sm_load_run", [&](std::string &message) -> bool {
                LuaJITHost script_host;
                if (!script_host.initialize()) { message = "host init failed"; return false; }
                SandboxedFilesystem fs(root, true);
                PortableScriptLoader loader(script_host, fs);
                PortableScriptRuntime runtime(loader);
                ScriptManager manager;
                manager.set_runtime(&runtime);
                if (!fs.write_file("sm_valid.lua", "sm_valid_ok = 42\n").ok) { message = "write failed"; return false; }
                ScriptMetadata meta;
                meta.id = "sm_valid";
                meta.path = "sm_valid.lua";
                if (!manager.register_script(meta).ok) { message = "register failed"; return false; }
                if (!manager.load("sm_valid").ok) { message = "load failed: " + manager.last_error(); return false; }
                if (!manager.run("sm_valid").ok) { message = "run failed: " + manager.last_error(); return false; }
                if (script_host.get_global_number("sm_valid_ok") != 42.0) { message = "result wrong"; return false; }
                if (script_host.stack_top() != 0) { message = "stack not clean"; return false; }
                return true;
            });

            record("36_sm_multiple", [&](std::string &message) -> bool {
                LuaJITHost script_host;
                if (!script_host.initialize()) { message = "host init failed"; return false; }
                SandboxedFilesystem fs(root, true);
                PortableScriptLoader loader(script_host, fs);
                PortableScriptRuntime runtime(loader);
                ScriptManager manager;
                manager.set_runtime(&runtime);
                if (!fs.write_file("sm_a.lua", "sm_a_ok = 1\n").ok ||
                    !fs.write_file("sm_b.lua", "sm_b_ok = 2\n").ok) { message = "write failed"; return false; }
                for (const char* id : {"sm_a","sm_b"}) {
                    ScriptMetadata meta;
                    meta.id = id;
                    meta.path = std::string(id) + ".lua";
                    if (!manager.register_script(meta).ok) { message = "register failed"; return false; }
                    if (!manager.load(id).ok) { message = std::string("load failed: ") + manager.last_error(); return false; }
                }
                if (manager.active_count() != 2) { message = "active count wrong"; return false; }
                return script_host.get_global_number("sm_a_ok") == 1.0 &&
                       script_host.get_global_number("sm_b_ok") == 2.0;
            });

            record("37_sm_reload", [&](std::string &message) -> bool {
                LuaJITHost script_host;
                if (!script_host.initialize()) { message = "host init failed"; return false; }
                SandboxedFilesystem fs(root, true);
                PortableScriptLoader loader(script_host, fs);
                PortableScriptRuntime runtime(loader);
                ScriptManager manager;
                manager.set_runtime(&runtime);
                if (!fs.write_file("sm_reload.lua", "sm_reload_ok = 5\n").ok) { message = "write failed"; return false; }
                ScriptMetadata meta;
                meta.id = "sm_reload";
                meta.path = "sm_reload.lua";
                if (!manager.register_script(meta).ok || !manager.load("sm_reload").ok) { message = "load failed"; return false; }
                // update file content then reload
                if (!fs.write_file("sm_reload.lua", "sm_reload_ok = 9\n").ok) { message = "rewrite failed"; return false; }
                // manager.reload uses loader.reload which re-reads original cached source? check semantics
                if (!manager.reload("sm_reload").ok) { message = "reload failed: " + manager.last_error(); return false; }
                return script_host.get_global_number("sm_reload_ok") == 9.0;
            });

            record("38_sm_stop", [&](std::string &message) -> bool {
                LuaJITHost script_host;
                if (!script_host.initialize()) { message = "host init failed"; return false; }
                SandboxedFilesystem fs(root, true);
                PortableScriptLoader loader(script_host, fs);
                PortableScriptRuntime runtime(loader);
                ScriptManager manager;
                manager.set_runtime(&runtime);
                if (!fs.write_file("sm_stop.lua", "sm_stop_ok = 1\n").ok) { message = "write failed"; return false; }
                ScriptMetadata meta;
                meta.id = "sm_stop";
                meta.path = "sm_stop.lua";
                if (!manager.register_script(meta).ok || !manager.load("sm_stop").ok) { message = "load failed"; return false; }
                if (!manager.stop("sm_stop").ok) { message = "stop failed"; return false; }
                if (manager.run("sm_stop").ok) { message = "stopped script still runs"; return false; }
                if (manager.stop("sm_stop").ok) { message = "repeated stop accepted"; return false; }
                return true;
            });

            record("39_sm_unknown", [&](std::string &message) -> bool {
                LuaJITHost script_host;
                if (!script_host.initialize()) { message = "host init failed"; return false; }
                SandboxedFilesystem fs(root, true);
                PortableScriptLoader loader(script_host, fs);
                PortableScriptRuntime runtime(loader);
                ScriptManager manager;
                manager.set_runtime(&runtime);
                if (manager.run("nope").error != arizona_neo::core::CoreError::NotFoundScript) { message = "run unknown wrong error"; return false; }
                if (manager.load("nope").error != arizona_neo::core::CoreError::NotFoundScript) { message = "load unknown wrong error"; return false; }
                if (manager.unload("nope").error != arizona_neo::core::CoreError::NotFoundScript) { message = "unload unknown wrong error"; return false; }
                if (manager.reload("nope").error != arizona_neo::core::CoreError::NotFoundScript) { message = "reload unknown wrong error"; return false; }
                return true;
            });

            record("40_sm_invalid_path", [&](std::string &message) -> bool {
                LuaJITHost script_host;
                if (!script_host.initialize()) { message = "host init failed"; return false; }
                SandboxedFilesystem fs(root, true);
                PortableScriptLoader loader(script_host, fs);
                PortableScriptRuntime runtime(loader);
                ScriptManager manager;
                manager.set_runtime(&runtime);
                ScriptMetadata meta;
                meta.id = "sm_bad";
                meta.path = "../outside.lua";
                if (!manager.register_script(meta).ok) { message = "register failed"; return false; }
                const auto r = manager.load("sm_bad");
                if (r.ok || r.error != arizona_neo::core::CoreError::PathTraversal) { message = "path traversal not rejected"; return false; }
                return true;
            });

            record("41_sm_stack", [&](std::string &message) -> bool {
                LuaJITHost script_host;
                if (!script_host.initialize()) { message = "host init failed"; return false; }
                SandboxedFilesystem fs(root, true);
                PortableScriptLoader loader(script_host, fs);
                PortableScriptRuntime runtime(loader);
                ScriptManager manager;
                manager.set_runtime(&runtime);
                if (!fs.write_file("sm_err.lua", "error('sm_boom')\n").ok) { message = "write failed"; return false; }
                ScriptMetadata meta;
                meta.id = "sm_err";
                meta.path = "sm_err.lua";
                if (!manager.register_script(meta).ok) { message = "register failed"; return false; }
                const auto r = manager.load("sm_err");
                if (r.ok) { message = "error script loaded"; return false; }
                if (script_host.stack_top() != 0) { message = "stack not clean after error"; return false; }
                // other script still works
                if (!fs.write_file("sm_ok.lua", "sm_ok_ok = 7\n").ok) { message = "write failed"; return false; }
                ScriptMetadata meta2;
                meta2.id = "sm_ok";
                meta2.path = "sm_ok.lua";
                if (!manager.register_script(meta2).ok || !manager.load("sm_ok").ok || !manager.run("sm_ok").ok) { message = "survivor failed"; return false; }
                return script_host.get_global_number("sm_ok_ok") == 7.0;
            });

            record("42_sm_shutdown_reinit", [&](std::string &message) -> bool {
                LuaJITHost script_host;
                if (!script_host.initialize()) { message = "host init failed"; return false; }
                SandboxedFilesystem fs(root, true);
                PortableScriptLoader loader(script_host, fs);
                PortableScriptRuntime runtime(loader);
                ScriptManager manager;
                manager.set_runtime(&runtime);
                if (!fs.write_file("sm_reinit.lua", "sm_reinit_ok = 3\n").ok) { message = "write failed"; return false; }
                ScriptMetadata meta;
                meta.id = "sm_reinit";
                meta.path = "sm_reinit.lua";
                if (!manager.register_script(meta).ok || !manager.load("sm_reinit").ok) { message = "load failed"; return false; }
                script_host.shutdown();
                if (manager.run("sm_reinit").ok) { message = "ran after shutdown"; return false; }
                if (!script_host.initialize()) { message = "reinit failed"; return false; }
                if (!manager.reload("sm_reinit").ok) { message = "reload recovery failed: " + manager.last_error(); return false; }
                if (!manager.run("sm_reinit").ok) { message = "run after reinit failed"; return false; }
                manager.clear_runtime();
                const auto unbound = manager.run("sm_reinit");
                if (unbound.ok || unbound.error != arizona_neo::core::CoreError::RuntimeUnavailable) { message = "unbound not RuntimeUnavailable"; return false; }
                return script_host.stack_top() == 0;
            });

            // Real original NeoLoader script execution (lib/sha256.lua).
            record("43_neo_script_execute", [&](std::string &message) -> bool {
                arizona_neo::runtime::LuaLibraryPolicy policy;
                policy.bit = true;
                LuaJITHost script_host(policy);
                if (!script_host.initialize()) { message = "host init failed"; return false; }
                SandboxedFilesystem fs(root, true);
                PortableScriptLoader loader(script_host, fs);
                PortableScriptRuntime runtime(loader);
                ScriptManager manager;
                manager.set_runtime(&runtime);
                if (!fs.write_file("neo_sha256_device.lua", NEO_SHA256_SCRIPT).ok) { message = "write failed"; return false; }
                ScriptMetadata meta;
                meta.id = "neo_sha256";
                meta.path = "neo_sha256_device.lua";
                if (!manager.register_script(meta).ok) { message = "register failed"; return false; }
                auto load_result = manager.load("neo_sha256");
                if (!load_result.ok) { message = "neo script load failed: " + manager.last_error(); return false; }
                if (!manager.run("neo_sha256").ok) { message = "neo script run failed: " + manager.last_error(); return false; }
                if (script_host.stack_top() != 0) { message = "stack not clean"; return false; }
                return true;
            });
        }
    }

    // Overall verdict: FAIL wins over BLOCKED; PASS only when every test passed.
    BOOL sawFailure = NO;
    BOOL sawBlocked = NO;
    for (ANDTestResult *result in self.mutableResults) {
        if (result.status == ANDTestStatusFail) sawFailure = YES;
        if (result.status == ANDTestStatusBlocked) sawBlocked = YES;
    }
    if (self.mutableResults.count == 0 || sawFailure) {
        self.internalOverall = @"FAIL";
    } else if (sawBlocked) {
        self.internalOverall = @"BLOCKED";
    } else {
        self.internalOverall = @"PASS";
    }
}

@end
