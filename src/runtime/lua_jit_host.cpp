#if defined(ARIZONA_NEO_WITH_LUAJIT)

#include "arizona_neo/runtime/lua_jit_host.hpp"

// LuaJIT headers (unlike many other C libraries) do not contain their own
// `extern "C"` guards, so they MUST be wrapped here when compiled as C++.
// Without this, the C++ compiler mangles Lua API names (e.g. _Z13luaL_newstatev)
// and the linker cannot resolve them against libluajit.a (Stage 6.2.3).
extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <exception>
#include <string>

namespace arizona_neo::runtime {
namespace {

// Lua 5.1's lua_getglobal() may invoke an __index metamethod on the globals
// table. A Lua error there is a longjmp, which C++ try/catch cannot intercept
// and which would bypass destructors of live C++ objects (e.g. std::string in
// get_global_string). Running it as a C trampoline under lua_pcall() converts
// such a longjmp into a normal error return code.
int protected_getglobal(lua_State* L) {
    const char* name = static_cast<const char*>(lua_touserdata(L, 1));
    lua_getglobal(L, name);
    return 1;
}

// Copy only string/number error objects. lua_tolstring() on an arbitrary
// object can run __tostring in some implementations; checking lua_type() first
// keeps error extraction free of metamethod calls and longjmps.
void copy_error_message(lua_State* L, std::string& out) {
    out.clear();
    const int type = lua_type(L, -1);
    if (type == LUA_TSTRING || type == LUA_TNUMBER) {
        std::size_t len = 0;
        const char* msg = lua_tolstring(L, -1, &len);
        if (msg != nullptr) {
            out.assign(msg, len);
        }
    }
    if (out.empty()) {
        out = "Lua error (non-string error object)";
    }
    lua_settop(L, 0);
}

} // namespace

LuaJITHost::LuaJITHost() = default;

LuaJITHost::~LuaJITHost() {
    shutdown();
}

bool LuaJITHost::initialize() {
    if (initialized_) {
        // Idempotent: never create a second lua_State while one is alive.
        return true;
    }
    last_error_.clear();
    try {
        state_ = luaL_newstate();
        if (state_ == nullptr) {
            last_error_ = "luaL_newstate returned nullptr";
            return false;
        }
        // Minimal, controlled set of standard libraries (Lua 5.1/LuaJIT pattern).
        // io/os/debug are intentionally NOT opened at this stage.
        auto open_lib = [&](lua_CFunction fn, const char* name) -> bool {
            lua_pushcfunction(state_, fn);
            lua_pushstring(state_, name ? name : "");
            if (lua_pcall(state_, 1, 0, 0) != 0) {
                copy_error_message(state_, last_error_);
                return false;
            }
            return true;
        };
        if (!open_lib(luaopen_base, "")) {
            lua_close(state_); state_ = nullptr; initialized_ = false; return false;
        }
        if (!open_lib(luaopen_math, LUA_MATHLIBNAME)) {
            lua_close(state_); state_ = nullptr; initialized_ = false; return false;
        }
        if (!open_lib(luaopen_string, LUA_STRLIBNAME)) {
            lua_close(state_); state_ = nullptr; initialized_ = false; return false;
        }
        if (!open_lib(luaopen_table, LUA_TABLIBNAME)) {
            lua_close(state_); state_ = nullptr; initialized_ = false; return false;
        }
        if (!open_lib(luaopen_package, LUA_LOADLIBNAME)) {
            lua_close(state_); state_ = nullptr; initialized_ = false; return false;
        }
        if (!open_lib(luaopen_ffi, LUA_FFILIBNAME)) {
            lua_close(state_); state_ = nullptr; initialized_ = false; return false;
        }
        if (!open_lib(luaopen_jit, LUA_JITLIBNAME)) {
            lua_close(state_); state_ = nullptr; initialized_ = false; return false;
        }

        initialized_ = true;
        return true;
    } catch (const std::exception& e) {
        last_error_ = e.what();
        if (state_ != nullptr) {
            lua_close(state_);
            state_ = nullptr;
        }
        initialized_ = false;
        return false;
    } catch (...) {
        last_error_ = "unknown exception during initialize";
        if (state_ != nullptr) {
            lua_close(state_);
            state_ = nullptr;
        }
        initialized_ = false;
        return false;
    }
}

bool LuaJITHost::execute(std::string_view source, std::string_view chunk_name) {
    if (!initialized_ || state_ == nullptr) {
        last_error_ = "runtime not initialized";
        return false;
    }
    last_error_.clear();

    std::string name(chunk_name);
    if (name.empty()) {
        name = "chunk";
    }
    std::string code(source);

    try {
        // Both luaL_loadbuffer and lua_pcall are protected Lua C API entry
        // points: Lua errors inside the chunk become non-zero return codes.
        if (luaL_loadbuffer(state_, code.c_str(), code.size(), name.c_str()) != 0) {
            copy_error_message(state_, last_error_);
            return false;
        }

        if (lua_pcall(state_, 0, 1, 0) != 0) {
            copy_error_message(state_, last_error_);
            return false;
        }

        // Leave no results or error objects on the stack after any execution.
        lua_settop(state_, 0);
        return true;
    } catch (const std::exception& e) {
        last_error_ = e.what();
        lua_settop(state_, 0);
        return false;
    } catch (...) {
        last_error_ = "unknown exception during execute";
        lua_settop(state_, 0);
        return false;
    }
}

void LuaJITHost::shutdown() noexcept {
    if (state_ != nullptr) {
        // lua_close() may run __gc finalizers. LuaJIT protects each finalizer
        // internally with lj_vm_pcall (lj_gc.c), but arbitrary Lua code can
        // still run here; callers must not depend on finalizer side effects.
        // It is invoked with no live C++ locals that need unwinding.
        lua_close(state_);
        state_ = nullptr;
    }
    initialized_ = false;
}

int LuaJITHost::stack_top() const noexcept {
    return state_ != nullptr ? lua_gettop(state_) : 0;
}

const std::string& LuaJITHost::last_error() const noexcept {
    return last_error_;
}

bool LuaJITHost::is_initialized() const noexcept {
    return initialized_;
}

double LuaJITHost::get_global_number(const char* name) const {
    if (state_ == nullptr || name == nullptr) {
        return 0.0;
    }
    lua_pushcfunction(state_, protected_getglobal);
    lua_pushlightuserdata(state_, const_cast<char*>(name));
    if (lua_pcall(state_, 1, 1, 0) != 0) {
        // lua_pcall left the error object on the stack; drop it.
        lua_settop(state_, 0);
        return 0.0;
    }
    double v = 0.0;
    if (lua_isnumber(state_, -1)) {
        v = lua_tonumber(state_, -1);
    }
    lua_settop(state_, 0);
    return v;
}

std::string LuaJITHost::get_global_string(const char* name) const {
    if (state_ == nullptr || name == nullptr) {
        return {};
    }
    lua_pushcfunction(state_, protected_getglobal);
    lua_pushlightuserdata(state_, const_cast<char*>(name));
    if (lua_pcall(state_, 1, 1, 0) != 0) {
        lua_settop(state_, 0);
        return {};
    }
    std::string v;
    // lua_isstring accepts numbers too; lua_tolstring converts them in place
    // without invoking metamethods.
    if (lua_isstring(state_, -1)) {
        std::size_t len = 0;
        const char* s = lua_tolstring(state_, -1, &len);
        if (s != nullptr) {
            v.assign(s, len);
        }
    }
    lua_settop(state_, 0);
    return v;
}

} // namespace arizona_neo::runtime

#endif // ARIZONA_NEO_WITH_LUAJIT
