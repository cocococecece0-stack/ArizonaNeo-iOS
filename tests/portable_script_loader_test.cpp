#include "arizona_neo/core/filesystem.hpp"
#include "arizona_neo/core/result.hpp"
#include "arizona_neo/runtime/lua_jit_host.hpp"
#include "arizona_neo/runtime/portable_script_loader.hpp"

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
using arizona_neo::runtime::LuaJITHost;
using arizona_neo::runtime::PortableScriptLoader;

std::filesystem::path make_temp_root() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto root = std::filesystem::temp_directory_path() /
                ("arizona_neo_scripts_" + std::to_string(stamp));
    std::filesystem::create_directories(root);
    return root;
}

} // namespace

int main() {
    const std::filesystem::path root = make_temp_root();
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code ec; std::filesystem::remove_all(path, ec); }
    } cleanup{root};

    SandboxedFilesystem fs(root, true);
    LuaJITHost host;
    CHECK(host.initialize(), "LuaJIT host initializes");

    PortableScriptLoader loader(host, fs);

    // 1 valid load, 2 successful execution, 14 repeated execution + stack hygiene.
    CHECK(fs.write_file("valid.lua", "loaded_ok = 7\n").ok, "write valid script");
    auto loaded = loader.load("valid", "valid.lua");
    CHECK(loaded.ok, "valid script loads");
    CHECK(loader.is_active("valid"), "loaded script is active");
    auto state = loader.state("valid");
    CHECK(state.ok && state.value == arizona_neo::core::ScriptState::Loaded,
          "loaded state is Loaded");
    CHECK(loader.execute("valid").ok, "first execute succeeds");
    CHECK(host.get_global_number("loaded_ok") == 7.0, "script result readable from host");
    CHECK(host.stack_top() == 0, "stack clean after first execute");
    CHECK(loader.execute("valid").ok, "second execute succeeds");
    CHECK(host.stack_top() == 0, "stack clean after second execute");

    // 3 missing file, 13 failed-load cleanup.
    const auto before_failures = loader.active_count();
    auto missing = loader.load("missing", "does_not_exist.lua");
    CHECK(!missing.ok && missing.error == CoreError::NotFound, "missing file rejected");
    CHECK(loader.state("missing").error == CoreError::NotFoundScript,
          "failed load leaves no registered script");
    CHECK(loader.active_count() == before_failures, "failed load does not change active count");

    // 4 invalid absolute path.
    auto absolute = loader.load("absolute", "/etc/passwd");
    CHECK(!absolute.ok && absolute.error == CoreError::InvalidPath,
          "absolute path rejected");

    // 5 path traversal.
    auto traversal = loader.load("traversal", "../outside.lua");
    CHECK(!traversal.ok && traversal.error == CoreError::PathTraversal,
          "path traversal rejected");

    // 6 oversized script.
    const std::string oversized(PortableScriptLoader::kMaxScriptBytes + 1, ' ');
    CHECK(fs.write_file("huge.lua", oversized).ok, "write oversized script");
    auto huge = loader.load("huge", "huge.lua");
    CHECK(!huge.ok && huge.error == CoreError::InvalidArgument, "oversized script rejected");
    CHECK(huge.message.find("maximum") != std::string::npos, "oversize message is explicit");

    // 6b. The old 1 MiB cap rejected Market_Helper.lua (1,700,956 bytes) before
    // dependency analysis even started. The bounded 4 MiB cap must accept a
    // script of that size. Whitespace is valid Lua and keeps the test source
    // generated rather than committed.
    const std::string market_helper_sized(1700956, ' ');
    CHECK(fs.write_file("market_helper_sized.lua", market_helper_sized).ok,
          "write Market_Helper-sized script");
    auto market_sized = loader.load("market_helper_sized", "market_helper_sized.lua");
    CHECK(market_sized.ok, "script above the old 1 MiB cap loads");
    CHECK(loader.state("market_helper_sized").ok, "Market_Helper-sized script has state");

    // 7 syntax error, 9 traceback preservation.
    auto syntax = loader.load("traceback_case", "syntax_placeholder.lua");
    CHECK(!syntax.ok, "syntax pre-step: missing placeholder fails");
    CHECK(fs.write_file("syntax.lua", "return 2 +").ok, "write syntax-error script");
    auto syntax2 = loader.load("traceback_case", "syntax.lua");
    CHECK(!syntax2.ok && syntax2.error == CoreError::Unknown, "syntax error reported");
    CHECK(syntax2.message.find("traceback_case") != std::string::npos,
          "Lua error keeps chunk name (traceback evidence)");
    CHECK(host.stack_top() == 0, "stack clean after syntax error");
    CHECK(loader.state("traceback_case").error == CoreError::NotFoundScript,
          "syntax failure leaves no partial registration");

    // 8 runtime error.
    CHECK(fs.write_file("runtime_error.lua", "error('script boom')").ok,
          "write runtime-error script");
    auto runtime_error = loader.load("runtime_case", "runtime_error.lua");
    CHECK(!runtime_error.ok, "runtime error reported");
    CHECK(runtime_error.message.find("script boom") != std::string::npos,
          "Lua runtime message preserved");
    CHECK(host.stack_top() == 0, "stack clean after Lua runtime error");

    // 10 duplicate identity.
    auto duplicate = loader.load("valid", "valid.lua");
    CHECK(!duplicate.ok && duplicate.error == CoreError::Duplicate,
          "duplicate script identity rejected");

    // 11 unload, 12 reload.
    CHECK(loader.unload("valid").ok, "unload succeeds");
    CHECK(!loader.is_active("valid"), "unloaded script is inactive");
    CHECK(loader.load("valid", "valid.lua").ok, "reload via load succeeds");
    CHECK(loader.reload("valid").ok, "explicit reload succeeds");
    CHECK(loader.state("valid").ok, "reloaded script has deterministic state");

    // 15 shutdown behavior.
    loader.stop("valid");
    auto stopped = loader.execute("valid");
    CHECK(!stopped.ok && stopped.error == CoreError::InvalidTransition,
          "stopped script cannot execute");
    CHECK(loader.unload("valid").ok, "unload after stop succeeds");
    host.shutdown();
    auto after_shutdown = loader.execute("missing");
    CHECK(!after_shutdown.ok, "execute after host shutdown fails");

    // Recreate host for remaining sandbox tests.
    CHECK(host.initialize(), "host re-initializes");
    CHECK(loader.scripts().root() == root, "loader uses configured scripts root");

    // 16 script root isolation.
    const std::filesystem::path outside = root.parent_path() / "arizona_outside.lua";
    { std::ofstream out(outside); out << "outside = 1\n"; }
    auto outside_load = loader.load("outside", outside.string());
    CHECK(!outside_load.ok, "file outside scripts root is not loadable");
    std::error_code ec;
    std::filesystem::remove(outside, ec);

    // 17 symlink escape rejection.
    const std::filesystem::path link = root / "escape_link.lua";
    std::filesystem::create_symlink(outside, link, ec);
    if (!ec) {
        auto symlink_load = loader.load("symlink", "escape_link.lua");
        CHECK(!symlink_load.ok, "symlink escaping scripts root is rejected");
        std::filesystem::remove(link, ec);
    } else {
        std::cout << "SKIP: symlink creation unsupported on this filesystem\n";
    }

    // 18 disabled Lua libraries are not exposed.
    CHECK(fs.write_file("no_libs.lua",
        "assert(io == nil and os == nil and debug == nil and bit == nil)\n"
        "no_libs_ok = 1\n").ok, "write no-libs script");
    CHECK(loader.load("no_libs", "no_libs.lua").ok, "no-libs script loads");
    CHECK(host.get_global_number("no_libs_ok") == 1.0, "disabled libraries stay unavailable");

    // Shared state: explicitly NOT isolated.
    CHECK(fs.write_file("writer.lua", "shared_probe = 99\n").ok, "write shared-state script");
    CHECK(loader.load("writer", "writer.lua").ok, "writer script loads");
    CHECK(host.get_global_number("shared_probe") == 99.0,
          "scripts share one Lua state (no isolation claimed)");

    if (failures == 0) std::cout << "portable_script_loader_test: PASSED\n";
    else std::cerr << "portable_script_loader_test: " << failures << " FAILED\n";
    return failures == 0 ? 0 : 1;
}
