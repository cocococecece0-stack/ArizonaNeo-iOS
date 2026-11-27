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

// Cooperative MoonLoader-style thread runtime. It is intentionally written in
// Lua so the same implementation works on LuaJIT (Lua 5.1 API) and Lua 5.4.
// `neo_tick(delta)` advances this virtual clock after the C++ Scheduler tick.
// No native thread, timer or game callback is involved.
constexpr const char* kThreadRuntime = R"LUA(
local unpack_values = table.unpack or unpack
local threads = {}
local now_ms = 0
local next_thread_id = 1

local function current_thread()
    local co, is_main = coroutine.running()
    if co == nil or is_main then
        return nil
    end
    return co
end

function wait(ms)
    if not current_thread() then
        error('wait: must be called from a script thread', 2)
    end
    ms = tonumber(ms) or 0
    if ms < 0 then ms = 0 end
    return coroutine.yield(ms)
end

local function thread_status(handle)
    local id = type(handle) == 'table' and handle.id or nil
    local thread = id and threads[id] or nil
    if not thread or thread.dead then return 'dead' end
    if thread.running then return 'running' end
    return 'suspended'
end

local handle_mt = {
    __index = {
        status = function(self) return thread_status(self) end,
        error = function(self)
            local thread = self and threads[self.id] or nil
            return thread and thread.error or nil
        end,
    },
}

local function create_thread(fn, ...)
    if type(fn) ~= 'function' then
        error('lua_thread.create: function expected', 2)
    end
    local id = next_thread_id
    next_thread_id = next_thread_id + 1
    local handle = setmetatable({ id = id }, handle_mt)
    threads[id] = {
        co = coroutine.create(fn),
        wake = now_ms,
        args = { ... },
        nargs = select('#', ...),
        running = false,
        dead = false,
    }
    return handle
end

if type(lua_thread) ~= 'table' then
    lua_thread = {}
end
if type(lua_thread.create) ~= 'function' then
    lua_thread.create = create_thread
end

function __neo_thread_tick(delta)
    delta = tonumber(delta) or 0
    if delta < 0 then delta = 0 end
    now_ms = now_ms + delta

    local due = {}
    for id, thread in pairs(threads) do
        if not thread.dead and not thread.running and thread.wake <= now_ms then
            due[#due + 1] = id
        end
    end
    table.sort(due)

    for _, id in ipairs(due) do
        local thread = threads[id]
        if thread and not thread.dead and not thread.running then
            thread.running = true
            local args, nargs = thread.args or {}, thread.nargs or 0
            thread.args, thread.nargs = nil, 0
            local resume_ok, ok, result = pcall(function()
                return coroutine.resume(thread.co, unpack_values(args, 1, nargs))
            end)
            thread.running = false
            if not resume_ok then
                thread.dead = true
                thread.error = tostring(ok)
            elseif not ok then
                thread.dead = true
                thread.error = tostring(result)
            elseif coroutine.status(thread.co) == 'dead' then
                thread.dead = true
            else
                local delay = tonumber(result) or 0
                if delay < 0 then delay = 0 end
                thread.wake = now_ms + delay
            end
        end
    end
end

function __neo_run_main()
    if type(main) ~= 'function' then
        return false, 'main function not defined'
    end
    return lua_thread.create(main)
end
)LUA";

} // namespace

