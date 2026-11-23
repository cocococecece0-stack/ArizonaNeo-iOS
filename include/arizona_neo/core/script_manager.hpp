#pragma once

#include "arizona_neo/core/result.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace arizona_neo::core {

enum class ScriptState {
    Discovered,
    Loaded,
    Running,
    Stopped,
    Failed
};

const char* to_string(ScriptState state) noexcept;

struct ScriptMetadata {
    std::string id;
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::filesystem::path path;
};

struct ScriptInfo {
    ScriptMetadata metadata;
    ScriptState state = ScriptState::Discovered;
};

// Manages script registration and lifecycle state.
//
// Note: there is no Lua runtime yet. Attempts to run a script return
// CoreError::RuntimeUnavailable and do not change state to Running.
class ScriptManager {
public:
    ScriptManager() = default;

    // Register a script. Duplicate ids are rejected.
    Result<void> register_script(const ScriptMetadata& metadata);

    // Unregister a script by id.
    Result<void> unregister(const std::string& id);

    // List all registered scripts (snapshot).
    std::vector<ScriptInfo> list() const;

    // Get info for a single script.
    Result<ScriptInfo> get(const std::string& id) const;

    // Request a state transition. Invalid transitions are rejected.
    Result<void> set_state(const std::string& id, ScriptState new_state);

    // Attempt to run a script. Returns RuntimeUnavailable (no Lua backend).
    Result<void> run(const std::string& id);

    // Attempt to stop a running script.
    Result<void> stop(const std::string& id);

    static bool is_valid_transition(ScriptState from, ScriptState to) noexcept;

private:
    mutable std::mutex mutex_;
    std::map<std::string, ScriptInfo> scripts_;
};

} // namespace arizona_neo::core
