#include "arizona_neo/core/event_bus.hpp"
#include "arizona_neo/core/filesystem.hpp"
#include "arizona_neo/core/scheduler.hpp"
#include "arizona_neo/core/script_manager.hpp"
#include "arizona_neo/runtime/lua_jit_host.hpp"
#include "arizona_neo/runtime/neo_loader_api.hpp"
#include "arizona_neo/runtime/portable_script_loader.hpp"
#include "arizona_neo/runtime/portable_script_runtime.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << (msg) << "\n"; ++failures; } } while (0)

using arizona_neo::core::EventBus;
using arizona_neo::core::SandboxedFilesystem;
using arizona_neo::core::Scheduler;
using arizona_neo::core::ScriptManager;
using arizona_neo::core::ScriptMetadata;
using arizona_neo::core::ScriptState;
using arizona_neo::runtime::LuaExecutionStatus;
using arizona_neo::runtime::LuaJITHost;
using arizona_neo::runtime::NeoLoaderAPI;
using arizona_neo::runtime::PortableScriptLoader;
using arizona_neo::runtime::PortableScriptRuntime;

std::string read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}

bool write_module(SandboxedFilesystem& fs, const std::string& name) {
    const std::string source = read_file("scripts/neo/modules/" + name + ".lua");
    if (source.empty()) return false;
    return fs.write_file("modules/" + name + ".lua", source).ok;
}

const char* kEventsConsumer =
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

