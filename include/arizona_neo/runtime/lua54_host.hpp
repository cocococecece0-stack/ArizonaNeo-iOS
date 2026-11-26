#pragma once

#include "arizona_neo/core/lua_host.hpp"
#include "arizona_neo/runtime/lua_runtime_contract.hpp"

#include <map>
#include <string>

struct lua_State;

namespace arizona_neo::runtime {

// Lua 5.4 backend. Experimental: provides the same host surface as
// LuaJITHost for scripts that do not need LuaJIT-only features (ffi, bit
// module is supplied separately as a compatibility module). It is not yet
// wired into PortableScriptLoader/ScriptManager.
class Lua54Host : public arizona_neo::core::LuaHost {
public:
    Lua54Host();
    explicit Lua54Host(const LuaLibraryPolicy& policy);
    ~Lua54Host() override;

    Lua54Host(const Lua54Host&) = delete;
    Lua54Host& operator=(const Lua54Host&) = delete;

    bool initialize() override;
    bool execute(std::string_view source, std::string_view chunk_name) override;
    void shutdown() noexcept override;

    LuaExecutionResult execute_detailed(std::string_view source, std::string_view chunk_name);

    const LuaLibraryPolicy& library_policy() const noexcept { return policy_; }

    bool register_function(const char* name, LuaBridgeFunction function);
    const std::string& last_error() const noexcept { return last_error_; }
    bool is_initialized() const noexcept { return initialized_; }

    double get_global_number(const char* name) const;
    std::string get_global_string(const char* name) const;
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
