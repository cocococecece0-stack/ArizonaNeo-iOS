#pragma once

#include "arizona_neo/core/result.hpp"
#include "arizona_neo/core/script_runtime.hpp"

#include <cstddef>
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
// When a ScriptRuntime is bound (see set_runtime), load/run/unload/reload
// drive the real runtime. Without a bound runtime, run() returns
// CoreError::RuntimeUnavailable and state is not advanced to Running.
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

    // Load a registered script through the bound runtime. Without a runtime,
    // returns RuntimeUnavailable (preserves the original contract).
    Result<void> load(const std::string& id);

    // Execute an already loaded script through the bound runtime.
    Result<void> run(const std::string& id);

    // Stop (prevent further execution of) a loaded script. No coroutine is
    // cancelled: scripts execute synchronously in a shared Lua state.
    Result<void> stop(const std::string& id);

    // Remove the runtime-side script entry while keeping its metadata.
    Result<void> unload(const std::string& id);

    // Unload + load again from the registered relative path.
    Result<void> reload(const std::string& id);

    std::size_t active_count() const;
    const std::string& last_error() const noexcept { return last_error_; }

    // Non-owning runtime binding. Caller must keep the runtime alive.
    void set_runtime(ScriptRuntime* runtime) noexcept;
    ScriptRuntime* runtime() const noexcept { return runtime_; }
    void clear_runtime() noexcept;

    static bool is_valid_transition(ScriptState from, ScriptState to) noexcept;

private:
    mutable std::mutex mutex_;
    std::map<std::string, ScriptInfo> scripts_;
    ScriptRuntime* runtime_ = nullptr;
    std::string last_error_;
};

} // namespace arizona_neo::core
