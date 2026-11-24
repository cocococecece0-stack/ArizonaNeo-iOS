// XCTest suite executed INSIDE an iOS Simulator by `xcodebuild test`.
// Every assertion runs in the Simulator process via the integrated LuaJITHost.

#import <XCTest/XCTest.h>

#include "arizona_neo/runtime/lua_jit_host.hpp"

#include <string>

using arizona_neo::runtime::LuaJITHost;

@interface SimRuntimeTests : XCTestCase
@end

@implementation SimRuntimeTests

static void RunOK(LuaJITHost& host, const char* code, const char* name) {
    XCTAssertTrue(host.execute(code, name), @"%s", name);
    XCTAssertEqual(host.stack_top(), 0, @"stack hygiene after %s", name);
}

- (void)testInitializationLifecycle {
    LuaJITHost host;
    XCTAssertFalse(host.is_initialized());
    XCTAssertTrue(host.initialize());
    XCTAssertTrue(host.is_initialized());
    XCTAssertTrue(host.initialize(), @"repeated initialize must be safe");

    host.shutdown();
    XCTAssertFalse(host.is_initialized());
    XCTAssertEqual(host.stack_top(), 0);
    host.shutdown();  // repeated shutdown must be safe
    XCTAssertFalse(host.is_initialized());

    XCTAssertTrue(host.initialize(), @"initialize after shutdown");
    XCTAssertTrue(host.is_initialized());
    RunOK(host, "reinit = 99", "reinit");
    XCTAssertEqual(host.get_global_number("reinit"), 99.0);
    XCTAssertTrue(host.get_global_string("test_number").empty(), @"fresh state after re-init");
    host.shutdown();
}

- (void)testBasicExecutionAndGlobals {
    LuaJITHost host;
    XCTAssertTrue(host.initialize());

    RunOK(host,
          "local result = 20 + 22\n"
          "assert(result == 42)\n"
          "basic_result = result", "basic arithmetic");
    XCTAssertEqual(host.get_global_number("basic_result"), 42.0);

    RunOK(host,
          "test_number = 123\n"
          "test_string = 'ArizonaNeo'\n"
          "assert(test_number == 123)\n"
          "assert(test_string == 'ArizonaNeo')", "globals");
    XCTAssertEqual(host.get_global_number("test_number"), 123.0);
    XCTAssertEqual(host.get_global_string("test_string"), std::string("ArizonaNeo"));
    XCTAssertEqual(host.get_global_number("missing_global"), 0.0);
    XCTAssertTrue(host.get_global_string("missing_global").empty());
    XCTAssertEqual(host.get_global_number(nullptr), 0.0);
    XCTAssertTrue(host.get_global_string(nullptr).empty());
    host.shutdown();
}

- (void)testStandardLibraries {
    LuaJITHost host;
    XCTAssertTrue(host.initialize());
    RunOK(host,
          "assert(type(print) == 'function')            -- base\n"
          "assert(math.floor(4.2) == 4)                -- math\n"
          "assert(('abc'):upper() == 'ABC')            -- string\n"
          "assert(table.concat({1,2}, ',') == '1,2')   -- table\n"
          "assert(type(require) == 'function')         -- package\n"
          "assert(require('ffi') ~= nil)              -- ffi\n"
          "assert(require('jit') ~= nil)              -- jit\n"
          "libs_ok = 1", "standard libraries");
    XCTAssertEqual(host.get_global_number("libs_ok"), 1.0);

    // io/os/debug are intentionally not opened by this stage.
    RunOK(host,
          "debug_closed = (io == nil and os == nil and debug == nil) and 1 or 0",
          "minimal library surface");
    XCTAssertEqual(host.get_global_number("debug_closed"), 1.0);

    RunOK(host,
          "local j = require('jit')\n"
          "jit_status = j.status() and 1 or 0\n"
          "jit_status_count = select('#', j.status())", "jit.status");
    XCTAssertEqual(host.get_global_number("jit_status"), 0.0, @"LuaJIT must be interpreter-only");
    XCTAssertEqual(host.get_global_number("jit_status_count"), 1.0);
    host.shutdown();
}

- (void)testFFIStructExecution {
    LuaJITHost host;
    XCTAssertTrue(host.initialize());
    RunOK(host,
          "local ffi = require('ffi')\n"
          "ffi.cdef[[\n"
          "    typedef struct {\n"
          "        int x;\n"
          "        int y;\n"
          "    } NeoTestPoint;\n"
          "]]\n"
          "local point = ffi.new('NeoTestPoint')\n"
          "point.x = 12\n"
          "point.y = 30\n"
          "assert(point.x + point.y == 42)\n"
          "ffi_result = point.x + point.y", "FFI NeoTestPoint");
    XCTAssertEqual(host.get_global_number("ffi_result"), 42.0);

    // An FFI error must be reported to C++ without destroying the Lua state.
    XCTAssertFalse(host.execute("local ffi = require('ffi'); local p = ffi.new('NoSuchType')",
                                "ffi error"));
    XCTAssertFalse(host.last_error().empty());
    XCTAssertEqual(host.stack_top(), 0);
    RunOK(host, "ffi_after_error = 5", "FFI recovery");
    XCTAssertEqual(host.get_global_number("ffi_after_error"), 5.0);
    host.shutdown();
}

- (void)testErrorRecoveryAndStackHygiene {
    LuaJITHost host;
    XCTAssertTrue(host.initialize());
    XCTAssertFalse(host.execute("local x = 20 +", @"syntax error"));
    XCTAssertFalse(host.last_error().empty());
    XCTAssertEqual(host.stack_top(), 0);
    XCTAssertFalse(host.execute("error('simulator boom')", @"runtime error"));
    XCTAssertTrue(host.last_error().find("simulator boom") != std::string::npos);
    XCTAssertEqual(host.stack_top(), 0);
    RunOK(host, "after_error = 7", "recovery");
    XCTAssertEqual(host.get_global_number("after_error"), 7.0);
    XCTAssertTrue(host.last_error().empty());

    RunOK(host, "seq_a = 1", "sequential 1");
    RunOK(host, "seq_b = seq_a + 1", "sequential 2");
    XCTAssertEqual(host.get_global_number("seq_b"), 2.0);
    host.shutdown();
}

@end
