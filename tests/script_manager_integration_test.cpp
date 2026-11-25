#include "arizona_neo/core/event_bus.hpp"
#include "arizona_neo/core/filesystem.hpp"
#include "arizona_neo/core/scheduler.hpp"
#include "arizona_neo/runtime/neo_loader_api.hpp"
#include "arizona_neo/runtime/neo_loader_host.hpp"
#include "arizona_neo/game/game_bridge.hpp"
#include "arizona_neo/game/graphics_failure_adapter.hpp"
#include "arizona_neo/game/ios_native_interface.hpp"
#include "arizona_neo/core/script_manager.hpp"
#include "arizona_neo/runtime/lua_jit_host.hpp"
#include "arizona_neo/runtime/portable_script_loader.hpp"
#include "arizona_neo/runtime/portable_script_runtime.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << (msg) << "\n"; ++failures; } } while (0)

using arizona_neo::core::CoreError;
using arizona_neo::core::SandboxedFilesystem;
using arizona_neo::core::ScriptManager;
using arizona_neo::core::ScriptMetadata;
using arizona_neo::core::ScriptState;
using arizona_neo::runtime::LuaJITHost;
using arizona_neo::runtime::PortableScriptLoader;
using arizona_neo::runtime::PortableScriptRuntime;

std::filesystem::path make_temp_root() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto root = std::filesystem::temp_directory_path() /
                ("arizona_neo_scripts_" + std::to_string(stamp));
    std::filesystem::create_directories(root);
    return root;
}

void add_script(ScriptManager& manager, SandboxedFilesystem& fs,
                const std::string& id, const std::string& file,
                const std::string& source) {
    CHECK(fs.write_file(file, source).ok, ("write " + file).c_str());
    ScriptMetadata meta;
    meta.id = id;
    meta.name = id;
    meta.path = file;
    CHECK(manager.register_script(meta).ok, ("register " + id).c_str());
}

// Test double for GameBridge Lua API verification; not a game API.
class TestGameInterface final : public arizona_neo::game::GameInterface {
public:
    bool ready() const noexcept override { return true; }
    std::vector<arizona_neo::game::GameCapability> supported() const override {
        return {arizona_neo::game::GameCapability::Events};
    }
    std::optional<arizona_neo::game::GameEvent> poll_event() override {
        if (events.empty()) return std::nullopt;
        auto event = events.front();
        events.erase(events.begin());
        return event;
    }
    std::string lifecycle_state() const override { return "running"; }
    std::vector<arizona_neo::game::GameEvent> events;
};

void* native_fake_context = nullptr;
void (*native_fake_handler)(void*) = nullptr;

void native_fake_set_handler(void* context, void (*handler)(void*)) {
    native_fake_context = context;
    native_fake_handler = handler;
}
void native_fake_notify() {
    if (native_fake_handler != nullptr && native_fake_context != nullptr) {
        native_fake_handler(native_fake_context);
    }
}

} // namespace

