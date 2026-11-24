#import "DeviceRuntimeTests.h"

#include "arizona_neo/runtime/lua_jit_host.hpp"

#include <chrono>
#include <cstdarg>
#include <exception>
#include <functional>
#include <string>

using arizona_neo::runtime::LuaJITHost;

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
