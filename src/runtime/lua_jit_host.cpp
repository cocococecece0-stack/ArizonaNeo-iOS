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
#include <luajit.h>
}

#include <cstdio>
#include <cstring>
#include <exception>
#include <string>
#include <utility>

namespace arizona_neo::runtime {

const char *to_string(LuaExecutionStatus status) noexcept {
    switch (status) {
        case LuaExecutionStatus::Success: return "Success";
        case LuaExecutionStatus::SyntaxError: return "SyntaxError";
        case LuaExecutionStatus::RuntimeError: return "RuntimeError";
        case LuaExecutionStatus::InvalidState: return "InvalidState";
        case LuaExecutionStatus::InternalError: return "InternalError";
    }
    return "Unknown";
}

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

void copy_to_buffer(char* destination, std::size_t size, const char* text) {
    if (size == 0) return;
    if (text == nullptr) {
        destination[0] = '\0';
        return;
    }
    std::snprintf(destination, size, "%s", text);
}

// The trampoline converts a C++ exception into a Lua error. The message lives
// in a POD buffer so that lua_error()'s longjmp cannot skip a std::string
// destructor. No C++ object with a non-trivial destructor is alive at that point.
int bridge_trampoline(lua_State* L) {
    auto* function = static_cast<LuaBridgeFunction*>(lua_touserdata(L, lua_upvalueindex(1)));
    char message[512];
    message[0] = '\0';
    int results = 0;
    bool failed = false;
    if (function == nullptr || !(*function)) {
        failed = true;
        copy_to_buffer(message, sizeof(message), "runtime bridge: invalid function");
    } else {
        try {
            results = (*function)(L);
        } catch (const std::exception& error) {
            failed = true;
            copy_to_buffer(message, sizeof(message), error.what());
        } catch (...) {
            failed = true;
            copy_to_buffer(message, sizeof(message), "unknown C++ exception in runtime bridge");
        }
    }
    if (failed) {
        lua_pushstring(L, message);
        return lua_error(L);
    }
    return results;
}

// Sandboxed `require` implementation. All state is held by LuaJITHost (the
// upvalue is a lightuserdata pointer to the host), so no C++ object with a
// non-trivial destructor is alive when lua_error() longjmps.
int module_require_closure(lua_State* L) {
    auto* host = static_cast<LuaJITHost*>(lua_touserdata(L, lua_upvalueindex(1)));
    std::size_t name_len = 0;
    const char* requested = lua_tolstring(L, 1, &name_len);
    if (host == nullptr || requested == nullptr || name_len == 0) {
        lua_pushliteral(L, "require: module name required");
        return lua_error(L);
    }
    char name[256];
    const std::size_t copied = name_len < sizeof(name) - 1 ? name_len : sizeof(name) - 1;
    std::memcpy(name, requested, copied);
    name[copied] = '\0';

    // Already loaded?
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaded");
    lua_getfield(L, -1, name);
    if (!lua_isnil(L, -1)) {
        return 1;
    }
    lua_pop(L, 3);

    if (!host->read_module_source(name, host->module_buffer())) {
        lua_pushfstring(L, "module '%s' not found in sandbox", name);
        return lua_error(L);
    }

    char chunk[320];
    std::snprintf(chunk, sizeof(chunk), "@module/%s", name);
    const int load_status = luaL_loadbuffer(L, host->module_buffer().data(),
                                            host->module_buffer().size(), chunk);
    if (load_status != 0) {
        return lua_error(L);
    }
    lua_pushstring(L, name);
    const int call_status = lua_pcall(L, 1, 1, 0);
    if (call_status != 0) {
        return lua_error(L);
    }

    // Cache the module value in package.loaded; leave it on top as the result.
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaded");
    lua_pushvalue(L, -3);
    lua_setfield(L, -2, name);
    lua_pop(L, 2);
    return 1;
}

} // namespace

LuaJITHost::LuaJITHost() : LuaJITHost(LuaLibraryPolicy{}) {}