const char* kMoonloaderConsumer =
    "require 'lib.moonloader'\n"
    "function main() end\n"
    "moonloader_consumer_ready = (type(TAG) == 'table' and "
    "type(Vector3D) == 'function' and type(matrix3x3) == 'function') and 1 or 0\n";

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "arizona_neo_stage152_user_scripts";
    std::error_code cleanup_error;
    std::filesystem::remove_all(root, cleanup_error);
    std::filesystem::create_directories(root, cleanup_error);
    SandboxedFilesystem fs(root, true);

    const char* modules[] = {"sampfuncs", "moonloader", "vector3d", "matrix3x3"};
    for (const char* module : modules) {
        CHECK(write_module(fs, module), (std::string("write module ") + module).c_str());
    }
    const std::string events_source = read_file("scripts/neo/modules/samp/events.lua");
    CHECK(!events_source.empty(), "read samp.events module");
    CHECK(fs.write_file("modules/samp/events.lua", events_source).ok, "write samp.events module");
    CHECK(fs.write_file("scripts/events_consumer.lua", kEventsConsumer).ok, "write events consumer");
    CHECK(fs.write_file("scripts/moonloader_consumer.lua", kMoonloaderConsumer).ok,
          "write moonloader consumer");

    // Unmodified real user scripts, kept as fixtures with their source hashes
    // in tests/fixtures/stage15_2/README.md. Only their load-time code runs.
    const std::string arzmod_source = read_file("tests/fixtures/stage15_2/arzmod.lua");
    CHECK(!arzmod_source.empty(), "read real arzmod.lua fixture");
    CHECK(fs.write_file("scripts/arzmod.lua", arzmod_source).ok,
          "write real arzmod.lua fixture");
    const std::string helper_source =
        read_file("tests/fixtures/stage15_2/Helper_for_earning_money.lua");
    CHECK(!helper_source.empty(), "read real Helper_for_earning_money.lua fixture");
    CHECK(fs.write_file("scripts/Helper_for_earning_money.lua", helper_source).ok,
          "write real Helper_for_earning_money.lua fixture");

    auto module_reader = [&fs](const std::string& requested, std::string& out) -> bool {
        std::string name = requested;
        if (name.rfind("lib.", 0) == 0) name = name.substr(4);
        for (char& character : name) if (character == '.') character = '/';
        auto read = fs.read_file("modules/" + name + ".lua");
        if (!read.ok) read = fs.read_file("modules/" + name + "/init.lua");
        if (!read.ok) return false;
        out = std::move(read.value);
        return true;
    };

    {
        LuaJITHost host;
        host.set_module_source_reader(module_reader);
        CHECK(host.initialize(), "LuaJIT host initialize");

        // Field-assignment registration, alias caching and dispatch are the
        // portable part of the original samp.events module.
        CHECK(host.execute_detailed(
            "local e = require('lib.samp.events')\n"
            "samp_events_alias = (e == require('samp.events')) and 1 or 0\n"
            "samp_events_type = (type(e) == 'table' and type(e.dispatch) == 'function') and 1 or 0\n"
            "samp_events_initial = e.has_handler('onServerMessage') and 0 or 1\n"
            "e.onServerMessage = function(color, text) samp_events_color = color; samp_events_text = text; return false end\n"
            "samp_events_assigned = e.has_handler('onServerMessage') and 1 or 0\n"
            "samp_events_result = (e.dispatch('onServerMessage', 0x1234, 'hello') == false) and 1 or 0\n"
            "samp_events_missing = (e.dispatch('onNoSuchEvent') == nil) and 1 or 0\n"
            "samp_events_removed = e.remove_handler('onServerMessage') and not e.has_handler('onServerMessage') and 1 or 0\n"
            "local sf = require('sampfuncs')\n"
            "sampfuncs_constants = (sf.HIGH_PRIORITY == 1 and sf.UNRELIABLE_SEQUENCED == 7 "
            "and HIGH_PRIORITY == 1 and MAX_PLAYERS == 1004) and 1 or 0\n",
            "samp_events_direct").status == LuaExecutionStatus::Success,
            "samp.events direct checks execute");
        CHECK(host.get_global_number("samp_events_alias") == 1.0,
              "lib.samp.events and samp.events share one registry");
        CHECK(host.get_global_number("samp_events_type") == 1.0, "samp.events table shape");
        CHECK(host.get_global_number("samp_events_initial") == 1.0,
              "no handlers fabricated at module load");
        CHECK(host.get_global_number("samp_events_assigned") == 1.0,
              "field-assignment handler registered");
        CHECK(host.get_global_number("samp_events_result") == 1.0,
              "dispatch invokes handler and returns its result");
        CHECK(host.get_global_number("samp_events_color") == 4660.0,
              "dispatch preserves numeric argument");
        CHECK(host.get_global_string("samp_events_text") == "hello",
              "dispatch preserves string argument");
        CHECK(host.get_global_number("samp_events_missing") == 1.0,
              "unknown event does not fabricate a handler");
        CHECK(host.get_global_number("samp_events_removed") == 1.0,
              "remove_handler removes the handler");
        CHECK(host.get_global_number("sampfuncs_constants") == 1.0,
              "portable sampfuncs constants exported");

        // Invalid handler values are rejected with a Lua error instead of
        // silently creating a non-callable event field.
        CHECK(host.execute_detailed(
            "local e = require('samp.events'); e.onServerMessage = 42",
            "samp_events_invalid").status == LuaExecutionStatus::RuntimeError,
            "non-function handler rejected");
        CHECK(host.stack_top() == 0, "stack clean after direct checks");
        host.shutdown();
    }

    {
        EventBus bus;
        Scheduler scheduler;
        LuaJITHost host;
        host.set_module_source_reader(module_reader);
        CHECK(host.initialize(), "API host initialize");
        NeoLoaderAPI api(host, bus, scheduler, fs);
        CHECK(api.install(), "NeoLoaderAPI install");

        PortableScriptLoader loader(host, fs);
        PortableScriptRuntime runtime(loader);
        ScriptManager manager;
        manager.set_runtime(&runtime);
        api.set_script_manager(&manager);

        ScriptMetadata events_meta;
        events_meta.id = "events_consumer";
        events_meta.path = "scripts/events_consumer.lua";
        CHECK(manager.register_script(events_meta).ok, "register events consumer");
        CHECK(manager.load("events_consumer").ok, "load events consumer");
        CHECK(api.script_meta().name == "stage15_2_events_consumer",
              "script_name captured from consumer");
        CHECK(manager.get("events_consumer").ok &&
              manager.get("events_consumer").value.state == ScriptState::Loaded,
              "events consumer state is Loaded");

        ScriptMetadata moon_meta;
        moon_meta.id = "moonloader_consumer";
        moon_meta.path = "scripts/moonloader_consumer.lua";
        CHECK(manager.register_script(moon_meta).ok, "register moonloader consumer");
        CHECK(manager.load("moonloader_consumer").ok, "load moonloader consumer");
        CHECK(host.get_global_number("moonloader_consumer_ready") == 1.0,
              "moonloader module dependency chain loads");
        CHECK(host.execute_detailed(
            "moonloader_main_type = (type(main) == 'function') and 1 or 0",
            "verify_main").status == LuaExecutionStatus::Success,
            "moonloader consumer main defined");
        CHECK(host.get_global_number("moonloader_main_type") == 1.0,
              "script main function is defined without executing game code");

        CHECK(host.execute_detailed(
            "local e = require('lib.samp.events')\n"
            "events_dispatch_false = (e.dispatch('onServerMessage', 7, 'world') == false) and 1 or 0\n"
            "events_dialog = (e.dispatch('onShowDialog', 25286) == nil) and 1 or 0\n",
            "verify_consumer_handlers").status == LuaExecutionStatus::Success,
            "consumer handlers dispatch");
        CHECK(host.get_global_number("events_dispatch_false") == 1.0,
              "consumer onServerMessage invoked");
        CHECK(host.get_global_number("events_last_color") == 7.0,
              "consumer received color");
        CHECK(host.get_global_string("events_last_text") == "world",
              "consumer received text");
        CHECK(host.get_global_number("events_last_dialog") == 25286.0,
              "consumer onShowDialog invoked");
        CHECK(host.stack_top() == 0, "stack clean after ScriptManager checks");
    }

    {
        // Real arzmod.lua: it only needs lib.samp.events at load time and wraps
        // three SAMP functions. The stubs below are test doubles for the absent
        // native SAMP API; they are never installed by the runtime itself.
        LuaJITHost host;
        host.set_module_source_reader(module_reader);
        CHECK(host.initialize(), "real scripts host initialize");
        CHECK(host.execute_detailed(
            "PLAYER_PED = 7\n"
            "sampGetPlayerIdByCharHandle = function(handle) return false, nil end\n"
            "sampGetCharHandleBySampPlayerId = function(playerId) return false, nil end\n"
            "sampGetPlayerNickname = function(playerId) return 'orig' .. tostring(playerId) end\n",
            "install_samp_test_doubles").status == LuaExecutionStatus::Success,
            "install test doubles for absent native SAMP API");

        PortableScriptLoader loader(host, fs);
        CHECK(loader.load("arzmod", "scripts/arzmod.lua").ok,
              "real arzmod.lua loads with portable dependencies");
        CHECK(host.execute_detailed(
            "local e = require('lib.samp.events')\n"
            "arzmod_handlers = (e.has_handler('onInitGame') and "
            "e.has_handler('onSetPlayerName')) and 1 or 0\n"
            "e.dispatch('onInitGame', 3)\n"
            "e.dispatch('onSetPlayerName', 3, 'Neo_Player', true)\n"
            "local ok1, id = sampGetPlayerIdByCharHandle(PLAYER_PED)\n"
            "local ok2, handle = sampGetCharHandleBySampPlayerId(3)\n"
            "local name = sampGetPlayerNickname(3)\n"
            "local ok3, other = sampGetCharHandleBySampPlayerId(4)\n"
            "local otherName = sampGetPlayerNickname(4)\n"
            "arzmod_ready = (arzmod_handlers == 1 and ok1 and id == 3 and ok2 "
            "and handle == PLAYER_PED and name == 'Neo_Player' and not ok3 "
            "and other == nil and otherName == 'orig4') and 1 or 0\n",
            "verify_real_arzmod").status == LuaExecutionStatus::Success,
            "real arzmod.lua wrappers execute");
        CHECK(host.get_global_number("arzmod_ready") == 1.0,
              "real arzmod.lua event state and wrappers behave as written");

        // Real Helper_for_earning_money.lua: load-time dependencies are
        // lib.moonloader and lib.samp.events. Its main() loop needs native
        // isSampAvailable/wait/getCharCoordinates and is deliberately not run.
        CHECK(loader.load("helper", "scripts/Helper_for_earning_money.lua").ok,
              "real Helper_for_earning_money.lua loads");
        CHECK(host.execute_detailed(
            "local e = require('lib.samp.events')\n"
            "helper_ready = (type(main) == 'function' and type(cmdZarabotok) == 'function' "
            "and type(cmdResetStat) == 'function' and e.has_handler('onServerMessage')) and 1 or 0\n"
            "helper_dialog_id = nil\n"
            "helper_chat_count = 0\n"
            "sampShowDialog = function(id, title, text, button1, button2, style)\n"
            "  helper_dialog_id = id\n"
            "end\n"
            "sampAddChatMessage = function(text, color) helper_chat_count = helper_chat_count + 1 end\n"
            "cmdResetStat()\n"
            "cmdZarabotok()\n"
            "helper_calls = (helper_chat_count == 1 and helper_dialog_id == 8888) and 1 or 0\n",
            "verify_real_helper").status == LuaExecutionStatus::Success,
            "real Helper_for_earning_money.lua functions execute");
        CHECK(host.get_global_number("helper_ready") == 1.0,
              "real Helper_for_earning_money.lua load-time surface is present");
        CHECK(host.get_global_number("helper_calls") == 1.0,
              "real Helper_for_earning_money.lua command functions use portable Lua logic");
        CHECK(host.stack_top() == 0, "stack clean after real script checks");
        host.shutdown();
    }

    {
        // thisScript metadata/reload hook, safe filesystem globals and the
        // cooperative wait/lua_thread runtime.
        EventBus bus;
        Scheduler scheduler;
        LuaJITHost host;
        host.set_module_source_reader(module_reader);
        CHECK(host.initialize(), "lifecycle host initialize");
        NeoLoaderAPI api(host, bus, scheduler, fs);
        CHECK(api.install(), "lifecycle API install");

        CHECK(host.execute_detailed(
            "script_name('stage15_2r_meta')\n"
            "script_author('Neo')\n"
            "script_version('2.0')\n"
            "script_version_number(42)\n"
            "script_description('stage15_2r metadata')\n"
            "script_properties('p1', 'p2')\n"
            "local s = thisScript()\n"
            "this_script_fields = (s.name == 'stage15_2r_meta' and s.author == 'Neo' "
            "and s.version == '2.0' and s.version_number == 42 "
            "and s.description == 'stage15_2r metadata' "
            "and s.properties[1] == 'p1' and s.properties[2] == 'p2') and 1 or 0\n"
            "this_script_identity = (s == thisScript()) and 1 or 0\n"
            "local ok, err = s:reload()\n"
            "this_script_default_reload = (ok == false and type(err) == 'string' "
            "and err:find('unavailable', 1, true) ~= nil) and 1 or 0\n",
            "verify_this_script_default").status == LuaExecutionStatus::Success,
            "thisScript metadata and default reload probe execute");
        CHECK(host.get_global_number("this_script_fields") == 1.0,
              "thisScript exposes script metadata");
        CHECK(host.get_global_number("this_script_identity") == 1.0,
              "thisScript returns a stable table identity");
        CHECK(host.get_global_number("this_script_default_reload") == 1.0,
              "thisScript reload fails explicitly without a lifecycle callback");

        api.set_script_path("scripts/lifecycle.lua");
        int reload_calls = 0;
        api.set_reload_handler([&reload_calls]() {
            ++reload_calls;
            return true;
        });
        CHECK(host.execute_detailed(
            "local s = thisScript()\n"
            "this_script_path = (s.path == 'scripts/lifecycle.lua') and 1 or 0\n"
            "local ok = s:reload()\n"
            "this_script_reload = (ok == true) and 1 or 0\n",
            "verify_this_script_reload").status == LuaExecutionStatus::Success,
            "thisScript path and reload hook execute");
        CHECK(host.get_global_number("this_script_path") == 1.0,
              "thisScript reports the host-provided path");
        CHECK(host.get_global_number("this_script_reload") == 1.0,
              "thisScript reload uses the lifecycle callback");
        CHECK(reload_calls == 1, "lifecycle reload callback invoked exactly once");
        CHECK(api.script_meta().path == "scripts/lifecycle.lua",
              "script path is stored in metadata");

        CHECK(fs.write_file("fs_probe/file.txt", "x").ok, "write fs probe file");
        CHECK(fs.create_directories("fs_probe/sub").ok, "create fs probe directory");
        CHECK(host.execute_detailed(
            "filesystem_globals = (doesFileExist('fs_probe/file.txt') and "
            "not doesFileExist('fs_probe/missing.txt') and "
            "doesDirectoryExist('fs_probe') and not doesDirectoryExist('fs_probe/file.txt') "
            "and createDirectory('fs_probe/newdir') and doesDirectoryExist('fs_probe/newdir') "
            "and not doesFileExist('../outside.txt')) and 1 or 0\n",
            "verify_filesystem_globals").status == LuaExecutionStatus::Success,
            "sandbox filesystem globals execute");
        CHECK(host.get_global_number("filesystem_globals") == 1.0,
              "filesystem globals stay inside the sandbox");

        CHECK(host.execute_detailed(
            "wait_outside_ok = pcall(wait, 0)\n"
            "wait_outside_rejected = (not wait_outside_ok) and 1 or 0\n"
            "main = function()\n"
            "  wait_marker = 1\n"
            "  wait(10)\n"
            "  wait_marker = 2\n"
            "  wait(5)\n"
            "  wait_marker = 3\n"
            "end\n"
            "thread_handle = lua_thread.create(function()\n"
            "  thread_marker = 1\n"
            "  wait(5)\n"
            "  thread_marker = 2\n"
            "end)\n"
            "thread_error_handle = lua_thread.create(function() error('thread boom') end)\n",
            "install_lifecycle_threads").status == LuaExecutionStatus::Success,
            "cooperative thread scripts install");
        CHECK(host.get_global_number("wait_outside_rejected") == 1.0,
              "wait outside a script thread is rejected");
        CHECK(api.run_main(), "run_main starts main in a cooperative thread");
        CHECK(host.execute_detailed("neo_tick(0)", "tick_0").status == LuaExecutionStatus::Success,
              "neo_tick drives cooperative threads");
        CHECK(host.get_global_number("wait_marker") == 1.0,
              "main runs until its first wait");
        CHECK(host.execute_detailed("neo_tick(9)", "tick_9").status == LuaExecutionStatus::Success,
              "neo_tick advances virtual time");
        CHECK(host.get_global_number("wait_marker") == 1.0,
              "main remains suspended before the deadline");
        CHECK(host.execute_detailed("neo_tick(1)", "tick_10").status == LuaExecutionStatus::Success,
              "main resumes at the deadline");
        CHECK(host.get_global_number("wait_marker") == 2.0,
              "main resumed after wait(10)");
        CHECK(host.execute_detailed("neo_tick(5)", "tick_15").status == LuaExecutionStatus::Success,
              "main resumes after the second wait");
        CHECK(host.get_global_number("wait_marker") == 3.0,
              "main completed without game callbacks");
        CHECK(host.execute_detailed(
            "thread_status = thread_handle:status()\n"
            "thread_dead_status = thread_error_handle:status()\n"
            "thread_error_text = thread_error_handle:error() or ''\n",
            "verify_thread_status").status == LuaExecutionStatus::Success,
            "thread status and error inspection execute");
        CHECK(host.get_global_string("thread_status") == "dead",
              "completed lua_thread handle reports dead");
        CHECK(host.get_global_string("thread_dead_status") == "dead",
              "failed lua_thread handle reports dead");
        CHECK(host.get_global_string("thread_error_text").find("thread boom") != std::string::npos,
              "lua_thread error is preserved without crashing the host");
        CHECK(host.stack_top() == 0, "stack clean after lifecycle checks");
        host.shutdown();
    }

    {
        // A failing script must not leave a registered entry or corrupt the
        // shared Lua stack. This uses the same module surface as a real script.
        LuaJITHost host;
        host.set_module_source_reader(module_reader);
        CHECK(host.initialize(), "failure host initialize");
        CHECK(fs.write_file(
            "scripts/events_failure.lua",
            "local e = require('lib.samp.events')\n"
            "e.onServerMessage = 42\n").ok, "write failing consumer");
        PortableScriptLoader loader(host, fs);
        const auto result = loader.load("events_failure", "scripts/events_failure.lua");
        CHECK(!result.ok, "invalid handler fails script load");
        CHECK(loader.state("events_failure").error == arizona_neo::core::CoreError::NotFoundScript,
              "failed load leaves no entry");
        CHECK(loader.last_error().find("must be a function") != std::string::npos,
              "failure reports the Lua error");
        CHECK(host.stack_top() == 0, "stack clean after failed script");

        CHECK(host.execute_detailed(
            "local ok = pcall(require, 'missing_stage152_module')\n"
            "missing_module_error = (not ok) and 1 or 0\n"
            "local ok2 = pcall(require, '../../etc/passwd')\n"
            "traversal_error = (not ok2) and 1 or 0\n",
            "verify_sandbox").status == LuaExecutionStatus::Success,
            "sandbox require checks execute");
        CHECK(host.get_global_number("missing_module_error") == 1.0,
              "missing module raises a Lua error");
        CHECK(host.get_global_number("traversal_error") == 1.0,
              "module path traversal is rejected");
        CHECK(host.stack_top() == 0, "stack clean after sandbox checks");
    }

    std::filesystem::remove_all(root, cleanup_error);
    if (failures == 0) std::cout << "stage15_2 user script compatibility tests: PASSED\n";
    else std::cerr << "stage15_2 user script compatibility tests: " << failures << " FAILED\n";
    return failures == 0 ? 0 : 1;
}
