#if defined(ARIZONA_NEO_WITH_LUAJIT)

#include "arizona_neo/runtime/neo_loader_api.hpp"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <chrono>

namespace arizona_neo::runtime {

namespace {

const char* arg_string(lua_State* L, int index) {
    std::size_t length = 0;
    const char* value = lua_tolstring(L, index, &length);
    return value != nullptr && length > 0 ? value : nullptr;
}

} // namespace

NeoLoaderAPI::NeoLoaderAPI(LuaJITHost& host,
                           core::EventBus& bus,
                           core::Scheduler& scheduler,
                           core::SandboxedFilesystem& fs)
    : host_(host), bus_(bus), scheduler_(scheduler), fs_(fs) {}

NeoLoaderAPI::~NeoLoaderAPI() {
    for (auto token : tokens_) bus_.unsubscribe(token);
    for (auto task : tasks_) scheduler_.cancel(task);
    lua_State* L = host_.state();
    if (L != nullptr) {
        for (int ref : refs_) luaL_unref(L, LUA_REGISTRYINDEX, ref);
    }
    tokens_.clear();
    tasks_.clear();
    refs_.clear();
}

int NeoLoaderAPI::ref_function(int index) {
    lua_State* L = host_.state();
    if (L == nullptr || !lua_isfunction(L, index)) return LUA_NOREF;
    lua_pushvalue(L, index);
    const int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    refs_.push_back(ref);
    return ref;
}

void NeoLoaderAPI::call_ref(int ref) {
    lua_State* L = host_.state();
    if (L == nullptr || ref == LUA_NOREF) return;
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    if (lua_pcall(L, 0, 0, 0) != 0) {
        lua_pop(L, 1);
    }
}

int NeoLoaderAPI::l_script_name(lua_State* L) {
    if (const char* value = arg_string(L, 1)) meta_.name = value;
    return 0;
}

int NeoLoaderAPI::l_script_author(lua_State* L) {
    if (const char* value = arg_string(L, 1)) meta_.author = value;
    return 0;
}

int NeoLoaderAPI::l_script_version(lua_State* L) {
    if (const char* value = arg_string(L, 1)) meta_.version = value;
    return 0;
}

int NeoLoaderAPI::l_script_version_number(lua_State* L) {
    meta_.version_number = static_cast<std::int64_t>(lua_tointeger(L, 1));
    return 0;
}

int NeoLoaderAPI::l_script_description(lua_State* L) {
    if (const char* value = arg_string(L, 1)) meta_.description = value;
    return 0;
}

int NeoLoaderAPI::l_script_properties(lua_State* L) {
    meta_.properties.clear();
    const int count = lua_gettop(L);
    for (int i = 1; i <= count; ++i) {
        if (const char* value = arg_string(L, i)) meta_.properties.emplace_back(value);
    }
    return 0;
}

int NeoLoaderAPI::l_neo_meta(lua_State* L) {
    lua_newtable(L);
    lua_pushstring(L, meta_.name.c_str());
    lua_setfield(L, -2, "name");
    lua_pushstring(L, meta_.author.c_str());
    lua_setfield(L, -2, "author");
    lua_pushstring(L, meta_.version.c_str());
    lua_setfield(L, -2, "version");
    lua_pushstring(L, meta_.description.c_str());
    lua_setfield(L, -2, "description");
    lua_pushnumber(L, static_cast<lua_Number>(meta_.version_number));
    lua_setfield(L, -2, "version_number");
    lua_newtable(L);
    for (std::size_t i = 0; i < meta_.properties.size(); ++i) {
        lua_pushinteger(L, static_cast<lua_Integer>(i + 1));
        lua_pushstring(L, meta_.properties[i].c_str());
        lua_settable(L, -3);
    }
    lua_setfield(L, -2, "properties");
    return 1;
}

int NeoLoaderAPI::l_event_subscribe(lua_State* L) {
    const char* name = arg_string(L, 1);
    if (name == nullptr || !lua_isfunction(L, 2)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    const int ref = ref_function(2);
    if (ref == LUA_NOREF) {
        lua_pushboolean(L, 0);
        return 1;
    }
    const auto token = bus_.subscribe(name, [this, ref](const std::string&) { call_ref(ref); });
    tokens_.push_back(token);
    lua_pushnumber(L, static_cast<lua_Number>(token.id));
    return 1;
}

int NeoLoaderAPI::l_event_publish(lua_State* L) {
    const char* name = arg_string(L, 1);
    if (name == nullptr) {
        lua_pushnumber(L, 0);
        return 1;
    }
    bus_.publish(name);
    lua_pushnumber(L, static_cast<lua_Number>(bus_.subscriber_count(name)));
    return 1;
}

int NeoLoaderAPI::l_event_count(lua_State* L) {
    const char* name = arg_string(L, 1);
    lua_pushnumber(L, name != nullptr
        ? static_cast<lua_Number>(bus_.subscriber_count(name)) : 0);
    return 1;
}

int NeoLoaderAPI::l_schedule(lua_State* L) {
    const lua_Integer delay = lua_tointeger(L, 1);
    if (delay < 0 || !lua_isfunction(L, 2)) {
        lua_pushnumber(L, 0);
        return 1;
    }
    const int ref = ref_function(2);
    if (ref == LUA_NOREF) {
        lua_pushnumber(L, 0);
        return 1;
    }
    const auto id = scheduler_.schedule_after(
        std::chrono::milliseconds(delay), [this, ref]() { call_ref(ref); });
    tasks_.push_back(id);
    lua_pushnumber(L, static_cast<lua_Number>(id));
    return 1;
}

int NeoLoaderAPI::l_tick(lua_State* L) {
    const lua_Integer delta = lua_tointeger(L, 1);
    scheduler_.tick(std::chrono::milliseconds(delta < 0 ? 0 : delta));
    lua_pushnumber(L, static_cast<lua_Number>(scheduler_.pending_count()));
    return 1;
}

int NeoLoaderAPI::l_fs_read(lua_State* L) {
    const char* path = arg_string(L, 1);
    if (path == nullptr) {
        lua_pushnil(L);
        lua_pushliteral(L, "path required");
        return 2;
    }
    const auto read = fs_.read_file(path);
    if (!read.ok) {
        lua_pushnil(L);
        lua_pushstring(L, read.message.c_str());
        return 2;
    }
    lua_pushlstring(L, read.value.data(), read.value.size());
    return 1;
}

int NeoLoaderAPI::l_fs_write(lua_State* L) {
    const char* path = arg_string(L, 1);
    std::size_t length = 0;
    const char* content = lua_tolstring(L, 2, &length);
    if (path == nullptr || content == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, fs_.write_file(path, std::string(content, length)).ok ? 1 : 0);
    return 1;
}

int NeoLoaderAPI::l_fs_exists(lua_State* L) {
    const char* path = arg_string(L, 1);
    lua_pushboolean(L, path != nullptr && fs_.read_file(path).ok ? 1 : 0);
    return 1;
}

int NeoLoaderAPI::l_os_clock(lua_State* L) {
    static const auto start = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    lua_pushnumber(L, static_cast<lua_Number>(elapsed));
    return 1;
}

int NeoLoaderAPI::l_os_time(lua_State* L) {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    lua_pushnumber(L, static_cast<lua_Number>(
        std::chrono::duration_cast<std::chrono::seconds>(now).count()));
    return 1;
}

int NeoLoaderAPI::l_script_state(lua_State* L) {
    const char* id = arg_string(L, 1);
    if (manager_ == nullptr || id == nullptr) {
        lua_pushnil(L);
        return 1;
    }
    const auto info = manager_->get(id);
    if (!info.ok) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushstring(L, arizona_neo::core::to_string(info.value.state));
    return 1;
}

int NeoLoaderAPI::l_active_scripts(lua_State* L) {
    lua_pushnumber(L, manager_ != nullptr
        ? static_cast<lua_Number>(manager_->active_count()) : 0);
    return 1;
}

int NeoLoaderAPI::l_game_ready(lua_State* L) {
    lua_pushboolean(L, bridge_ != nullptr && bridge_->ready() ? 1 : 0);
    return 1;
}

int NeoLoaderAPI::l_game_capabilities(lua_State* L) {
    lua_newtable(L);
    if (bridge_ == nullptr) return 1;
    for (const auto& entry : bridge_->capabilities().all()) {
        lua_pushstring(L, arizona_neo::game::to_string(entry.first));
        lua_pushstring(L, arizona_neo::game::to_string(entry.second.status));
        lua_settable(L, -3);
    }
    return 1;
}

int NeoLoaderAPI::l_game_pump(lua_State* L) {
    lua_pushnumber(L, bridge_ != nullptr
        ? static_cast<lua_Number>(bridge_->pump_all()) : 0);
    return 1;
}

int NeoLoaderAPI::l_game_lifecycle(lua_State* L) {
    lua_pushstring(L, bridge_ != nullptr
        ? bridge_->lifecycle_state().c_str() : "unavailable");
    return 1;
}

bool NeoLoaderAPI::install() {
    if (installed_ || host_.state() == nullptr) return false;

    host_.register_function("script_name", [this](lua_State* L) { return l_script_name(L); });
    host_.register_function("script_author", [this](lua_State* L) { return l_script_author(L); });
    host_.register_function("script_version", [this](lua_State* L) { return l_script_version(L); });
    host_.register_function("script_version_number", [this](lua_State* L) { return l_script_version_number(L); });
    host_.register_function("script_description", [this](lua_State* L) { return l_script_description(L); });
    host_.register_function("script_properties", [this](lua_State* L) { return l_script_properties(L); });

    host_.register_function("neo_meta", [this](lua_State* L) { return l_neo_meta(L); });
    host_.register_function("neo_event_subscribe", [this](lua_State* L) { return l_event_subscribe(L); });
    host_.register_function("neo_event_publish", [this](lua_State* L) { return l_event_publish(L); });
    host_.register_function("neo_event_count", [this](lua_State* L) { return l_event_count(L); });
    host_.register_function("neo_schedule", [this](lua_State* L) { return l_schedule(L); });
    host_.register_function("neo_tick", [this](lua_State* L) { return l_tick(L); });
    host_.register_function("neo_fs_read", [this](lua_State* L) { return l_fs_read(L); });
    host_.register_function("neo_fs_write", [this](lua_State* L) { return l_fs_write(L); });
    host_.register_function("neo_fs_exists", [this](lua_State* L) { return l_fs_exists(L); });

    // Restricted os subset: no execute/remove/rename, no environment access.
    host_.register_function("os.clock", [this](lua_State* L) { return l_os_clock(L); });
    host_.register_function("os.time", [this](lua_State* L) { return l_os_time(L); });
    host_.register_function("neo_script_state", [this](lua_State* L) { return l_script_state(L); });
    host_.register_function("neo_active_scripts", [this](lua_State* L) { return l_active_scripts(L); });
    host_.register_function("neo_game_ready", [this](lua_State* L) { return l_game_ready(L); });
    host_.register_function("neo_game_capabilities", [this](lua_State* L) { return l_game_capabilities(L); });
    host_.register_function("neo_game_pump", [this](lua_State* L) { return l_game_pump(L); });
    host_.register_function("neo_game_lifecycle", [this](lua_State* L) { return l_game_lifecycle(L); });

    installed_ = true;
    return true;
}

} // namespace arizona_neo::runtime

#endif // ARIZONA_NEO_WITH_LUAJIT
