#include "arizona_neo/runtime/lua54_host.hpp"

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

    // Execute an original NeoLoader module under Lua 5.4 by assigning its
    // returned table to a global. sha256.lua is original code; only the
    // assignment wrapper is test harness.
    const std::string heap_source = read_file("scripts/neo/modules/binaryheap.lua");
    CHECK(!heap_source.empty(), "original module source readable");
    std::string wrapped = "binaryheap = (function()\n" + heap_source + "\nend)()\n" +
                          "neo_heap_marker = (type(binaryheap.binaryHeap) == 'function') and 1 or 0\n";
    const auto result = host.execute_detailed(wrapped, "binaryheap_under_lua54");
    if (result.status != LuaExecutionStatus::Success) {
        std::cerr << "LUA54 ERR: " << result.message << "\n";
    }
    CHECK(result.status == LuaExecutionStatus::Success, "original binaryheap.lua executes under Lua 5.4");
    CHECK(host.get_global_number("neo_heap_marker") == 1.0,
          "binaryheap module is usable under Lua 5.4");
    CHECK(host.stack_top() == 0, "stack clean after Lua 5.4 module");

    host.shutdown();
    CHECK(!host.is_initialized(), "Lua 5.4 shutdown");

    if (failures == 0) std::cout << "lua54_host tests: PASSED\n";
    else std::cerr << "lua54_host tests: " << failures << " FAILED\n";
    return failures == 0 ? 0 : 1;
}
