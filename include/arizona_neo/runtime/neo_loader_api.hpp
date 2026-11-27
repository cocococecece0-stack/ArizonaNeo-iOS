#pragma once

#include "arizona_neo/core/event_bus.hpp"
#include "arizona_neo/game/game_bridge.hpp"
#include "arizona_neo/core/filesystem.hpp"
#include "arizona_neo/core/script_manager.hpp"
#include "arizona_neo/core/scheduler.hpp"
#include "arizona_neo/core/lua_host.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

struct lua_State;

namespace arizona_neo::runtime {

// Portable NeoLoader compatibility layer. Bridges the existing core EventBus
// and Scheduler into the shared Lua state and exposes sandboxed file helpers
// plus script metadata functions used by original NeoLoader scripts.
//
// Ownership: non-owning references to host, bus, scheduler and filesystem.
// All referenced objects must outlive this instance. Not thread-safe: Lua
// access must happen on a single thread.
class NeoLoaderAPI {
public:
    struct ScriptMeta {
        std::string name;
        std::string author;
        std::string version;
        std::string description;
        std::int64_t version_number = 0;
        std::vector<std::string> properties;
        std::string path;
    };

    NeoLoaderAPI(core::LuaHost& host,
                 core::EventBus& bus,
                 core::Scheduler& scheduler,
                 core::SandboxedFilesystem& fs);
    ~NeoLoaderAPI();

    NeoLoaderAPI(const NeoLoaderAPI&) = delete;
    NeoLoaderAPI& operator=(const NeoLoaderAPI&) = delete;

    bool install();

    const ScriptMeta& script_meta() const noexcept { return meta_; }

    // Sets the source path reported by thisScript().path. The host owns path
    // discovery; an unset path is reported as an empty string, never invented.
    void set_script_path(std::string path) { meta_.path = std::move(path); }

    // Optional lifecycle callback used by thisScript():reload(). Without it,
    // reload returns false plus an explicit "unavailable" message; it is never
    // reported as a successful reload.
    void set_reload_handler(std::function<bool()> handler) {
        reload_handler_ = std::move(handler);
    }

    // Starts the global main() function in a cooperative script thread.
    // Returns false when main is absent or the thread runtime is unavailable.
    bool run_main();

    // Optional non-owning ScriptManager binding used by the lifecycle queries.
    void set_script_manager(core::ScriptManager* manager) noexcept { manager_ = manager; }

    // Optional non-owning GameBridge binding. When absent, game capabilities
    // are reported as unavailable and no native call is attempted.
    void set_game_bridge(game::GameBridge* bridge) noexcept { bridge_ = bridge; }

private:
    int ref_function(int index);
    void call_ref(int ref);

    int l_script_name(lua_State* L);
    int l_script_author(lua_State* L);
    int l_script_version(lua_State* L);
    int l_script_version_number(lua_State* L);
    int l_script_description(lua_State* L);
    int l_script_properties(lua_State* L);
    int l_neo_meta(lua_State* L);
    int l_this_script(lua_State* L);
    int l_script_reload(lua_State* L);
    int l_run_main(lua_State* L);
    int l_event_subscribe(lua_State* L);
    int l_event_publish(lua_State* L);
    int l_event_count(lua_State* L);
    int l_schedule(lua_State* L);
    int l_tick(lua_State* L);
    int l_fs_read(lua_State* L);
    int l_fs_write(lua_State* L);
    int l_fs_exists(lua_State* L);
    int l_log(lua_State* L);
    int l_fs_list(lua_State* L);
    int l_fs_mkdir(lua_State* L);
    int l_does_file_exist(lua_State* L);
    int l_does_directory_exist(lua_State* L);
    int l_create_directory(lua_State* L);
    int l_os_clock(lua_State* L);
    int l_os_time(lua_State* L);
    int l_script_state(lua_State* L);
    int l_active_scripts(lua_State* L);
    int l_game_ready(lua_State* L);
    int l_game_capabilities(lua_State* L);
    int l_game_pump(lua_State* L);
    int l_game_lifecycle(lua_State* L);


    core::LuaHost& host_;
    core::EventBus& bus_;
    core::Scheduler& scheduler_;
    core::SandboxedFilesystem& fs_;
    core::ScriptManager* manager_ = nullptr;
    game::GameBridge* bridge_ = nullptr;
    std::function<bool()> reload_handler_;
    ScriptMeta meta_;
    int this_script_ref_ = -2; // LUA_NOREF without requiring lauxlib in the header
    std::vector<int> refs_;
    std::vector<core::EventBus::SubscriptionToken> tokens_;
    std::vector<core::Scheduler::TaskId> tasks_;
    bool installed_ = false;
};

} // namespace arizona_neo::runtime
