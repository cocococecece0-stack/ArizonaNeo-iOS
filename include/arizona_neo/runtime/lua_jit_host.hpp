#pragma once

#include "arizona_neo/core/lua_host.hpp"
#include "arizona_neo/runtime/lua_runtime_contract.hpp"

#include <map>
#include <string>

struct lua_State;

namespace arizona_neo::runtime {

// LuaJIT-based implementation of LuaHost.
//
// Available only when ARIZONA_NEO_WITH_LUAJIT is defined at compile time.
// The class keeps a single lua_State and manages its lifetime via RAII.
class LuaJITHost : public arizona_neo::core::LuaHost {
public:
    LuaJITHost();
    explicit LuaJITHost(const LuaLibraryPolicy& policy);
    ~LuaJITHost() override;

    // Owns a single lua_State: copying would double-close it.
    LuaJITHost(const LuaJITHost&) = delete;
    LuaJITHost& operator=(const LuaJITHost&) = delete;

    bool initialize() override;
    bool execute(
        std::string_view source,
        std::string_view chunk_name
    ) override;
    void shutdown() noexcept override;

    // Structured result: distinguishes success, syntax/runtime errors,
    // invalid state and internal C++ failures. `execute()` is a bool wrapper.
    LuaExecutionResult execute_detailed(
        std::string_view source,
        std::string_view chunk_name);

    // The policy actually used when initialize() opened the Lua state.
    const LuaLibraryPolicy& library_policy() const noexcept { return policy_; }

    // Register a C++ callable under a Lua global name (dots create nested
    // tables, e.g. "runtime.ping"). Duplicate names are rejected.
    // Functions registered before initialize() are installed at initialize().
    bool register_function(const char* name, LuaBridgeFunction function);

    // Human-readable error from the last failed operation.
    const std::string& last_error() const noexcept;

    // Whether initialize() succeeded and shutdown() has not been called.
    bool is_initialized() const noexcept;

    // Helpers for tests (extension, not part of LuaHost).
    double get_global_number(const char* name) const;
    std::string get_global_string(const char* name) const;
    // Current Lua stack height (0 when no state). Used to verify stack hygiene.
    int stack_top() const noexcept;

private:
    void install_bridge();
    bool register_bridge_entry(const char* full_name, LuaBridgeFunction& function);

    lua_State* state_ = nullptr;
    bool initialized_ = false;
    std::string last_error_;
    LuaLibraryPolicy policy_;
    std::map<std::string, LuaBridgeFunction> bridge_functions_;
};

} // namespace arizona_neo::runtime
