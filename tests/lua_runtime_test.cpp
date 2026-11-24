#include "arizona_neo/core/lua_host.hpp"

#if defined(ARIZONA_NEO_WITH_LUAJIT)
#include "arizona_neo/runtime/lua_jit_host.hpp"
#endif

#include <iostream>
#include <string>

using namespace arizona_neo::core;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << (msg) << "\n"; ++failures; } } while (0)

#if defined(ARIZONA_NEO_WITH_LUAJIT)
using arizona_neo::runtime::LuaJITHost;

// Every successful/failed execute() and every getter must leave the stack empty.
static void check_stack(LuaJITHost& h, const char* where) {
    if (h.stack_top() != 0) {
        std::cerr << "FAIL: stack not empty after " << where
                  << " (top=" << h.stack_top() << ")\n";
        ++failures;
    }
}

static void run_ok(LuaJITHost& h, const char* code, const char* name) {
    CHECK(h.execute(code, name), name);
    check_stack(h, name);
}
#endif

int main() {
#if !defined(ARIZONA_NEO_WITH_LUAJIT)
    std::cout << "lua runtime tests: SKIPPED (LuaJIT disabled)\n";
    return 0;
#else
    // ------------------------------------------------------------------
    // 1. Initialization lifecycle
    // ------------------------------------------------------------------
    LuaJITHost host;
    CHECK(!host.is_initialized(), "fresh host is not initialized");
    CHECK(host.stack_top() == 0, "fresh host stack empty");
    CHECK(host.initialize(), "initialize");
    CHECK(host.is_initialized(), "is_initialized");
    run_ok(host, "persistent = 'kept'", "seed");
    CHECK(host.get_global_string("persistent") == "kept", "seed value");
    check_stack(host, "get_global_string");

    // Repeated initialize() must neither replace the state nor leak another one.
    CHECK(host.initialize(), "repeat initialize");
    CHECK(host.is_initialized(), "still initialized");
    CHECK(host.get_global_string("persistent") == "kept", "repeat init keeps state");

    // ------------------------------------------------------------------
    // 2. Lua execution
    // ------------------------------------------------------------------
    // bool result is not treated as the expression value: read it via a global.
    CHECK(host.execute("return 2 + 2", "t1"), "execute 2+2");
    check_stack(host, "execute 2+2");
    run_ok(host, "result_value = 2 + 2", "t1b");
    CHECK(host.get_global_number("result_value") == 4.0, "numeric result 4");

    run_ok(host, "local value = 123\nassert(value == 123)\nlocal_value = value", "t2");
    CHECK(host.get_global_number("local_value") == 123.0, "local assert script");

    run_ok(host, "local text = 'LuaJIT runtime works'\nassert(#text > 0)\ntext_value = text", "t3");
    CHECK(host.get_global_string("text_value") == "LuaJIT runtime works", "string script");

    // Several sequential chunks sharing state.
    run_ok(host, "a = 1", "m1");
    run_ok(host, "b = a + 1", "m2");
    run_ok(host, "c = b * 10", "m3");
    CHECK(host.get_global_number("c") == 20.0, "sequential scripts");

    // Syntax error.
    {
        const bool ok = host.execute("return 2 +", "syn");
        CHECK(!ok, "syntax error returns false");
        CHECK(!host.last_error().empty(), "syntax error message");
        check_stack(host, "syntax error");
    }
    // Runtime error.
    {
        const bool ok = host.execute("error('boom')", "rt");
        CHECK(!ok, "runtime error returns false");
        CHECK(host.last_error().find("boom") != std::string::npos, "runtime error message");
        check_stack(host, "runtime error");
    }
    // Recovery and re-execution after errors.
    run_ok(host, "after_error = 99", "recover");
    CHECK(host.get_global_number("after_error") == 99.0, "state intact after errors");
    CHECK(host.last_error().empty(), "last_error cleared on success");

    // ------------------------------------------------------------------
    // 3. Global value helpers
    // ------------------------------------------------------------------
    run_ok(host, "g_num = 42.5\ng_str = 'hello'\ng_tbl = {}", "globals");
    CHECK(host.get_global_number("g_num") == 42.5, "get_global_number");
    CHECK(host.get_global_string("g_str") == "hello", "get_global_string");
    CHECK(host.get_global_number("no_such_global") == 0.0, "missing number global");
    CHECK(host.get_global_string("no_such_global").empty(), "missing string global");
    CHECK(host.get_global_number("g_tbl") == 0.0, "wrong-type number global");
    CHECK(host.get_global_string("g_tbl").empty(), "wrong-type string global");
    CHECK(host.get_global_number(nullptr) == 0.0, "nullptr name (number)");
    CHECK(host.get_global_string(nullptr).empty(), "nullptr name (string)");
    check_stack(host, "global helpers");

    // An __index metamethod on _G that calls error() makes lua_getglobal()
    // longjmp. Protected trampolines must turn this into a safe error return.
    run_ok(host,
           "meta_restore = setmetatable\n"
           "setmetatable(_G, { __index = function() error('meta boom') end })",
           "meta_setup");
    CHECK(host.get_global_number("missing_under_metamethod") == 0.0,
          "get_global_number survives __index longjmp");
    check_stack(host, "metamethod getter");
    run_ok(host, "meta_restore(_G, nil)\nmeta_restore = nil", "meta_restore");
    run_ok(host, "after_meta = 7", "after_meta");
    CHECK(host.get_global_number("after_meta") == 7.0, "state intact after metamethod error");

    // ------------------------------------------------------------------
    // 4. JIT disabled (behavioral check via integrated runtime)
    // ------------------------------------------------------------------
    {
        LuaJITHost jit_host;
        CHECK(jit_host.initialize(), "jit host init");
        // jit.status() returns false + (with JIT enabled) CPU/opt flag strings.
        run_ok(jit_host,
               "local j = require('jit')\n"
               "jit_status = j.status() and 1 or 0\n"
               "jit_status_count = select('#', j.status())",
               "jit_status");
        CHECK(jit_host.get_global_number("jit_status") == 0.0, "JIT disabled");
        CHECK(jit_host.get_global_number("jit_status_count") == 1.0,
              "JIT-disabled status has exactly one value");
        jit_host.shutdown();
    }

    // ------------------------------------------------------------------
    // 5. FFI through the integrated LuaJITHost
    // ------------------------------------------------------------------
    {
        LuaJITHost ffi_host;
        CHECK(ffi_host.initialize(), "ffi host init");
        run_ok(ffi_host,
               "local ffi = require('ffi')\n"
               "ffi_available = ffi and 1 or 0\n"
               "ffi.cdef[[\n"
               "typedef struct {\n"
               "    int x;\n"
               "    int y;\n"
               "} TestPoint;\n"
               "]]\n"
               "local point = ffi.new('TestPoint')\n"
               "point.x = 10\n"
               "point.y = 20\n"
               "assert(point.x == 10)\n"
               "assert(point.y == 20)\n"
               "ffi_x = point.x\n"
               "ffi_y = point.y",
               "ffi_struct");
        CHECK(ffi_host.get_global_number("ffi_available") == 1.0, "FFI module available");
        CHECK(ffi_host.get_global_number("ffi_x") == 10.0, "FFI field write/read x");
        CHECK(ffi_host.get_global_number("ffi_y") == 20.0, "FFI field write/read y");

        // An FFI error must reach C++ as false and must not destroy the state.
        {
            const bool ok = ffi_host.execute(
                "local ffi = require('ffi')\n"
                "local p = ffi.new('ThisTypeDoesNotExist')", "ffi_error");
            CHECK(!ok, "FFI error returns false");
            CHECK(!ffi_host.last_error().empty(), "FFI error message");
            check_stack(ffi_host, "FFI error");
        }
        run_ok(ffi_host, "ffi_after_error = 5", "ffi_recover");
        CHECK(ffi_host.get_global_number("ffi_after_error") == 5.0,
              "state intact after FFI error");
        ffi_host.shutdown();
    }

    // ------------------------------------------------------------------
    // 6. Shutdown lifecycle
    // ------------------------------------------------------------------
    host.shutdown();
    CHECK(!host.is_initialized(), "shutdown");
    CHECK(host.stack_top() == 0, "stack after shutdown");
    host.shutdown(); // repeated shutdown must be a no-op
    CHECK(!host.is_initialized(), "repeat shutdown");
    CHECK(!host.execute("return 1", "after_shutdown"), "execute after shutdown false");
    CHECK(!host.last_error().empty(), "error after shutdown");

    // Re-initialize after shutdown: new state, old globals gone.
    CHECK(host.initialize(), "reinitialize");
    CHECK(host.is_initialized(), "reinitialized flag");
    run_ok(host, "fresh = 1", "after_reinit");
    CHECK(host.get_global_number("fresh") == 1.0, "reinit execution");
    CHECK(host.get_global_string("persistent").empty(), "reinit has fresh state");
    host.shutdown();

    if (failures == 0) std::cout << "lua runtime tests: PASSED\n";
    else std::cerr << "lua runtime tests: " << failures << " FAILED\n";
    return failures == 0 ? 0 : 1;
#endif
}
