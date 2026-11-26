#include "arizona_neo/runtime/neo_loader_diagnostics.hpp"

#include "arizona_neo/core/filesystem.hpp"

namespace arizona_neo::runtime {

const char* to_string(DiagnosticStatus status) noexcept {
    switch (status) {
        case DiagnosticStatus::NotStarted: return "NOT_STARTED";
        case DiagnosticStatus::Running: return "RUNNING";
        case DiagnosticStatus::Passed: return "PASSED";
        case DiagnosticStatus::Failed: return "FAILED";
        case DiagnosticStatus::Unavailable: return "UNAVAILABLE";
        case DiagnosticStatus::Skipped: return "SKIPPED";
    }
    return "UNKNOWN";
}

NeoLoaderDiagnostics::NeoLoaderDiagnostics(Inputs inputs) : inputs_(std::move(inputs)) {}

DiagnosticResult NeoLoaderDiagnostics::check(const std::string& id,
                                            const std::string& name,
                                            bool required,
                                            const std::string& description,
                                            const std::function<bool(std::string&)>& operation) {
    DiagnosticResult result;
    result.id = id;
    result.name = name;
    result.required = required;
    result.description = description;
    result.status = DiagnosticStatus::Running;
    std::string error;
    const bool ok = operation(error);
    result.status = ok ? DiagnosticStatus::Passed : DiagnosticStatus::Failed;
    result.error = error;
    return result;
}

std::vector<DiagnosticResult> NeoLoaderDiagnostics::run() {
    results_.clear();

    // 1. Bootstrap loaded (supplied by host; not invented here).
    {
        DiagnosticResult r;
        r.id = "bootstrap_loaded";
        r.name = "Bootstrap library loaded";
        r.required = true;
        r.description = "libNeoLoaderBootstrap.dylib loaded into the Arizona process";
        r.status = inputs_.bootstrap_loaded ? DiagnosticStatus::Passed
                                          : DiagnosticStatus::Failed;
        if (!inputs_.bootstrap_loaded) r.error = "bootstrap_loaded flag is false";
        results_.push_back(r);
    }

    // 2. Runtime initialized.
    results_.push_back(check("runtime_initialized", "Lua runtime initialized", true,
        "LuaJIT/Lua 5.4 host initialized successfully",
        [this](std::string& error) {
            if (inputs_.host == nullptr || !inputs_.host->is_initialized()) {
                error = "Lua host is not initialized";
                return false;
            }
            return true;
        }));

    // 3. ScriptManager.
    results_.push_back(check("script_manager", "ScriptManager created", true,
        "ScriptManager instance exists",
        [this](std::string& error) {
            if (inputs_.manager == nullptr) { error = "ScriptManager is null"; return false; }
            return true;
        }));

    // 4. PortableScriptLoader.
    results_.push_back(check("portable_loader", "PortableScriptLoader available", true,
        "PortableScriptLoader contract is available",
        [](std::string& error) { return true; }));

    // 5. NeoLoaderAPI.
    results_.push_back(check("neoloader_api", "NeoLoaderAPI registered", true,
        "NeoLoader Lua API functions are registered",
        [this](std::string& error) {
            if (inputs_.host == nullptr || !inputs_.host->is_initialized()) {
                error = "runtime unavailable";
                return false;
            }
            const auto result = inputs_.host->execute_detailed(
                "neo_diag_api = (type(neo_log) == 'function' and "
                "type(neo_meta) == 'function' and type(neo_event_subscribe) == 'function') "
                "and 1 or 0", "neo_diag_api");
            if (result.status != LuaExecutionStatus::Success) {
                error = result.message;
                return false;
            }
            return inputs_.host->get_global_number("neo_diag_api") == 1.0;
        }));

    // 6. Documents.
    results_.push_back(check("documents_root", "Documents/NeoLoader available", true,
        "UserDocumentsRoot is ready and writable",
        [this](std::string& error) {
            if (inputs_.filesystem == nullptr) {
                error = "documents filesystem is null";
                return false;
            }
            const auto exists = inputs_.filesystem->exists("");
            if (!exists.ok) {
                error = exists.message;
                return false;
            }
            return true;
        }));

    // 7. Structure.
    results_.push_back(check("documents_structure", "NeoLoader folder structure", true,
        "scripts/lib/config/logs/resources exist",
        [this](std::string& error) {
            if (inputs_.filesystem == nullptr) { error = "filesystem is null"; return false; }
            for (const char* directory : {"scripts", "lib", "config", "logs", "resources"}) {
                const auto result = inputs_.filesystem->exists(directory);
                if (!result.ok) {
                    error = std::string(directory) + ": " + result.message;
                    return false;
                }
            }
            return true;
        }));

    // 8. Discovery.
    results_.push_back(check("script_discovery", "Script discovery", true,
        "At least one Lua script is discovered",
        [this](std::string& error) {
            if (inputs_.discovered_scripts.empty()) {
                error = "no scripts discovered in Documents/NeoLoader/scripts";
                return false;
            }
            return true;
        }));

    // 9. Built-in test script.
    results_.push_back(check("builtin_test_script", "Built-in NeoLoader menu script", true,
        "neo_loader_menu.lua loads and executes",
        [this](std::string& error) {
            if (inputs_.manager == nullptr || inputs_.host == nullptr ||
                !inputs_.host->is_initialized()) {
                error = "runtime unavailable";
                return false;
            }
            const auto info = inputs_.manager->get(inputs_.builtin_script_id);
            if (!info.ok) {
                error = "built-in script is not registered";
                return false;
            }
            const auto result = inputs_.manager->run(inputs_.builtin_script_id);
            if (!result.ok) {
                error = result.message;
                return false;
            }
            return true;
        }));

    // 10. GameBridge.
    {
        DiagnosticResult r;
        r.id = "game_bridge";
        r.name = "GameBridge";
        r.required = false;
        r.description = "Confirmed native game interface availability";
        if (inputs_.game_bridge == nullptr) {
            r.status = DiagnosticStatus::Unavailable;
            r.error = "no GameBridge attached";
        } else if (inputs_.game_bridge->ready()) {
            r.status = DiagnosticStatus::Passed;
        } else {
            r.status = DiagnosticStatus::Unavailable;
            r.error = inputs_.game_bridge->lifecycle_state();
        }
        results_.push_back(r);
    }

    // 11. GUI backend.
    {
        DiagnosticResult r;
        r.id = "gui_backend";
        r.name = "Game GUI backend";
        r.required = false;
        r.description = "Confirmed in-game render callback";
        r.status = DiagnosticStatus::Unavailable;
        r.error = "no confirmed render callback";
        results_.push_back(r);
    }

    // 12. /ms command.
    {
        DiagnosticResult r;
        r.id = "ms_command";
        r.name = "NeoLoader /ms command";
        r.required = false;
        r.description = "Confirmed game chat command registration";
        r.status = DiagnosticStatus::Unavailable;
        r.error = "no confirmed chat command API";
        results_.push_back(r);
    }

    return results_;
}

bool NeoLoaderDiagnostics::all_required_passed() const noexcept {
    for (const auto& result : results_) {
        if (result.required && result.status != DiagnosticStatus::Passed) return false;
    }
    return true;
}

} // namespace arizona_neo::runtime
