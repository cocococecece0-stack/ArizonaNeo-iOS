#pragma once

#include "arizona_neo/core/event_bus.hpp"
#include "arizona_neo/core/filesystem.hpp"
#include "arizona_neo/core/script_manager.hpp"
#include "arizona_neo/core/scheduler.hpp"
#include "arizona_neo/runtime/lua_jit_host.hpp"

#include <cstdint>
#include <string>
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
    };

    NeoLoaderAPI(LuaJITHost& host,
                 core::EventBus& bus,
                 core::Scheduler& scheduler,
                 core::SandboxedFilesystem& fs);
    ~NeoLoaderAPI();

    NeoLoaderAPI(const NeoLoaderAPI&) = delete;
    NeoLoaderAPI& operator=(const NeoLoaderAPI&) = delete;

    bool install();

    const ScriptMeta& script_meta() const noexcept { return meta_; }

    // Optional non-owning ScriptManager binding used by the lifecycle queries.
    void set_script_manager(core::ScriptManager* manager) noexcept { manager_ = manager; }

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
    int l_event_subscribe(lua_State* L);
    int l_event_publish(lua_State* L);
    int l_event_count(lua_State* L);
    int l_schedule(lua_State* L);
    int l_tick(lua_State* L);
    int l_fs_read(lua_State* L);
    int l_fs_write(lua_State* L);
    int l_fs_exists(lua_State* L);
    int l_os_clock(lua_State* L);
    int l_os_time(lua_State* L);
    int l_script_state(lua_State* L);
    int l_active_scripts(lua_State* L);


    LuaJITHost& host_;
    core::EventBus& bus_;
    core::Scheduler& scheduler_;
    core::SandboxedFilesystem& fs_;
    core::ScriptManager* manager_ = nullptr;
    ScriptMeta meta_;
    std::vector<int> refs_;
    std::vector<core::EventBus::SubscriptionToken> tokens_;
    std::vector<core::Scheduler::TaskId> tasks_;
    bool installed_ = false;
};

} // namespace arizona_neo::runtime
