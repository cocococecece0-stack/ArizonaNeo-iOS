#include "arizona_neo/core/event_bus.hpp"
#include "arizona_neo/core/filesystem.hpp"
#include "arizona_neo/core/scheduler.hpp"
#include "arizona_neo/runtime/lua54_host.hpp"
#include "arizona_neo/runtime/module_search_paths.hpp"
#include "arizona_neo/runtime/neo_loader_api.hpp"
#include "arizona_neo/runtime/portable_script_loader.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << (msg) << "\n"; ++failures; } } while (0)

std::string read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}

} // namespace

int main() {
    using arizona_neo::runtime::Lua54Host;
    using arizona_neo::runtime::LuaExecutionStatus;

    arizona_neo::runtime::LuaLibraryPolicy policy;
    policy.bit = false; // Lua 5.4 uses operators, not the bit library.
    Lua54Host host(policy);
    CHECK(host.initialize(), "Lua 5.4 initialize");
    CHECK(host.is_initialized(), "Lua 5.4 initialized flag");
    CHECK(host.execute_detailed("assert(runtime.ping() == 'pong')", "ping").status ==
          LuaExecutionStatus::Success, "runtime bridge works");
    CHECK(host.stack_top() == 0, "stack clean after ping");

    // PortableScriptLoader works with the generic LuaHost interface.
    const auto lua54_root = std::filesystem::temp_directory_path() / "arizona_neo_lua54_runtime";
    std::error_code cleanup_error;
    std::filesystem::remove_all(lua54_root, cleanup_error);
    std::filesystem::create_directories(lua54_root, cleanup_error);
    arizona_neo::core::SandboxedFilesystem lua54_fs(lua54_root, true);
    std::ifstream heap_file("scripts/neo/modules/binaryheap.lua", std::ios::binary);
    std::string heap_source((std::istreambuf_iterator<char>(heap_file)),
                            std::istreambuf_iterator<char>());
    CHECK(lua54_fs.write_file("modules/binaryheap.lua", heap_source).ok, "write binaryheap module");
    Lua54Host loader_host(policy);
    loader_host.set_module_source_reader(
        [&lua54_fs](const std::string& requested, std::string& out) -> bool {
            for (const auto& candidate : arizona_neo::runtime::module_candidates(requested)) {
                const auto read = lua54_fs.read_file(candidate);
                if (!read.ok) continue;
                out = std::move(read.value);
                return true;
            }
            return false;
        });
    CHECK(loader_host.initialize(), "Lua 5.4 loader host initialize");
    arizona_neo::runtime::PortableScriptLoader loader(loader_host, lua54_fs);
    CHECK(loader.load("heap", "modules/binaryheap.lua").ok, "PortableScriptLoader loads under Lua 5.4");
    CHECK(loader.execute("heap").ok, "PortableScriptLoader executes under Lua 5.4");
    CHECK(loader_host.stack_top() == 0, "stack clean after PortableScriptLoader");

    // Backend-neutral NeoLoader API and the portable samp.events registry must
    // work under Lua 5.4, not only under LuaJIT.
    std::ifstream events_file("scripts/neo/modules/samp/events.lua", std::ios::binary);
    std::string events_source((std::istreambuf_iterator<char>(events_file)),
                              std::istreambuf_iterator<char>());
    CHECK(!events_source.empty(), "samp.events module readable");
    CHECK(lua54_fs.write_file("modules/samp/events.lua", events_source).ok,
          "write samp.events module");
    std::ifstream sampfuncs_file("scripts/neo/modules/sampfuncs.lua", std::ios::binary);
    std::string sampfuncs_source((std::istreambuf_iterator<char>(sampfuncs_file)),
                                 std::istreambuf_iterator<char>());
    CHECK(!sampfuncs_source.empty(), "sampfuncs module readable");
    CHECK(lua54_fs.write_file("modules/sampfuncs.lua", sampfuncs_source).ok,
          "write sampfuncs module");
    for (const char* module : {"moonloader", "vector3d", "matrix3x3"}) {
        const std::string source = read_file(std::string("scripts/neo/modules/") + module + ".lua");
        CHECK(!source.empty(), (std::string("read Lua 5.4 module ") + module).c_str());
        CHECK(lua54_fs.write_file(std::string("modules/") + module + ".lua", source).ok,
              (std::string("write Lua 5.4 module ") + module).c_str());
    }
    const std::string arzmod_source = read_file("tests/fixtures/stage15_2/arzmod.lua");
    CHECK(!arzmod_source.empty(), "real arzmod.lua readable for Lua 5.4");
    CHECK(lua54_fs.write_file("scripts/arzmod.lua", arzmod_source).ok,
          "write real arzmod.lua for Lua 5.4");
    const std::string helper_source =
        read_file("tests/fixtures/stage15_2/Helper_for_earning_money.lua");
    CHECK(!helper_source.empty(), "real Helper_for_earning_money.lua readable for Lua 5.4");
    CHECK(lua54_fs.write_file("scripts/Helper_for_earning_money.lua", helper_source).ok,
          "write real Helper_for_earning_money.lua for Lua 5.4");

    const char* consumer =
        "script_name('stage15_2_events_consumer')\n"
        "script_version('1.0')\n"
        "local sampev = require('lib.samp.events')\n"
        "function sampev.onServerMessage(color, text)\n"
        "  events_last_color = color\n"
        "  events_last_text = text\n"
        "  return false\n"
        "end\n"
        "function sampev.onShowDialog(dialogId)\n"
        "  events_last_dialog = dialogId\n"
        "end\n"
        "events_consumer_ready = (type(sampev.onServerMessage) == 'function') and 1 or 0\n";
    CHECK(lua54_fs.write_file("scripts/events_consumer.lua", consumer).ok,
          "write events consumer");

    Lua54Host api_host(policy);
    api_host.set_module_source_reader(
        [&lua54_fs](const std::string& requested, std::string& out) -> bool {
            std::string name = requested;
            if (name.rfind("lib.", 0) == 0) name = name.substr(4);
            for (char& character : name) if (character == '.') character = '/';
            auto read = lua54_fs.read_file("modules/" + name + ".lua");
            if (!read.ok) read = lua54_fs.read_file("modules/" + name + "/init.lua");
            if (!read.ok) return false;
            out = std::move(read.value);
            return true;
        });
    CHECK(api_host.initialize(), "Lua 5.4 API host initialize");
    {
        arizona_neo::core::EventBus bus;
        arizona_neo::core::Scheduler scheduler;
        arizona_neo::runtime::NeoLoaderAPI api(api_host, bus, scheduler, lua54_fs);
        CHECK(api.install(), "NeoLoaderAPI installs under Lua 5.4");

        arizona_neo::runtime::PortableScriptLoader api_loader(api_host, lua54_fs);
        CHECK(api_loader.load("events_consumer", "scripts/events_consumer.lua").ok,
              "samp.events consumer loads under Lua 5.4");
        CHECK(api.script_meta().name == "stage15_2_events_consumer",
              "script metadata captured under Lua 5.4");

        CHECK(api_host.execute_detailed(
            "local e = require('samp.events')\n"
            "events_cached = (e == require('lib.samp.events')) and 1 or 0\n"
            "events_dispatch_false = (e.dispatch('onServerMessage', 0x1234, 'hello') == false) and 1 or 0\n"
            "events_removed = e.remove_handler('onServerMessage') and not e.has_handler('onServerMessage') and 1 or 0\n"
            "local sf = require('sampfuncs')\n"
            "events_constants = (sf.HIGH_PRIORITY == 1 and sf.UNRELIABLE_SEQUENCED == 7 "
            "and HIGH_PRIORITY == 1 and MAX_PLAYERS == 1004) and 1 or 0\n",
            "verify_samp_events_lua54").status == LuaExecutionStatus::Success,
            "samp.events checks execute under Lua 5.4");
        CHECK(api_host.get_global_number("events_consumer_ready") == 1.0,
              "handler registered by original field-assignment style");
        CHECK(api_host.get_global_number("events_last_color") == 4660.0,
              "dispatch passes color argument");
        CHECK(api_host.get_global_string("events_last_text") == "hello",
              "dispatch passes text argument");
        CHECK(api_host.get_global_number("events_cached") == 1.0,
              "samp.events and lib.samp.events share one module");
        CHECK(api_host.get_global_number("events_dispatch_false") == 1.0,
              "dispatch returns handler result");
        CHECK(api_host.get_global_number("events_removed") == 1.0,
              "remove_handler removes field-assignment handler");
        CHECK(api_host.get_global_number("events_constants") == 1.0,
              "sampfuncs constants are portable under Lua 5.4");

        // thisScript lifecycle, safe filesystem globals and wait/lua_thread
        // must work on the Lua 5.4 backend too.
        CHECK(api_host.execute_detailed(
            "script_name('stage15_2r_lua54')\n"
            "script_author('Neo')\n"
            "script_version('2.0')\n"
            "script_version_number(42)\n"
            "local s = thisScript()\n"
            "lua54_this_script = (s.name == 'stage15_2r_lua54' and s.version == '2.0' "
            "and s.version_number == 42) and 1 or 0\n"
            "lua54_this_identity = (s == thisScript()) and 1 or 0\n"
            "local ok, err = s:reload()\n"
            "lua54_reload_default = (ok == false and type(err) == 'string' "
            "and err:find('unavailable', 1, true) ~= nil) and 1 or 0\n",
            "verify_this_script_lua54").status == LuaExecutionStatus::Success,
            "thisScript checks execute under Lua 5.4");
        CHECK(api_host.get_global_number("lua54_this_script") == 1.0,
              "thisScript metadata works under Lua 5.4");
        CHECK(api_host.get_global_number("lua54_this_identity") == 1.0,
              "thisScript identity is stable under Lua 5.4");
        CHECK(api_host.get_global_number("lua54_reload_default") == 1.0,
              "thisScript default reload fails explicitly under Lua 5.4");

        api.set_script_path("scripts/lua54_lifecycle.lua");
        int lua54_reload_calls = 0;
        api.set_reload_handler([&lua54_reload_calls]() {
            ++lua54_reload_calls;
            return true;
        });
        CHECK(api_host.execute_detailed(
            "lua54_this_path = (thisScript().path == 'scripts/lua54_lifecycle.lua') and 1 or 0\n"
            "lua54_reload_ok = thisScript():reload() and 1 or 0\n"
            "lua54_wait_outside = pcall(wait, 0)\n"
            "main = function()\n"
            "  lua54_wait_marker = 1\n"
            "  wait(10)\n"
            "  lua54_wait_marker = 2\n"
            "end\n",
            "verify_lifecycle_lua54").status == LuaExecutionStatus::Success,
            "Lua 5.4 lifecycle setup executes");
        CHECK(api_host.get_global_number("lua54_this_path") == 1.0,
              "thisScript path works under Lua 5.4");
        CHECK(api_host.get_global_number("lua54_reload_ok") == 1.0,
              "thisScript reload hook works under Lua 5.4");
        CHECK(lua54_reload_calls == 1, "Lua 5.4 reload callback invoked once");
        CHECK(api_host.get_global_number("lua54_wait_outside") == 0.0,
              "wait outside a script thread is rejected under Lua 5.4");
        CHECK(api.run_main(), "run_main starts under Lua 5.4");
        CHECK(api_host.execute_detailed("neo_tick(0)", "lua54_tick_0").status ==
              LuaExecutionStatus::Success, "Lua 5.4 neo_tick executes");
        CHECK(api_host.get_global_number("lua54_wait_marker") == 1.0,
              "Lua 5.4 main runs until wait");
        CHECK(api_host.execute_detailed("neo_tick(10)", "lua54_tick_10").status ==
              LuaExecutionStatus::Success, "Lua 5.4 neo_tick advances time");
        CHECK(api_host.get_global_number("lua54_wait_marker") == 2.0,
              "Lua 5.4 wait resumes main");

        // The same two real scripts must load through the generic LuaHost
        // interface under Lua 5.4, not only under LuaJIT.
        CHECK(api_host.execute_detailed(
            "PLAYER_PED = 7\n"
            "sampGetPlayerIdByCharHandle = function(handle) return false, nil end\n"
            "sampGetCharHandleBySampPlayerId = function(playerId) return false, nil end\n"
            "sampGetPlayerNickname = function(playerId) return 'orig' .. tostring(playerId) end\n",
            "install_samp_doubles_lua54").status == LuaExecutionStatus::Success,
            "install Lua 5.4 SAMP test doubles");
        CHECK(api_loader.load("arzmod", "scripts/arzmod.lua").ok,
              "real arzmod.lua loads under Lua 5.4");
        CHECK(api_host.execute_detailed(
            "local e = require('lib.samp.events')\n"
            "e.dispatch('onInitGame', 3)\n"
            "e.dispatch('onSetPlayerName', 3, 'Neo_Player', true)\n"
            "local ok1, id = sampGetPlayerIdByCharHandle(PLAYER_PED)\n"
            "local ok2, handle = sampGetCharHandleBySampPlayerId(3)\n"
            "local name = sampGetPlayerNickname(3)\n"
            "arzmod_lua54 = (ok1 and id == 3 and ok2 and handle == PLAYER_PED "
            "and name == 'Neo_Player') and 1 or 0\n",
            "verify_real_arzmod_lua54").status == LuaExecutionStatus::Success,
            "real arzmod.lua wrappers execute under Lua 5.4");
        CHECK(api_host.get_global_number("arzmod_lua54") == 1.0,
              "real arzmod.lua behaves as written under Lua 5.4");

        CHECK(api_loader.load("helper", "scripts/Helper_for_earning_money.lua").ok,
              "real Helper_for_earning_money.lua loads under Lua 5.4");
        CHECK(api_host.execute_detailed(
            "local e = require('lib.samp.events')\n"
            "lua54_helper_dialog = nil\n"
            "lua54_helper_chat = 0\n"
            "sampShowDialog = function(id) lua54_helper_dialog = id end\n"
            "sampAddChatMessage = function() lua54_helper_chat = lua54_helper_chat + 1 end\n"
            "cmdResetStat()\n"
            "cmdZarabotok()\n"
            "helper_lua54 = (type(main) == 'function' and e.has_handler('onServerMessage') "
            "and lua54_helper_chat == 1 and lua54_helper_dialog == 8888) and 1 or 0\n",
            "verify_real_helper_lua54").status == LuaExecutionStatus::Success,
            "real Helper_for_earning_money.lua functions execute under Lua 5.4");
        CHECK(api_host.get_global_number("helper_lua54") == 1.0,
              "real Helper_for_earning_money.lua behaves as written under Lua 5.4");


        CHECK(api_host.execute_detailed(
            "neo_lua54_events = 0\n"
            "neo_event_subscribe('lua54_event', function() neo_lua54_events = neo_lua54_events + 1 end)\n"
            "neo_event_publish('lua54_event')\n"
            "neo_event_publish('lua54_event')\n",
            "verify_neo_api_lua54").status == LuaExecutionStatus::Success,
            "NeoLoaderAPI event bridge executes under Lua 5.4");
        CHECK(api_host.get_global_number("neo_lua54_events") == 2.0,
              "NeoLoaderAPI event bridge delivers under Lua 5.4");
        CHECK(api_host.stack_top() == 0, "stack clean after Lua 5.4 compatibility checks");
    }
    api_host.shutdown();
    CHECK(!api_host.is_initialized(), "Lua 5.4 API host shutdown");

    std::filesystem::remove_all(lua54_root, cleanup_error);


    host.shutdown();
    CHECK(!host.is_initialized(), "Lua 5.4 shutdown");

    if (failures == 0) std::cout << "lua54_host tests: PASSED\n";
    else std::cerr << "lua54_host tests: " << failures << " FAILED\n";
    return failures == 0 ? 0 : 1;
}