LuaJITHost::LuaJITHost(const LuaLibraryPolicy& policy) : policy_(policy) {
    // Minimal public Lua<->C++ bridge, installed into every new Lua state.
    register_function("runtime.version", [](lua_State* L) -> int {
        lua_pushstring(L, LUAJIT_VERSION);
        return 1;
    });
    register_function("runtime.ping", [](lua_State* L) -> int {
        lua_pushliteral(L, "pong");
        return 1;
    });
    register_function("runtime.echo", [](lua_State* L) -> int {
        if (lua_gettop(L) < 1) {
            lua_pushnil(L);
        } else {
            lua_pushvalue(L, 1);
        }
        return 1;
    });
}

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

        // Centralized library policy. `luaL_openlibs()` is intentionally never
        // called: only the entries below can be opened, each at most once.
        // `coroutine` is part of luaopen_base in LuaJIT (lib_base.c:698-706).
        struct LibraryEntry {
            const char* name;
            lua_CFunction opener;
            bool enabled;
        };
        const LibraryEntry libraries[] = {
            { "", luaopen_base, policy_.base },
            { LUA_MATHLIBNAME, luaopen_math, policy_.math },
            { LUA_STRLIBNAME, luaopen_string, policy_.string },
            { LUA_TABLIBNAME, luaopen_table, policy_.table },
            { LUA_LOADLIBNAME, luaopen_package, policy_.package },
            { LUA_FFILIBNAME, luaopen_ffi, policy_.ffi },
            { LUA_JITLIBNAME, luaopen_jit, policy_.jit },
            { "bit", luaopen_bit, policy_.bit },
            { "io", luaopen_io, policy_.io },
            { "os", luaopen_os, policy_.os },
            { "debug", luaopen_debug, policy_.debug },
        };

        auto open_lib = [&](const LibraryEntry& entry) -> bool {
            lua_pushcfunction(state_, entry.opener);
            lua_pushstring(state_, entry.name);
            if (lua_pcall(state_, 1, 0, 0) != 0) {
                copy_error_message(state_, last_error_);
                return false;
            }
            return true;
        };

        for (const LibraryEntry& entry : libraries) {
            if (!entry.enabled) continue;
            if (!open_lib(entry)) {
                lua_close(state_);
                state_ = nullptr;
                initialized_ = false;
                return false;
            }
        }

        install_bridge();
        if (module_reader_) {
            lua_pushlightuserdata(state_, this);
            lua_pushcclosure(state_, module_require_closure, 1);
            lua_setglobal(state_, "require");
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

bool LuaJITHost::register_bridge_entry(const char* full_name, LuaBridgeFunction& function) {
    if (state_ == nullptr || full_name == nullptr || *full_name == '\0') return false;

    const int top = lua_gettop(state_);
    std::string name(full_name);
    const std::size_t dot = name.rfind('.');
    if (dot == std::string::npos) {
        lua_pushlightuserdata(state_, &function);
        lua_pushcclosure(state_, bridge_trampoline, 1);
        lua_setglobal(state_, name.c_str());
    } else {
        const std::string table_name = name.substr(0, dot);
        const std::string field_name = name.substr(dot + 1);
        lua_getglobal(state_, table_name.c_str());
        if (!lua_istable(state_, -1)) {
            lua_pop(state_, 1);
            lua_newtable(state_);
            lua_pushvalue(state_, -1);
            lua_setglobal(state_, table_name.c_str());
        }
        lua_pushlightuserdata(state_, &function);
        lua_pushcclosure(state_, bridge_trampoline, 1);
        lua_setfield(state_, -2, field_name.c_str());
        lua_pop(state_, 1);
    }
    lua_settop(state_, top);
    return true;
}

void LuaJITHost::install_bridge() {
    for (auto& entry : bridge_functions_) {
        register_bridge_entry(entry.first.c_str(), entry.second);
    }
}

bool LuaJITHost::register_function(const char* name, LuaBridgeFunction function) {
    if (name == nullptr || *name == '\0') return false;
    const std::string key(name);
    if (bridge_functions_.find(key) != bridge_functions_.end()) return false;

    const auto inserted = bridge_functions_.emplace(key, std::move(function));
    if (!inserted.second) return false;
    if (state_ != nullptr) {
        register_bridge_entry(key.c_str(), inserted.first->second);
    }
    return true;
}

void LuaJITHost::set_module_source_reader(ModuleSourceReader reader) noexcept {
    module_reader_ = std::move(reader);
}

bool LuaJITHost::read_module_source(const std::string& name, std::string& out) const noexcept {
    if (!module_reader_) return false;
    return module_reader_(name, out);
}

LuaExecutionResult LuaJITHost::execute_detailed(std::string_view source,
                                                std::string_view chunk_name) {
    LuaExecutionResult result;
    result.status = LuaExecutionStatus::InvalidState;
    result.message = "runtime not initialized";
    result.stack_top_after = stack_top();

    if (!initialized_ || state_ == nullptr) {
        last_error_ = result.message;
        return result;
    }

    last_error_.clear();
    std::string name(chunk_name);
    if (name.empty()) {
        name = "chunk";
    }
    std::string code(source);
    const int top_before = lua_gettop(state_);

    try {
        // Both luaL_loadbuffer and lua_pcall are protected Lua C API entry
        // points: Lua errors inside the chunk become non-zero return codes.
        const int load_status = luaL_loadbuffer(state_, code.c_str(), code.size(), name.c_str());
        if (load_status != 0) {
            result.status = (load_status == LUA_ERRSYNTAX)
                          ? LuaExecutionStatus::SyntaxError
                          : (load_status == LUA_ERRMEM)
                          ? LuaExecutionStatus::InternalError
                          : LuaExecutionStatus::RuntimeError;
            copy_error_message(state_, result.message);
        } else {
            const int call_status = lua_pcall(state_, 0, 1, 0);
            if (call_status != 0) {
                result.status = (call_status == LUA_ERRRUN || call_status == LUA_ERRERR)
                              ? LuaExecutionStatus::RuntimeError
                              : (call_status == LUA_ERRMEM)
                              ? LuaExecutionStatus::InternalError
                              : LuaExecutionStatus::RuntimeError;
                if (result.status == LuaExecutionStatus::RuntimeError) {
                    const char* message = lua_tostring(state_, -1);
                    luaL_traceback(state_, state_, message ? message : "runtime error", 1);
                }
                copy_error_message(state_, result.message);
            } else {
                result.status = LuaExecutionStatus::Success;
                result.message.clear();
            }
        }
    } catch (const std::exception& e) {
        result.status = LuaExecutionStatus::InternalError;
        result.message = std::string("C++ exception during execute: ") + e.what();
    } catch (...) {
        result.status = LuaExecutionStatus::InternalError;
        result.message = "unknown C++ exception during execute";
    }

    // Restore the exact stack height that existed before this call, including
    // on every error path.
    lua_settop(state_, top_before);
    result.stack_top_after = lua_gettop(state_);
    last_error_ = (result.status == LuaExecutionStatus::Success) ? std::string() : result.message;
    return result;
}

bool LuaJITHost::execute(std::string_view source, std::string_view chunk_name) {
    return execute_detailed(source, chunk_name).status == LuaExecutionStatus::Success;
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
