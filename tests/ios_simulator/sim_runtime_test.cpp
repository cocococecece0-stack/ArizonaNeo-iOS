// Real iOS Simulator runtime test for the integrated LuaJITHost.
// Executed inside the Simulator via `xcrun simctl spawn <device> <binary>`.
// Returns 0 only when every scenario passes.

#include "arizona_neo/runtime/lua_jit_host.hpp"

#include <iostream>
#include <string>

using arizona_neo::runtime::LuaJITHost;

static int failures = 0;

static void check(bool ok, const char* name) {
    std::cout << (ok ? "PASS " : "FAIL ") << name << "\n";
    if (!ok) ++failures;
}

static void run_ok(LuaJITHost& host, const char* code, const char* name) {
    const bool ok = host.execute(code, name);
    check(ok, (std::string("execute: ") + name).c_str());
    check(host.stack_top() == 0, (std::string("stack clean: ") + name).c_str());
}

int main() {
    LuaJITHost host;

    // --- Lifecycle: initialization -------------------------------------
    check(!host.is_initialized(), "fresh state is not initialized");
    check(host.initialize(), "initialize()");
    check(host.is_initialized(), "is_initialized() after initialize");
    check(host.initialize(), "repeated initialize() is idempotent");

    // --- Basic execution ------------------------------------------------
    // `execute()` returns bool; verify the Lua value through a global.
    run_ok(host,
           "local result = 20 + 22\n"
           "assert(result == 42)\n"
           "basic_result = result", "basic arithmetic");
    check(host.get_global_number("basic_result") == 42.0, "arithmetic result via global");

    // --- Global values ---------------------------------------------------
    run_ok(host,
           "test_number = 123\n"
           "test_string = 'ArizonaNeo'\n"
           "assert(test_number == 123)\n"
           "assert(test_string == 'ArizonaNeo')", "globals in Lua");
    check(host.get_global_number("test_number") == 123.0, "get_global_number()");
    check(host.get_global_string("test_string") == "ArizonaNeo", "get_global_string()");
    check(host.get_global_number("missing_global") == 0.0, "missing number global");
    check(host.get_global_string("missing_global").empty(), "missing string global");
    check(host.get_global_number(nullptr) == 0.0, "nullptr name (number)");
    check(host.get_global_string(nullptr).empty(), "nullptr name (string)");

    // --- Standard libraries (minimal intentional set) --------------------
    run_ok(host,
           "assert(type(print) == 'function')            -- base\n"
           "assert(math.floor(4.2) == 4)                -- math\n"
           "assert(('abc'):upper() == 'ABC')            -- string\n"
           "assert(table.concat({1,2}, ',') == '1,2')   -- table\n"
           "assert(type(require) == 'function')         -- package\n"
           "assert(require('ffi') ~= nil)              -- ffi\n"
           "assert(require('jit') ~= nil)              -- jit\n"
           "libs_ok = 1", "standard libraries");
    check(host.get_global_number("libs_ok") == 1.0, "base/math/string/table/package/ffi/jit available");
    // io/os/debug are intentionally NOT opened by this stage.
    run_ok(host,
           "debug_closed = (io == nil and os == nil and debug == nil) and 1 or 0",
           "io/os/debug not opened");
    check(host.get_global_number("debug_closed") == 1.0, "minimal library surface preserved");

    // --- FFI (executed in the Simulator) ---------------------------------
    run_ok(host,
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
           "ffi_result = point.x + point.y", "FFI struct");
    check(host.get_global_number("ffi_result") == 42.0, "FFI struct fields and arithmetic");

    // --- JIT disabled in the Simulator LuaJIT build ----------------------
    run_ok(host,
           "local j = require('jit')\n"
           "jit_status = j.status() and 1 or 0\n"
           "jit_status_count = select('#', j.status())", "jit.status()");
    check(host.get_global_number("jit_status") == 0.0, "JIT disabled");
    check(host.get_global_number("jit_status_count") == 1.0, "JIT-disabled status has one value");

    // --- Error handling and recovery -------------------------------------
    check(!host.execute("local x = 20 +", "syntax error"), "syntax error returns false");
    check(!host.last_error().empty(), "syntax error message");
    check(host.stack_top() == 0, "stack clean after syntax error");
    check(!host.execute("error('simulator boom')", "runtime error"), "runtime error returns false");
    check(host.last_error().find("simulator boom") != std::string::npos, "runtime error message");
    check(host.stack_top() == 0, "stack clean after runtime error");
    run_ok(host, "after_error = 7", "execute after errors");
    check(host.get_global_number("after_error") == 7.0, "Lua state intact after errors");
    check(host.last_error().empty(), "last_error cleared on success");

    // --- Lifecycle: multiple executions, shutdown, re-init ---------------
    run_ok(host, "seq_a = 1", "sequential execute 1");
    run_ok(host, "seq_b = seq_a + 1", "sequential execute 2");
    check(host.get_global_number("seq_b") == 2.0, "state shared across executes");

    host.shutdown();
    check(!host.is_initialized(), "shutdown()");
    check(host.stack_top() == 0, "stack empty after shutdown");
    host.shutdown();
    check(!host.is_initialized(), "repeated shutdown() is safe");
    check(!host.execute("return 1", "after shutdown"), "execute after shutdown fails");
    check(!host.last_error().empty(), "error after shutdown");

    check(host.initialize(), "initialize after shutdown");
    check(host.is_initialized(), "initialized again");
    run_ok(host, "reinit = 99", "execute after re-init");
    check(host.get_global_number("reinit") == 99.0, "re-init runtime works");
    check(host.get_global_string("test_string").empty(), "re-init starts with fresh state");
    host.shutdown();

    if (failures == 0) {
        std::cout << "SIM_RUNTIME_TESTS: PASSED\n";
        return 0;
    }
    std::cout << "SIM_RUNTIME_TESTS: " << failures << " FAILED\n";
    return 1;
}
