#include "arizona_neo/core/filesystem.hpp"
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
