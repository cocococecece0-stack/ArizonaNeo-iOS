#include "arizona_neo/core/event_bus.hpp"
#include "arizona_neo/core/filesystem.hpp"
#include "arizona_neo/core/scheduler.hpp"
#include "arizona_neo/core/script_manager.hpp"
#include "arizona_neo/game/game_bridge.hpp"
#include "arizona_neo/runtime/lua_jit_host.hpp"
#include "arizona_neo/runtime/neo_loader_diagnostics.hpp"
#include "arizona_neo/runtime/neo_loader_api.hpp"
#include "arizona_neo/runtime/portable_script_loader.hpp"
#include "arizona_neo/runtime/portable_script_runtime.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

using namespace arizona_neo;

namespace {

int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << (msg) << "\n"; ++failures; } } while (0)

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "arizona_neo_diagnostics_test";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root / "NeoLoader" / "scripts", error);
    std::filesystem::create_directories(root / "NeoLoader" / "lib", error);
    std::filesystem::create_directories(root / "NeoLoader" / "config", error);
    std::filesystem::create_directories(root / "NeoLoader" / "logs", error);
    std::filesystem::create_directories(root / "NeoLoader" / "resources", error);
    std::ofstream(root / "NeoLoader" / "scripts" / "neo_loader_menu.lua") << "neo_log('diag')\n";

    core::SandboxedFilesystem fs(root / "NeoLoader", true);
    runtime::LuaJITHost host;
    CHECK(host.initialize(), "host init");
    core::ScriptManager manager;
    core::EventBus bus;
    core::Scheduler scheduler;
    game::GameBridge bridge(bus);
    runtime::NeoLoaderAPI api(host, bus, scheduler, fs);
    CHECK(api.install(), "api install");
    arizona_neo::runtime::PortableScriptLoader loader(host, fs);
    arizona_neo::runtime::PortableScriptRuntime runtime(loader);
    manager.set_runtime(&runtime);

    core::ScriptMetadata meta;
    meta.id = "neo_loader_menu";
    meta.path = "scripts/neo_loader_menu.lua";
    CHECK(manager.register_script(meta).ok, "register builtin");
    CHECK(manager.load("neo_loader_menu").ok, "load builtin");

    runtime::NeoLoaderDiagnostics::Inputs inputs;
    inputs.bootstrap_loaded = true;
    inputs.host = &host;
    inputs.manager = &manager;
    inputs.filesystem = &fs;
    inputs.game_bridge = &bridge;
    inputs.discovered_scripts = {"neo_loader_menu.lua"};

    runtime::NeoLoaderDiagnostics diagnostics(inputs);
    const auto results = diagnostics.run();
    for (const auto& result : results) {
        std::cout << result.id << ": " << runtime::to_string(result.status) << "\n";
    }
    CHECK(diagnostics.all_required_passed(), "all required diagnostics pass");
    CHECK(results.size() == 12, "twelve diagnostic checks");

    const auto builtin = results[8];
    CHECK(builtin.id == "builtin_test_script" && builtin.status == runtime::DiagnosticStatus::Passed,
          "built-in script check passed");

    const auto game = results[9];
    CHECK(game.id == "game_bridge" && game.status == runtime::DiagnosticStatus::Unavailable,
          "game bridge honestly unavailable");

    const auto gui = results[10];
    CHECK(gui.id == "gui_backend" && gui.status == runtime::DiagnosticStatus::Unavailable,
          "GUI backend honestly unavailable");

    const auto ms = results[11];
    CHECK(ms.id == "ms_command" && ms.status == runtime::DiagnosticStatus::Unavailable,
          "/ms honestly unavailable");

    host.shutdown();
    std::filesystem::remove_all(root, error);
    if (failures == 0) std::cout << "neo_loader_diagnostics tests: PASSED\n";
    else std::cerr << "neo_loader_diagnostics tests: " << failures << " FAILED\n";
    return failures == 0 ? 0 : 1;
}
