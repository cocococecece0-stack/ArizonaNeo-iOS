#include "arizona_neo/core/filesystem.hpp"
#include "arizona_neo/core/script_manager.hpp"
#include "arizona_neo/runtime/lua_jit_host.hpp"
#include "arizona_neo/runtime/portable_script_loader.hpp"
#include "arizona_neo/runtime/portable_script_runtime.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
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
