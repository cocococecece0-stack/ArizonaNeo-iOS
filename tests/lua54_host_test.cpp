#include "arizona_neo/core/filesystem.hpp"
#include "arizona_neo/runtime/lua54_host.hpp"
#include "arizona_neo/runtime/module_search_paths.hpp"
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
    std::filesystem::remove_all(lua54_root, cleanup_error);


    host.shutdown();
    CHECK(!host.is_initialized(), "Lua 5.4 shutdown");

    if (failures == 0) std::cout << "lua54_host tests: PASSED\n";
    else std::cerr << "lua54_host tests: " << failures << " FAILED\n";
    return failures == 0 ? 0 : 1;
}
