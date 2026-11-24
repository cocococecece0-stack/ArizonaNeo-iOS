#pragma once

#include <functional>
#include <string>

struct lua_State;

namespace arizona_neo::runtime {

// Status of a single script execution. This is a C++ contract; it does not
// depend on any Lua library being open at the call site.
enum class LuaExecutionStatus {
    Success,
    SyntaxError,
    RuntimeError,
    InvalidState,
    InternalError
};

const char *to_string(LuaExecutionStatus status) noexcept;

struct LuaExecutionResult {
    LuaExecutionStatus status = LuaExecutionStatus::InvalidState;
    std::string message;
    int stack_top_after = 0;
};

// Explicit, testable policy for standard library registration.
// `coroutine` is registered by `luaopen_base` in LuaJIT (lib_base.c) and is
// therefore intentionally not listed as a separate flag.
// bit/io/os/debug are OFF by default: they must be enabled only with an
// explicit sandbox-ready adapter and dedicated safety tests.
struct LuaLibraryPolicy {
    bool base = true;
    bool math = true;
    bool string = true;
    bool table = true;
    bool package = true;
    bool ffi = true;
    bool jit = true;

    bool bit = false;
    bool io = false;
    bool os = false;
    bool debug = false;
};

// Signature of a C++ function callable from Lua. The function receives the
// live lua_State and returns the number of Lua results, matching lua_CFunction.
using LuaBridgeFunction = std::function<int(lua_State *)>;

} // namespace arizona_neo::runtime
