#if defined(ARIZONA_NEO_WITH_LUA54)

#include "arizona_neo/runtime/lua54_host.hpp"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <cstdio>
#include <cstring>
#include <exception>
#include <cstring>
#include <map>
#include <string>
#include <utility>

namespace arizona_neo::runtime {

const char* to_string(LuaExecutionStatus status) noexcept {
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

int protected_getglobal(lua_State* L) {
    const char* name = static_cast<const char*>(lua_touserdata(L, 1));
    lua_getglobal(L, name);
    return 1;
}

void copy_error_message(lua_State* L, std::string& out) {
    out.clear();
    const int type = lua_type(L, -1);
    if (type == LUA_TSTRING || type == LUA_TNUMBER) {
        std::size_t len = 0;
        const char* msg = lua_tolstring(L, -1, &len);
        if (msg != nullptr) out.assign(msg, len);
    }
    if (out.empty()) out = "Lua error (non-string error object)";
    lua_settop(L, 0);
}

int bridge_trampoline(lua_State* L) {
    auto* function = static_cast<LuaBridgeFunction*>(lua_touserdata(L, lua_upvalueindex(1)));
    char message[512];
    message[0] = '\0';
    int results = 0;
    bool failed = false;
    if (function == nullptr || !(*function)) {
        failed = true;
        std::snprintf(message, sizeof(message), "runtime bridge: invalid function");
    } else {
        try { results = (*function)(L); }
        catch (const std::exception& error) {
            failed = true;
            std::snprintf(message, sizeof(message), "%s", error.what());
        } catch (...) {
            failed = true;
            std::snprintf(message, sizeof(message), "unknown C++ exception in runtime bridge");
        }
    }
    if (failed) {
        lua_pushstring(L, message);
        return lua_error(L);
    }
    return results;
}

} // namespace

Lua54Host::Lua54Host() : Lua54Host(LuaLibraryPolicy{}) {}

Lua54Host::Lua54Host(const LuaLibraryPolicy& policy) : policy_(policy) {
    register_function("runtime.version", [](lua_State* L) -> int {
        lua_pushstring(L, "Lua 5.4");
        return 1;
    });
    register_function("runtime.ping", [](lua_State* L) -> int {
        lua_pushliteral(L, "pong");
        return 1;
    });
}

Lua54Host::~Lua54Host() { shutdown(); }

bool Lua54Host::initialize() {
    if (initialized_) return true;
    last_error_.clear();
    try {
        state_ = luaL_newstate();
        if (state_ == nullptr) {
            last_error_ = "luaL_newstate returned nullptr";
            return false;
        }
        struct LibraryEntry {
            const char* name;
            lua_CFunction opener;
            bool enabled;
        };
        const LibraryEntry libraries[] = {
            {"", luaopen_base, policy_.base},
            {LUA_MATHLIBNAME, luaopen_math, policy_.math},
            {LUA_STRLIBNAME, luaopen_string, policy_.string},
            {LUA_TABLIBNAME, luaopen_table, policy_.table},
            {LUA_LOADLIBNAME, luaopen_package, policy_.package},
            {"coroutine", luaopen_coroutine, true},
            {"utf8", luaopen_utf8, true},
            {"io", luaopen_io, policy_.io},
            {"os", luaopen_os, policy_.os},
            {"debug", luaopen_debug, policy_.debug},
        };
        auto open_lib = [&](const LibraryEntry& entry) -> bool {
            const char* name = entry.name[0] == '\0' ? LUA_GNAME : entry.name;
            luaL_requiref(state_, name, entry.opener, 1);
            lua_pop(state_, 1);
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
        // Lua 5.2+ removed globals that older NeoLoader modules still use.
        // Lua 5.2+ removed globals that older NeoLoader modules still use.
        luaL_dostring(state_, "if type(table) == 'table' and type(table.unpack) == 'function' then unpack = table.unpack end");
        luaL_dostring(state_, "if type(load) == 'function' then loadstring = load end");

        install_bridge();
        if (module_reader_) {
            set_module_source_reader(module_reader_);
        }
        initialized_ = true;
        return true;
    } catch (const std::exception& e) {
        last_error_ = e.what();
        if (state_ != nullptr) { lua_close(state_); state_ = nullptr; }
        initialized_ = false;
        return false;
    } catch (...) {
        last_error_ = "unknown exception during initialize";
        if (state_ != nullptr) { lua_close(state_); state_ = nullptr; }
        initialized_ = false;
        return false;
    }
}

bool Lua54Host::register_bridge_entry(const char* full_name, LuaBridgeFunction& function) {
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

void Lua54Host::install_bridge() {
    for (auto& entry : bridge_functions_) {
        register_bridge_entry(entry.first.c_str(), entry.second);
    }
}

bool Lua54Host::read_module_source(const std::string& name, std::string& out) const noexcept {
    if (!module_reader_) return false;
    return module_reader_(name, out);
}

void Lua54Host::set_module_source_reader(core::LuaHost::ModuleSourceReader reader) {
    module_reader_ = std::move(reader);
    if (state_ != nullptr && module_reader_) {
        register_function("require", [this](lua_State* L) -> int {
            const char* requested = lua_tostring(L, 1);
            if (requested == nullptr) {
                lua_pushliteral(L, "module name required");
                return lua_error(L);
            }
            std::string name(requested);
            lua_getglobal(L, "package");
            lua_getfield(L, -1, "loaded");
            lua_getfield(L, -1, name.c_str());
            if (!lua_isnil(L, -1)) return 1;
            lua_pop(L, 3);
            if (!read_module_source(name, module_buffer())) {
                lua_pushfstring(L, "module '%s' not found in sandbox", name.c_str());
                return lua_error(L);
            }
            const int load_status = luaL_loadbuffer(L, module_buffer().data(),
                                                    module_buffer().size(),
                                                    ("@module/" + name).c_str());
            if (load_status != 0) return lua_error(L);
            lua_pushstring(L, name.c_str());
            if (lua_pcall(L, 1, 1, 0) != 0) return lua_error(L);
            lua_getglobal(L, "package");
            lua_getfield(L, -1, "loaded");
            lua_pushvalue(L, -3);
            lua_setfield(L, -2, name.c_str());
            lua_pop(L, 2);
            return 1;
        });
    }
}

bool Lua54Host::register_function(const char* name, LuaBridgeFunction function) {
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

LuaExecutionResult Lua54Host::execute_detailed(std::string_view source, std::string_view chunk_name) {
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
    if (name.empty()) name = "chunk";
    std::string code(source);
    const int top_before = lua_gettop(state_);
    try {
        const int load_status = luaL_loadbuffer(state_, code.c_str(), code.size(), name.c_str());
        if (load_status != 0) {
            result.status = (load_status == LUA_ERRSYNTAX) ? LuaExecutionStatus::SyntaxError
                : (load_status == LUA_ERRMEM) ? LuaExecutionStatus::InternalError
                : LuaExecutionStatus::RuntimeError;
            copy_error_message(state_, result.message);
        } else {
            const int call_status = lua_pcall(state_, 0, 1, 0);
            if (call_status != 0) {
                result.status = (call_status == LUA_ERRRUN || call_status == LUA_ERRERR)
                    ? LuaExecutionStatus::RuntimeError
                    : (call_status == LUA_ERRMEM) ? LuaExecutionStatus::InternalError
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
    lua_settop(state_, top_before);
    result.stack_top_after = lua_gettop(state_);
    last_error_ = (result.status == LuaExecutionStatus::Success) ? std::string() : result.message;
    return result;
}

bool Lua54Host::execute(std::string_view source, std::string_view chunk_name) {
    return execute_detailed(source, chunk_name).status == LuaExecutionStatus::Success;
}

void Lua54Host::shutdown() noexcept {
    if (state_ != nullptr) {
        lua_close(state_);
        state_ = nullptr;
    }
    initialized_ = false;
}

int Lua54Host::stack_top() const noexcept {
    return state_ != nullptr ? lua_gettop(state_) : 0;
}

double Lua54Host::get_global_number(const char* name) const {
    if (state_ == nullptr || name == nullptr) return 0.0;
    lua_pushcfunction(state_, protected_getglobal);
    lua_pushlightuserdata(state_, const_cast<char*>(name));
    if (lua_pcall(state_, 1, 1, 0) != 0) {
        lua_settop(state_, 0);
        return 0.0;
    }
    double value = 0.0;
    if (lua_isnumber(state_, -1)) value = lua_tonumber(state_, -1);
    lua_settop(state_, 0);
    return value;
}

std::string Lua54Host::get_global_string(const char* name) const {
    if (state_ == nullptr || name == nullptr) return {};
    lua_pushcfunction(state_, protected_getglobal);
    lua_pushlightuserdata(state_, const_cast<char*>(name));
    if (lua_pcall(state_, 1, 1, 0) != 0) {
        lua_settop(state_, 0);
        return {};
    }
    std::string value;
    if (lua_isstring(state_, -1)) {
        std::size_t len = 0;
        const char* text = lua_tolstring(state_, -1, &len);
        if (text != nullptr) value.assign(text, len);
    }
    lua_settop(state_, 0);
    return value;
}

} // namespace arizona_neo::runtime

#endif // ARIZONA_NEO_WITH_LUA54
