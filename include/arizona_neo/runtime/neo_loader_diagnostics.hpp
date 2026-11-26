#pragma once

#include "arizona_neo/core/lua_host.hpp"
#include "arizona_neo/core/result.hpp"
#include "arizona_neo/core/script_manager.hpp"
#include "arizona_neo/core/filesystem.hpp"
#include "arizona_neo/game/game_bridge.hpp"

#include <functional>
#include <string>
#include <vector>

namespace arizona_neo::runtime {

enum class DiagnosticStatus {
    NotStarted,
    Running,
    Passed,
    Failed,
    Unavailable,
    Skipped
};

const char* to_string(DiagnosticStatus status) noexcept;

struct DiagnosticResult {
    std::string id;
    std::string name;
    DiagnosticStatus status = DiagnosticStatus::NotStarted;
    std::string description;
    std::string error;
    bool required = true;
};

// Runs real checks against the live runtime. It never marks a check as passed
// unless the corresponding operation actually succeeds.
class NeoLoaderDiagnostics {
public:
    struct Inputs {
        bool bootstrap_loaded = false;
        core::LuaHost* host = nullptr;
        core::ScriptManager* manager = nullptr;
        core::SandboxedFilesystem* filesystem = nullptr;
        game::GameBridge* game_bridge = nullptr;
        std::vector<std::string> discovered_scripts;
        std::string builtin_script_id = "neo_loader_menu";
    };

    explicit NeoLoaderDiagnostics(Inputs inputs);

    std::vector<DiagnosticResult> run();
    bool all_required_passed() const noexcept;

private:
    DiagnosticResult check(const std::string& id, const std::string& name,
                           bool required, const std::string& description,
                           const std::function<bool(std::string&)>& operation);

    Inputs inputs_;
    std::vector<DiagnosticResult> results_;
};

} // namespace arizona_neo::runtime
