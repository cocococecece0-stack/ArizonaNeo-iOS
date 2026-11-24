#include "arizona_neo/core/lua_host.hpp"

#if defined(ARIZONA_NEO_WITH_LUAJIT)
#include "arizona_neo/runtime/lua_jit_host.hpp"
#endif

#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>

using namespace arizona_neo::core;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << (msg) << "\n"; ++failures; } } while (0)

#if defined(ARIZONA_NEO_WITH_LUAJIT)
using arizona_neo::runtime::LuaExecutionResult;
using arizona_neo::runtime::LuaExecutionStatus;
using arizona_neo::runtime::LuaJITHost;
using arizona_neo::runtime::LuaLibraryPolicy;

static_assert(!std::is_copy_constructible<LuaJITHost>::value,
              "LuaJITHost owns a lua_State and must not be copyable");
static_assert(!std::is_copy_assignable<LuaJITHost>::value,
              "LuaJITHost owns a lua_State and must not be copy-assignable");

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

    // ------------------------------------------------------------------
    // 7. A — centralized library policy
    // ------------------------------------------------------------------
    {
        LuaJITHost libs;
        CHECK(libs.initialize(), "policy host init");
        const char* script =
            "policy_base = type(print) == 'function' and 1 or 0\n"
            "policy_coroutine = type(coroutine) ~= 'nil' and 1 or 0\n"
            "policy_math = type(math) ~= 'nil' and 1 or 0\n"
            "policy_string = type(string) ~= 'nil' and 1 or 0\n"
            "policy_table = type(table) ~= 'nil' and 1 or 0\n"
            "policy_package = type(package) ~= 'nil' and 1 or 0\n"
            "policy_ffi = require('ffi') ~= nil and 1 or 0\n"
            "policy_jit = type(jit) ~= 'nil' and 1 or 0\n"
            "policy_forbidden = (io == nil and os == nil and debug == nil and bit == nil) and 1 or 0\n"
            "policy_ok = 1";
        CHECK(libs.execute(script, "library_policy"), "library policy script executed");
        CHECK(libs.get_global_number("policy_ok") == 1.0, "policy script completed");
        CHECK(libs.get_global_number("policy_base") == 1.0, "base open");
        CHECK(libs.get_global_number("policy_coroutine") == 1.0, "coroutine via base open");
        CHECK(libs.get_global_number("policy_math") == 1.0, "math open");
        CHECK(libs.get_global_number("policy_string") == 1.0, "string open");
        CHECK(libs.get_global_number("policy_table") == 1.0, "table open");
        CHECK(libs.get_global_number("policy_package") == 1.0, "package open");
        CHECK(libs.get_global_number("policy_ffi") == 1.0, "ffi open");
        CHECK(libs.get_global_number("policy_jit") == 1.0, "jit open");
        CHECK(libs.get_global_number("policy_forbidden") == 1.0,
              "io/os/debug/bit remain closed");
        CHECK(libs.execute(
            "loaded_ok = (package.loaded['_G'] ~= nil and package.loaded['math'] ~= nil and "
            "package.loaded['ffi'] ~= nil and package.loaded['jit'] ~= nil) and 1 or 0",
            "package_loaded"), "package.loaded populated");
        CHECK(libs.get_global_number("loaded_ok") == 1.0, "opened libraries registered once");
        check_stack(libs, "library policy");
        libs.shutdown();

        LuaLibraryPolicy no_math;
        no_math.math = false;
        LuaJITHost custom(no_math);
        CHECK(custom.initialize(), "custom policy host init");
        CHECK(custom.execute("assert(math == nil)\ncustom_policy_ok = 1", "custom_policy"),
              "custom policy disables math");
        CHECK(custom.get_global_number("custom_policy_ok") == 1.0, "custom policy result");
        custom.shutdown();
    }

    // ------------------------------------------------------------------
    // 8. B — Lua-to-C++ bridge
    // ------------------------------------------------------------------
    {
        LuaJITHost bridge;
        CHECK(bridge.initialize(), "bridge host init");
        CHECK(!bridge.register_function("runtime.ping", [](lua_State*) -> int { return 0; }),
              "duplicate bridge registration rejected");
        CHECK(bridge.execute(
            "bridge_version = runtime.version()\n"
            "bridge_ping = runtime.ping()\n"
            "bridge_echo_num = runtime.echo(42)\n"
            "bridge_echo_str = runtime.echo('abc')", "bridge_calls"),
            "Lua calls C++ bridge functions");
        CHECK(bridge.get_global_string("bridge_version").find("LuaJIT 2.1") != std::string::npos,
              "runtime.version() result");
        CHECK(bridge.get_global_string("bridge_ping") == "pong", "runtime.ping() result");
        CHECK(bridge.get_global_number("bridge_echo_num") == 42.0, "runtime.echo(number)");
        CHECK(bridge.get_global_string("bridge_echo_str") == "abc", "runtime.echo(string)");
        check_stack(bridge, "bridge success");

        CHECK(bridge.register_function("runtime.throws", [](lua_State*) -> int {
            throw std::runtime_error("bridge failure");
        }), "bridge exception function registered");
        const auto bridge_error = bridge.execute_detailed("runtime.throws()", "bridge_error");
        CHECK(bridge_error.status == LuaExecutionStatus::RuntimeError, "C++ exception becomes Lua runtime error");
        CHECK(bridge_error.message.find("bridge failure") != std::string::npos,
              "bridge exception message preserved");
        check_stack(bridge, "bridge error");
        CHECK(bridge.execute("after_bridge_error = 7", "bridge_recovery"), "state recovers after bridge error");
        CHECK(bridge.get_global_number("after_bridge_error") == 7.0, "bridge recovery result");
        bridge.shutdown();
    }

    // ------------------------------------------------------------------
    // 9. C — structured execution result
    // ------------------------------------------------------------------
    {
        LuaJITHost result_host;
        const auto before = result_host.execute_detailed("x = 1", "before_init");
        CHECK(before.status == LuaExecutionStatus::InvalidState, "execution before init is InvalidState");
        CHECK(!before.message.empty(), "invalid-state message");
        CHECK(result_host.initialize(), "result host init");

        const auto ok = result_host.execute_detailed("result_ok = 1", "ok");
        CHECK(ok.status == LuaExecutionStatus::Success, "successful status");
        CHECK(ok.message.empty(), "successful message empty");
        CHECK(ok.stack_top_after == 0, "successful stack restored");

        const auto syntax = result_host.execute_detailed("return 2 +", "syntax");
        CHECK(syntax.status == LuaExecutionStatus::SyntaxError, "syntax error status");
        CHECK(!syntax.message.empty(), "syntax error message preserved");
        CHECK(syntax.stack_top_after == ok.stack_top_after, "syntax stack restored");

        const auto runtime = result_host.execute_detailed("error('contract boom')", "runtime");
        CHECK(runtime.status == LuaExecutionStatus::RuntimeError, "runtime error status");
        CHECK(runtime.message.find("contract boom") != std::string::npos, "runtime message preserved");
        CHECK(runtime.stack_top_after == ok.stack_top_after, "runtime stack restored");

        const bool bool_result = result_host.execute("bool_wrapper = 1", "wrapper");
        CHECK(bool_result, "bool wrapper returns true on success");
        result_host.shutdown();
        const auto after = result_host.execute_detailed("return 1", "after");
        CHECK(after.status == LuaExecutionStatus::InvalidState, "execution after shutdown is InvalidState");
    }

    // ------------------------------------------------------------------
    // 10. D/E — error propagation, stack hygiene and lifecycle
    // ------------------------------------------------------------------
    {
        LuaJITHost life;
        CHECK(life.stack_top() == 0, "fresh stack empty");
        CHECK(life.initialize(), "lifecycle initialize");
        CHECK(life.initialize(), "repeated initialize idempotent");
        CHECK(life.execute("life_a = 1", "life_a"), "lifecycle execute");
        CHECK(!life.execute("error('life boom')", "life_error"), "lifecycle runtime error");
        CHECK(life.last_error().find("life boom") != std::string::npos, "lifecycle error text");
        CHECK(life.stack_top() == 0, "stack clean after error");
        CHECK(life.execute("life_b = 2", "life_b"), "execute after error");
        CHECK(life.get_global_number("life_a") == 1.0, "state intact after error");
        CHECK(life.get_global_number("life_b") == 2.0, "post-error execute result");
        life.shutdown();
        CHECK(!life.is_initialized(), "lifecycle shutdown");
        life.shutdown();
        CHECK(!life.is_initialized(), "repeated shutdown safe");
        const auto after_shutdown = life.execute_detailed("return 1", "post_shutdown");
        CHECK(after_shutdown.status == LuaExecutionStatus::InvalidState, "no use after shutdown");
        CHECK(life.initialize(), "re-initialize after shutdown");
        CHECK(life.execute("life_fresh = 1", "life_fresh"), "execute after re-init");
        CHECK(life.get_global_number("life_fresh") == 1.0, "re-init result");
        CHECK(life.get_global_string("life_a").empty(), "re-init starts with fresh state");
        CHECK(life.stack_top() == 0, "stack clean after re-init");
        life.shutdown();
    }

    if (failures == 0) std::cout << "lua runtime tests: PASSED\n";
    else std::cerr << "lua runtime tests: " << failures << " FAILED\n";
    return failures == 0 ? 0 : 1;
#endif
}
