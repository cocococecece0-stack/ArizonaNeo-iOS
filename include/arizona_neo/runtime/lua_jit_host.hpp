#pragma once

#include "arizona_neo/core/lua_host.hpp"

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
    ~LuaJITHost() override;

    bool initialize() override;
    bool execute(
        std::string_view source,
        std::string_view chunk_name
    ) override;
    void shutdown() noexcept override;

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
    lua_State* state_ = nullptr;
    bool initialized_ = false;
    std::string last_error_;
};

} // namespace arizona_neo::runtime