NeoLoaderAPI::NeoLoaderAPI(core::LuaHost& host,
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
        if (this_script_ref_ >= 0) {
            luaL_unref(L, LUA_REGISTRYINDEX, this_script_ref_);
            this_script_ref_ = -2;
        }
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

int NeoLoaderAPI::l_this_script(lua_State* L) {
    // Real scripts compare thisScript() by identity (for example
    // Market_Helper's onScriptTerminate handler), so the table is cached in
    // the registry and its metadata fields are refreshed on every call.
    if (this_script_ref_ >= 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, this_script_ref_);
    } else {
        lua_newtable(L);
        lua_pushvalue(L, -1);
        this_script_ref_ = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    lua_pushlstring(L, meta_.name.data(), meta_.name.size());
    lua_setfield(L, -2, "name");
    lua_pushlstring(L, meta_.author.data(), meta_.author.size());
    lua_setfield(L, -2, "author");
    lua_pushlstring(L, meta_.version.data(), meta_.version.size());
    lua_setfield(L, -2, "version");
    lua_pushlstring(L, meta_.description.data(), meta_.description.size());
    lua_setfield(L, -2, "description");
    lua_pushlstring(L, meta_.path.data(), meta_.path.size());
    lua_setfield(L, -2, "path");
    lua_pushnumber(L, static_cast<lua_Number>(meta_.version_number));
    lua_setfield(L, -2, "version_number");
    lua_newtable(L);
    for (std::size_t i = 0; i < meta_.properties.size(); ++i) {
        lua_pushinteger(L, static_cast<lua_Integer>(i + 1));
        lua_pushstring(L, meta_.properties[i].c_str());
        lua_settable(L, -3);
    }
    lua_setfield(L, -2, "properties");

    // Reuse the registered C closure so thisScript():reload() has one
    // implementation and reports an explicit failure when no host callback is
    // installed.
    lua_getglobal(L, "neo_script_reload");
    lua_setfield(L, -2, "reload");
    return 1;
}

int NeoLoaderAPI::l_script_reload(lua_State* L) {
    if (!reload_handler_) {
        lua_pushboolean(L, 0);
        lua_pushliteral(L, "reload unavailable: no script lifecycle callback");
        return 2;
    }
    if (reload_handler_()) {
        lua_pushboolean(L, 1);
        return 1;
    }
    lua_pushboolean(L, 0);
    lua_pushliteral(L, "reload callback failed");
    return 2;
}

int NeoLoaderAPI::l_run_main(lua_State* L) {
    const int base = lua_gettop(L);
    lua_getglobal(L, "main");
    if (!lua_isfunction(L, -1)) {
        lua_settop(L, base);
        lua_pushboolean(L, 0);
        lua_pushliteral(L, "main function not defined");
        return 2;
    }
    lua_getglobal(L, "lua_thread");
    if (!lua_istable(L, -1)) {
        lua_settop(L, base);
        lua_pushboolean(L, 0);
        lua_pushliteral(L, "lua_thread.create unavailable");
        return 2;
    }
    lua_getfield(L, -1, "create");
    if (!lua_isfunction(L, -1)) {
        lua_settop(L, base);
        lua_pushboolean(L, 0);
        lua_pushliteral(L, "lua_thread.create unavailable");
        return 2;
    }
    lua_pushvalue(L, base + 1);
    if (lua_pcall(L, 1, 1, 0) != 0) {
        const char* message = lua_tostring(L, -1);
        const std::string error = message != nullptr ? message : "lua_thread.create failed";
        lua_settop(L, base);
        lua_pushboolean(L, 0);
        lua_pushlstring(L, error.data(), error.size());
        return 2;
    }
    lua_pushvalue(L, -1);
    lua_replace(L, base + 1);
    lua_settop(L, base + 1);
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
    const auto step = std::chrono::milliseconds(delta < 0 ? 0 : delta);
    scheduler_.tick(step);

    // The Lua-side cooperative thread runtime advances on the same explicit
    // tick. It has no hidden timer and performs no game/native calls.
    lua_getglobal(L, "__neo_thread_tick");
    if (lua_isfunction(L, -1)) {
        lua_pushinteger(L, delta < 0 ? 0 : delta);
        if (lua_pcall(L, 1, 0, 0) != 0) {
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);
    }

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

int NeoLoaderAPI::l_fs_list(lua_State* L) {
    const char* path = arg_string(L, 1);
    lua_newtable(L);
    if (path == nullptr) return 1;
    const auto listed = fs_.list_directory(path);
    if (!listed.ok) return 1;
    for (std::size_t i = 0; i < listed.value.size(); ++i) {
        lua_pushinteger(L, static_cast<lua_Integer>(i + 1));
        lua_pushstring(L, listed.value[i].c_str());
        lua_settable(L, -3);
    }
    return 1;
}

int NeoLoaderAPI::l_fs_mkdir(lua_State* L) {
    const char* path = arg_string(L, 1);
    lua_pushboolean(L, path != nullptr && fs_.create_directories(path).ok ? 1 : 0);
    return 1;
}

int NeoLoaderAPI::l_does_file_exist(lua_State* L) {
    const char* path = arg_string(L, 1);
    lua_pushboolean(L, path != nullptr && fs_.read_file(path).ok ? 1 : 0);
    return 1;
}

int NeoLoaderAPI::l_does_directory_exist(lua_State* L) {
    const char* path = arg_string(L, 1);
    lua_pushboolean(L, path != nullptr && fs_.list_directory(path).ok ? 1 : 0);
    return 1;
}

int NeoLoaderAPI::l_create_directory(lua_State* L) {
    const char* path = arg_string(L, 1);
    lua_pushboolean(L, path != nullptr && fs_.create_directories(path).ok ? 1 : 0);
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

int NeoLoaderAPI::l_log(lua_State* L) {
    const char* message = arg_string(L, 1);
    if (message == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }
    std::string existing;
    const auto read = fs_.read_file("logs/neoloader.log");
    if (read.ok) existing = read.value;
    const auto result = fs_.write_file("logs/neoloader.log",
                                       existing + std::string(message) + "\n");
    lua_pushboolean(L, result.ok ? 1 : 0);
    return 1;
}

bool NeoLoaderAPI::run_main() {
    lua_State* L = host_.state();
    if (L == nullptr) return false;
    const int top = lua_gettop(L);
    lua_getglobal(L, "neo_run_main");
    if (!lua_isfunction(L, -1)) {
        lua_settop(L, top);
        return false;
    }
    if (lua_pcall(L, 0, 1, 0) != 0) {
        lua_settop(L, top);
        return false;
    }
    const bool started = lua_toboolean(L, -1) != 0;
    lua_settop(L, top);
    return started;
}

bool NeoLoaderAPI::install() {
    if (installed_ || host_.state() == nullptr) return false;

    // Install the cooperative Lua thread runtime before exposing thisScript.
    // This is pure Lua and works identically on LuaJIT and Lua 5.4.
    if (luaL_dostring(host_.state(), kThreadRuntime) != 0) {
        lua_pop(host_.state(), 1);
        return false;
    }

    host_.register_function("script_name", [this](lua_State* L) { return l_script_name(L); });
    host_.register_function("script_author", [this](lua_State* L) { return l_script_author(L); });
    host_.register_function("script_version", [this](lua_State* L) { return l_script_version(L); });
    host_.register_function("script_version_number", [this](lua_State* L) { return l_script_version_number(L); });
    host_.register_function("script_description", [this](lua_State* L) { return l_script_description(L); });
    host_.register_function("script_properties", [this](lua_State* L) { return l_script_properties(L); });

    host_.register_function("neo_meta", [this](lua_State* L) { return l_neo_meta(L); });
    host_.register_function("thisScript", [this](lua_State* L) { return l_this_script(L); });
    host_.register_function("neo_script_reload", [this](lua_State* L) { return l_script_reload(L); });
    host_.register_function("neo_run_main", [this](lua_State* L) { return l_run_main(L); });
    host_.register_function("neo_event_subscribe", [this](lua_State* L) { return l_event_subscribe(L); });
    host_.register_function("neo_event_publish", [this](lua_State* L) { return l_event_publish(L); });
    host_.register_function("neo_event_count", [this](lua_State* L) { return l_event_count(L); });
    host_.register_function("neo_schedule", [this](lua_State* L) { return l_schedule(L); });
    host_.register_function("neo_tick", [this](lua_State* L) { return l_tick(L); });
    host_.register_function("neo_fs_read", [this](lua_State* L) { return l_fs_read(L); });
    host_.register_function("neo_fs_write", [this](lua_State* L) { return l_fs_write(L); });
    host_.register_function("neo_fs_exists", [this](lua_State* L) { return l_fs_exists(L); });
    host_.register_function("neo_log", [this](lua_State* L) { return l_log(L); });
    host_.register_function("neo_fs_list", [this](lua_State* L) { return l_fs_list(L); });
    host_.register_function("neo_fs_mkdir", [this](lua_State* L) { return l_fs_mkdir(L); });
    host_.register_function("doesFileExist", [this](lua_State* L) { return l_does_file_exist(L); });
    host_.register_function("doesDirectoryExist", [this](lua_State* L) { return l_does_directory_exist(L); });
    host_.register_function("createDirectory", [this](lua_State* L) { return l_create_directory(L); });

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