int main() {
    const auto root = make_temp_root();
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path, ec); }
    } cleanup{root};

    SandboxedFilesystem fs(root, true);
    arizona_neo::runtime::LuaLibraryPolicy policy;
    policy.bit = true;
    LuaJITHost host(policy);
    CHECK(host.initialize(), "host initialize");
    PortableScriptLoader loader(host, fs);
    PortableScriptRuntime runtime(loader);
    ScriptManager manager;
    manager.set_runtime(&runtime);

    // 1 initialization / binding / 2 valid load / 3 run.
    CHECK(manager.runtime() == &runtime, "runtime binding");
    add_script(manager, fs, "valid", "valid.lua", "valid_ok = 42\n");
    CHECK(manager.load("valid").ok, "valid script load");
    auto state = manager.get("valid");
    CHECK(state.ok && state.value.state == ScriptState::Loaded, "loaded state");
    CHECK(manager.active_count() == 1, "one active script");
    CHECK(manager.run("valid").ok, "valid script run");
    CHECK(host.get_global_number("valid_ok") == 42.0, "script result via host");
    CHECK(host.stack_top() == 0, "stack clean after successful run");

    // 4 multiple scripts / 21 partial failure with multiple scripts.
    add_script(manager, fs, "second", "second.lua", "second_ok = 7\n");
    CHECK(manager.load("second").ok, "second script load");
    CHECK(manager.active_count() == 2, "two active scripts");
    CHECK(host.get_global_number("second_ok") == 7.0, "second script executed at load");

    // 5 duplicate identity / 6 repeated run.
    CHECK(!manager.load("valid").ok && manager.get("valid").value.state == ScriptState::Loaded,
          "duplicate load rejected");
    CHECK(manager.run("valid").ok, "repeated run of loaded script");
    CHECK(host.stack_top() == 0, "stack clean after repeated run");

    // 7 runtime error / 8 syntax error / 16 stack after error.
    add_script(manager, fs, "err", "err.lua", "error('script_manager_boom')");
    auto err = manager.load("err");
    CHECK(!err.ok && err.message.find("script_manager_boom") != std::string::npos,
          "Lua runtime error preserved");
    auto err_state = manager.get("err");
    CHECK(err_state.ok && err_state.value.state == ScriptState::Failed, "failed state after error");
    CHECK(host.stack_top() == 0, "stack clean after Lua runtime error");
    add_script(manager, fs, "syntax", "syntax.lua", "return 2 +");
    auto syntax = manager.load("syntax");
    CHECK(!syntax.ok && !syntax.message.empty(), "syntax error preserved");
    CHECK(host.stack_top() == 0, "stack clean after syntax error");

    // 17 other scripts survive one script's failure.
    CHECK(manager.run("valid").ok, "other script still runs after failure");
    CHECK(manager.run("second").ok, "second script still runs after failure");
    CHECK(manager.active_count() == 2, "failed loads do not count as active");

    // 9 unload / 10 reload / 11 stop / 12 repeated stop.
    // reload on a still-loaded script (entry present in loader).
    {
        auto reload_result = manager.reload("second");
        CHECK(reload_result.ok, "reload succeeds on loaded script");
        CHECK(manager.get("second").value.state == ScriptState::Loaded, "reloaded state Loaded");
    }
    CHECK(manager.stop("second").ok, "stop succeeds");
    CHECK(manager.get("second").value.state == ScriptState::Stopped, "stopped state");
    CHECK(!manager.run("second").ok, "stopped script cannot run");
    CHECK(!manager.stop("second").ok, "repeated stop rejected");

    // unload valid.
    CHECK(manager.unload("valid").ok, "unload succeeds");
    CHECK(manager.get("valid").value.state == ScriptState::Discovered, "unloaded state Discovered");
    CHECK(manager.active_count() == 0, "active count after unload (second stopped)");

    // reload after unload: loader requires prior entry; expect NotFoundScript.
    {
        auto reload_result = manager.reload("valid");
        CHECK(!reload_result.ok && reload_result.error == CoreError::NotFoundScript,
              "reload after unload rejected as NotFoundScript");
    }

    // reload unknown id.
    // (covered below)

    // 13 unknown identifiers / 14 invalid path / 15 empty file.
    CHECK(manager.run("nope").error == CoreError::NotFoundScript, "run unknown id");
    CHECK(manager.load("nope").error == CoreError::NotFoundScript, "load unknown id");
    CHECK(manager.unload("nope").error == CoreError::NotFoundScript, "unload unknown id");
    CHECK(manager.reload("nope").error == CoreError::NotFoundScript, "reload unknown id");
    {
        ScriptMetadata bad;
        bad.id = "badpath";
        bad.path = "../outside.lua";
        CHECK(manager.register_script(bad).ok, "register bad path");
        auto bad_load = manager.load("badpath");
        CHECK(!bad_load.ok && bad_load.error == CoreError::PathTraversal, "path traversal rejected");
    }
    add_script(manager, fs, "empty", "empty.lua", "");
    CHECK(manager.load("empty").ok, "empty Lua file loads and executes");

    // Production host contract: confirmed function pointers drive start/stop.
    {
        LuaJITHost production_host;
        CHECK(production_host.initialize(), "production host initialize");
        arizona_neo::core::EventBus production_bus;
        arizona_neo::core::Scheduler production_scheduler;
        SandboxedFilesystem production_fs(root, true);
        ScriptManager production_manager;
        arizona_neo::runtime::NeoLoaderHost neo_host(production_host, production_bus,
                                        production_scheduler, production_manager,
                                        production_fs);
        CHECK(!neo_host.ready(), "host not ready before start");

        arizona_neo::runtime::NeoLoaderHost::NativeInterfaces missing;
        CHECK(!neo_host.start(missing), "start rejects missing native interface");
        CHECK(!neo_host.bridge().capability_available(arizona_neo::game::GameCapability::Events),
              "capability stays unavailable without confirmed pointers");

        arizona_neo::runtime::NeoLoaderHost::NativeInterfaces confirmed;
        confirmed.graphics_failure_set_handler = &native_fake_set_handler;
        CHECK(neo_host.start(confirmed), "start attaches confirmed native interface");
        CHECK(neo_host.ready(), "host ready after confirmed attach");
        CHECK(neo_host.bridge().capability_available(arizona_neo::game::GameCapability::Events),
              "capability promoted after attach");

        native_fake_notify();
        CHECK(neo_host.bridge().pump(), "native callback forwarded after host start");
        CHECK(production_host.stack_top() == 0, "stack clean after host pump");

        neo_host.stop();
        CHECK(!neo_host.ready(), "host stopped");
        native_fake_notify();
        CHECK(!neo_host.bridge().pump(), "no callbacks after host stop");
        CHECK(native_fake_context == nullptr && native_fake_handler == nullptr,
              "host stop unregistered native callback");
        CHECK(neo_host.start(confirmed), "host supports restart after stop");
        CHECK(neo_host.ready(), "host ready after restart");
        neo_host.stop();
    }

    // Portable NeoLoader API layer: metadata, EventBus, Scheduler, sandbox fs.
    {
        const char* module_names[] = {"sha256", "base64", "binaryheap", "moonloader",
                                      "vector3d", "matrix3x3", "dkjson", "encoding", "inicfg",
                                      "md5", "basexx", "bitex", "vkeys", "timerwheel", "jsoncfg", "lfs"};
        SandboxedFilesystem api_fs(root, true);
        for (const char* name : module_names) {
            std::ifstream file(std::string("scripts/neo/modules/") + name + ".lua", std::ios::binary);
            std::string source((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            CHECK(!source.empty(), (std::string("api module source ") + name).c_str());
            CHECK(api_fs.write_file(std::string("modules/") + name + ".lua", source).ok,
                  (std::string("api write module ") + name).c_str());
        }
        std::ifstream script_file("scripts/neo/scripts/AutoFlip.lua", std::ios::binary);
        std::string script_source((std::istreambuf_iterator<char>(script_file)),
                                  std::istreambuf_iterator<char>());
        CHECK(!script_source.empty(), "original AutoFlip.lua readable");
        CHECK(api_fs.write_file("scripts/AutoFlip.lua", script_source).ok, "write AutoFlip.lua");

        arizona_neo::runtime::LuaLibraryPolicy api_policy;
        api_policy.bit = true;
        LuaJITHost api_host(api_policy);
        api_host.set_module_source_reader(
            [&api_fs](const std::string& requested, std::string& out) -> bool {
                std::string name = requested;
                if (name.rfind("lib.", 0) == 0) name = name.substr(4);
                for (char& c : name) if (c == '.') c = '/';
                const auto read = api_fs.read_file("modules/" + name + ".lua");
                if (!read.ok) return false;
                out = std::move(read.value);
                return true;
            });
        CHECK(api_host.initialize(), "api host initialize");

        arizona_neo::core::EventBus bus;
        arizona_neo::core::Scheduler scheduler;
        arizona_neo::game::GameBridge game_bridge(bus);
        arizona_neo::runtime::NeoLoaderAPI api(api_host, bus, scheduler, api_fs);
        api.set_game_bridge(&game_bridge);
        CHECK(api.install(), "portable API install");

        PortableScriptLoader api_loader(api_host, api_fs);
        PortableScriptRuntime api_runtime(api_loader);
        ScriptManager api_manager;
        api_manager.set_runtime(&api_runtime);
        api.set_script_manager(&api_manager);

        for (const char* name : module_names) {
            ScriptMetadata meta;
            meta.id = std::string("api_") + name;
            meta.path = std::string("modules/") + name + ".lua";
            CHECK(api_manager.register_script(meta).ok, "api register module");
            auto result = api_manager.load(meta.id);
            if (!result.ok) std::cerr << "API MODULE ERR " << name << ": " << result.message << "\n";
            CHECK(result.ok, (std::string("api load module ") + name).c_str());
        }

        ScriptMetadata auto_meta;
        auto_meta.id = "AutoFlip";
        auto_meta.path = "scripts/AutoFlip.lua";
        CHECK(api_manager.register_script(auto_meta).ok, "register AutoFlip");
        auto auto_result = api_manager.load("AutoFlip");
        if (!auto_result.ok) std::cerr << "AUTOFLIP ERR: " << auto_result.message << "\n";
        CHECK(auto_result.ok, "original AutoFlip.lua loads via portable API");
        CHECK(api.script_meta().name == "AutoFlip.lua", "script metadata captured from original script");

        CHECK(api_host.execute_detailed(
            "neo_meta_marker = (neo_meta().name == 'AutoFlip.lua') and 1 or 0",
            "verify_meta").status == arizona_neo::runtime::LuaExecutionStatus::Success,
            "metadata verification executes");
        CHECK(api_host.get_global_number("neo_meta_marker") == 1.0, "metadata visible in Lua");

        CHECK(api_host.execute_detailed(
            "neo_event_fired = 0; "
            "neo_event_subscribe('neo_test_event', function() neo_event_fired = neo_event_fired + 1 end); "
            "neo_event_publish('neo_test_event'); neo_event_publish('neo_test_event')",
            "verify_events").status == arizona_neo::runtime::LuaExecutionStatus::Success,
            "event verification executes");
        CHECK(api_host.get_global_number("neo_event_fired") == 2.0, "EventBus delivers Lua handlers");

        CHECK(api_host.execute_detailed(
            "neo_scheduled = 0; "
            "neo_schedule(10, function() neo_scheduled = neo_scheduled + 1 end); "
            "neo_tick(25)",
            "verify_scheduler").status == arizona_neo::runtime::LuaExecutionStatus::Success,
            "scheduler verification executes");
        CHECK(api_host.get_global_number("neo_scheduled") == 1.0, "Scheduler runs Lua callbacks");

        const auto fs_result = api_host.execute_detailed(
            "neo_fs_write('config/test.ini', '[settings]\\nenabled=true\\n'); "
            "local cfgmod = require('inicfg'); neo_cfg = cfgmod.load({}, 'test.ini')",
            "verify_fs");
        CHECK(fs_result.status == arizona_neo::runtime::LuaExecutionStatus::Success,
            "sandboxed config verification executes");
        CHECK(api_host.execute_detailed(
            "neo_cfg_marker = (neo_cfg.settings.enabled == true) and 1 or 0",
            "verify_cfg").status == arizona_neo::runtime::LuaExecutionStatus::Success,
            "config marker executes");
        CHECK(api_host.get_global_number("neo_cfg_marker") == 1.0, "sandboxed inicfg works");

        CHECK(api_host.execute_detailed(
            "neo_clock_marker = (type(os.clock()) == 'number' and type(os.time()) == 'number') and 1 or 0",
            "verify_clock").status == arizona_neo::runtime::LuaExecutionStatus::Success,
            "restricted os clock executes");
        CHECK(api_host.get_global_number("neo_clock_marker") == 1.0, "restricted os API works");

        CHECK(api_host.execute_detailed(
            "neo_manager_marker = (neo_active_scripts() >= 1 and "
            "neo_script_state('AutoFlip') == 'Loaded') and 1 or 0",
            "verify_manager").status == arizona_neo::runtime::LuaExecutionStatus::Success,
            "script lifecycle query executes");
        CHECK(api_host.get_global_number("neo_manager_marker") == 1.0, "ScriptManager lifecycle visible in Lua");

        CHECK(api_host.execute_detailed(
            "local m = require('md5'); neo_md5_marker = "
            "(m.sumhexa('abc') == '900150983cd24fb0d6963f7d28e17f72') and 1 or 0",
            "verify_md5").status == arizona_neo::runtime::LuaExecutionStatus::Success,
            "md5 verification executes");
        CHECK(api_host.get_global_number("neo_md5_marker") == 1.0, "original md5 module works");

        CHECK(api_host.execute_detailed(
            "local b = require('basexx'); neo_basexx_marker = (b.to_base64('abc') == 'YWJj') and 1 or 0",
            "verify_basexx").status == arizona_neo::runtime::LuaExecutionStatus::Success,
            "basexx verification executes");
        CHECK(api_host.get_global_number("neo_basexx_marker") == 1.0, "original basexx module works");

        CHECK(api_host.execute_detailed(
            "local bx = require('bitex'); neo_bitex_marker = (type(bx.bextract) == 'function') and 1 or 0",
            "verify_bitex").status == arizona_neo::runtime::LuaExecutionStatus::Success,
            "bitex verification executes");
        CHECK(api_host.get_global_number("neo_bitex_marker") == 1.0, "built-in bit module resolves via require");

        CHECK(api_host.execute_detailed(
            "local jc = require('jsoncfg'); jc.save({x = 1}, 'test.json'); "
            "local d = jc.load({}, 'test.json'); neo_json_marker = (d.x == 1) and 1 or 0",
            "verify_jsoncfg").status == arizona_neo::runtime::LuaExecutionStatus::Success,
            "jsoncfg verification executes");
        CHECK(api_host.get_global_number("neo_json_marker") == 1.0, "portable jsoncfg works");

        CHECK(api_host.execute_detailed(
            "local lfs_mod = require('lfs'); "
            "neo_fs_write('config/lfs_test.txt', 'x'); "
            "local attrs = lfs_mod.attributes('config/lfs_test.txt'); "
            "neo_lfs_marker = (type(attrs) == 'table' and attrs.mode == 'file') and 1 or 0",
            "verify_lfs").status == arizona_neo::runtime::LuaExecutionStatus::Success,
            "lfs verification executes");
        CHECK(api_host.get_global_number("neo_lfs_marker") == 1.0,
              "portable lfs works through sandbox");

        CHECK(api_host.execute_detailed(
            "neo_game_marker = (neo_game_ready() == false and "
            "neo_game_lifecycle() == 'unavailable') and 1 or 0",
            "verify_game_unavailable").status == arizona_neo::runtime::LuaExecutionStatus::Success,
            "game unavailable state executes");
        CHECK(api_host.get_global_number("neo_game_marker") == 1.0,
              "game bridge reports unavailable without adapter");

        auto game_caps = arizona_neo::game::default_capabilities();
        game_caps.set(arizona_neo::game::GameCapability::Events,
                      arizona_neo::game::CapabilityStatus::Available,
                      "test double with confirmed adapter contract");
        arizona_neo::game::GameBridge enabled_bridge(bus, game_caps);
        TestGameInterface game_adapter;
        game_adapter.events.push_back({arizona_neo::game::GameBridge::kEventName, "lua-game-event"});
        enabled_bridge.attach(&game_adapter);
        int game_events = 0;
        const auto game_token = bus.subscribe(arizona_neo::game::GameBridge::kEventName,
                                              [&](const std::string&) { ++game_events; });
        api.set_game_bridge(&enabled_bridge);
        CHECK(api_host.execute_detailed(
            "neo_game_ready_marker = (neo_game_ready() == true and "
            "neo_game_lifecycle() == 'running' and neo_game_pump() == 1) and 1 or 0",
            "verify_game_adapter").status == arizona_neo::runtime::LuaExecutionStatus::Success,
            "game adapter verification executes");
        CHECK(api_host.get_global_number("neo_game_ready_marker") == 1.0,
              "game bridge forwards real adapter events to Lua");
        CHECK(game_events == 1, "EventBus received game event from Lua-triggered pump");
        CHECK(bus.unsubscribe(game_token), "game test subscription removed");
        api.set_game_bridge(&game_bridge);

        // Confirmed native interface adapter (graphics failure callback).
        arizona_neo::game::IosGraphicsFailureInterface native_iface(&native_fake_set_handler);
        arizona_neo::game::GraphicsFailureAdapter native_adapter(native_iface);
        CHECK(native_adapter.ready(), "native adapter registered via confirmed ABI");
        arizona_neo::game::GameBridge native_bridge(bus);
        native_bridge.attach(&native_adapter);
        CHECK(native_bridge.confirm_capability(
                  arizona_neo::game::GameCapability::Events,
                  "confirmed _game_set_graphics_failure_handler ABI"),
              "native events capability promoted after attach");
        api.set_game_bridge(&native_bridge);

        CHECK(api_host.execute_detailed(
            "neo_native_events = 0; "
            "neo_event_subscribe('game.event', function() "
            "neo_native_events = neo_native_events + 1 end); "
            "neo_native_ready = neo_game_ready() and 1 or 0", "verify_native_ready").status ==
            arizona_neo::runtime::LuaExecutionStatus::Success, "native readiness executes");
        CHECK(api_host.get_global_number("neo_native_ready") == 1.0,
              "Lua sees confirmed native interface as ready");

        native_fake_notify();
        CHECK(api_host.execute_detailed(
            "neo_native_pump = neo_game_pump()", "verify_native_pump").status ==
            arizona_neo::runtime::LuaExecutionStatus::Success, "native pump executes");
        CHECK(api_host.get_global_number("neo_native_events") == 1.0,
              "real native callback reached Lua through EventBus");

        api.set_game_bridge(&game_bridge);
        native_bridge.detach();

        CHECK(api_host.stack_top() == 0, "stack clean after portable API checks");
    }

    // Multiple original NeoLoader modules with real require dependencies.
    {
        const char* module_names[] = {"sha256", "base64", "binaryheap",
                                      "moonloader", "vector3d", "matrix3x3", "dkjson"};
        SandboxedFilesystem module_fs(root, true);
        for (const char* name : module_names) {
            const std::string path = std::string("scripts/neo/modules/") + name + ".lua";
            std::ifstream file(path, std::ios::binary);
            std::string source((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
            CHECK(!source.empty(), (std::string("module source ") + name).c_str());
            CHECK(module_fs.write_file(std::string("modules/") + name + ".lua", source).ok,
                  (std::string("write module ") + name).c_str());
        }

        arizona_neo::runtime::LuaLibraryPolicy module_policy;
        module_policy.bit = true;
        LuaJITHost module_host(module_policy);
        module_host.set_module_source_reader(
            [&module_fs](const std::string& requested, std::string& out) -> bool {
                std::string name = requested;
                if (name.rfind("lib.", 0) == 0) name = name.substr(4);
                for (char& c : name) if (c == '.') c = '/';
                const auto read = module_fs.read_file("modules/" + name + ".lua");
                if (!read.ok) return false;
                out = std::move(read.value);
                return true;
            });
        CHECK(module_host.initialize(), "multi-module host initialize");

        PortableScriptLoader module_loader(module_host, module_fs);
        PortableScriptRuntime module_runtime(module_loader);
        ScriptManager module_manager;
        module_manager.set_runtime(&module_runtime);

        std::size_t loaded = 0;
        for (const char* name : module_names) {
            ScriptMetadata meta;
            meta.id = name;
            meta.path = std::string("modules/") + name + ".lua";
            CHECK(module_manager.register_script(meta).ok,
                  (std::string("register module ") + name).c_str());
            auto result = module_manager.load(name);
            if (!result.ok) {
                std::cerr << "MODULE LOAD ERR " << name << ": " << result.message << "\n";
            }
            CHECK(result.ok, (std::string("load module ") + name).c_str());
            if (result.ok) ++loaded;
        }
        CHECK(loaded == sizeof(module_names) / sizeof(module_names[0]), "all modules loaded");

        // Real dependency chain: moonloader requires vector3d + matrix3x3.
        CHECK(module_host.execute_detailed(
            "neo_dep_marker = (type(TAG) == 'table' and type(Vector3D) == 'function' "
            "and type(matrix3x3) == 'function') and 1 or 0", "verify_deps").status ==
            arizona_neo::runtime::LuaExecutionStatus::Success, "dependency verification executes");
        CHECK(module_host.get_global_number("neo_dep_marker") == 1.0,
              "moonloader require dependencies resolved");

        // require caches modules: same table on second call.
        CHECK(module_host.execute_detailed(
            "neo_cache_marker = (require('sha256') == require('sha256')) and 1 or 0",
            "verify_cache").status == arizona_neo::runtime::LuaExecutionStatus::Success,
            "cache verification executes");
        CHECK(module_host.get_global_number("neo_cache_marker") == 1.0,
              "require caches modules in package.loaded");

        // Missing dependency is a clear error, caught by pcall.
        CHECK(module_host.execute_detailed(
            "local ok, err = pcall(require, 'missing_module_xyz'); "
            "neo_missing_marker = (not ok and err ~= nil) and 1 or 0",
            "verify_missing").status == arizona_neo::runtime::LuaExecutionStatus::Success,
            "missing module executes");
        CHECK(module_host.get_global_number("neo_missing_marker") == 1.0,
              "missing module reported as error");
        CHECK(module_host.stack_top() == 0, "stack clean after multi-module execution");
    }

    // 18 shutdown/reinitialization / 19 no double free / 20 unbound runtime.
    // Real original NeoLoader script (lib/sha256.lua) execution.
    {
        std::ifstream neo_file("scripts/neo/sha256.lua", std::ios::binary);
        std::string neo_source((std::istreambuf_iterator<char>(neo_file)),
                               std::istreambuf_iterator<char>());
        CHECK(!neo_source.empty(), "original NeoLoader script file readable");
        CHECK(fs.write_file("sha256_neo.lua", neo_source).ok, "write neo script into sandbox");
        ScriptMetadata neo_meta;
        neo_meta.id = "neo_sha256";
        neo_meta.name = "NeoLoader sha256.lua";
        neo_meta.path = "sha256_neo.lua";
        CHECK(manager.register_script(neo_meta).ok, "register neo script");
        auto neo_load = manager.load("neo_sha256");
        if (!neo_load.ok) std::cerr << "NEO LOAD ERR: " << neo_load.message << "\n";
        CHECK(neo_load.ok, "original NeoLoader script executes");
        CHECK(host.stack_top() == 0, "stack clean after neo script");
        CHECK(manager.run("neo_sha256").ok, "original NeoLoader script re-runs");
    }

    // valid was unloaded (Discovered). Reload it back so it is Loaded before shutdown test.
    CHECK(manager.load("valid").ok, "reload-back valid before shutdown");

    host.shutdown();
    CHECK(!manager.run("valid").ok, "no execution after host shutdown");
    CHECK(host.initialize(), "host reinitializes");
    // After shutdown failure, state is Failed; recover via reload.
    CHECK(manager.reload("valid").ok, "reload recovers after host reinit");
    auto reinit_run = manager.run("valid");
    CHECK(reinit_run.ok, "manager works after host reinit");
    CHECK(host.stack_top() == 0, "stack clean after reinit run");

    manager.clear_runtime();
    auto unbound = manager.run("valid");
    CHECK(!unbound.ok && unbound.error == CoreError::RuntimeUnavailable,
          "unbound runtime returns RuntimeUnavailable");

    // Scope-based recreation checks ownership: no double-free of lua_State.
    {
        arizona_neo::runtime::LuaLibraryPolicy scoped_policy;
        scoped_policy.bit = true;
        LuaJITHost scoped_host(scoped_policy);
        CHECK(scoped_host.initialize(), "scoped host initialize");
        SandboxedFilesystem scoped_fs(root, true);
        PortableScriptLoader scoped_loader(scoped_host, scoped_fs);
        PortableScriptRuntime scoped_runtime(scoped_loader);
        ScriptManager scoped_manager;
        scoped_manager.set_runtime(&scoped_runtime);
        CHECK(scoped_manager.unload("nope").error == CoreError::NotFoundScript,
              "scoped manager operates");
    } // scoped_manager destroyed BEFORE scoped_host (declaration order is reverse)

    if (failures == 0) std::cout << "script_manager_integration_test: PASSED\n";
    else std::cerr << "script_manager_integration_test: " << failures << " FAILED\n";
    return failures == 0 ? 0 : 1;
}
