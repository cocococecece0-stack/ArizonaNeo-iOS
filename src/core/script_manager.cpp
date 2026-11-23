#include "arizona_neo/core/script_manager.hpp"

namespace arizona_neo::core {

const char* to_string(ScriptState state) noexcept {
    switch (state) {
        case ScriptState::Discovered: return "Discovered";
        case ScriptState::Loaded: return "Loaded";
        case ScriptState::Running: return "Running";
        case ScriptState::Stopped: return "Stopped";
        case ScriptState::Failed: return "Failed";
    }
    return "Unknown";
}

bool ScriptManager::is_valid_transition(ScriptState from, ScriptState to) noexcept {
    if (from == to) return false;
    switch (from) {
        case ScriptState::Discovered:
            return to == ScriptState::Loaded || to == ScriptState::Failed;
        case ScriptState::Loaded:
            return to == ScriptState::Running || to == ScriptState::Stopped || to == ScriptState::Failed;
        case ScriptState::Running:
            return to == ScriptState::Stopped || to == ScriptState::Failed;
        case ScriptState::Stopped:
            return to == ScriptState::Loaded || to == ScriptState::Discovered;
        case ScriptState::Failed:
            return to == ScriptState::Discovered || to == ScriptState::Loaded;
    }
    return false;
}

Result<void> ScriptManager::register_script(const ScriptMetadata& metadata) {
    if (metadata.id.empty()) {
        return Result<void>::failure(CoreError::InvalidArgument, "script id must not be empty");
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (scripts_.count(metadata.id) > 0) {
        return Result<void>::failure(CoreError::Duplicate, "script already registered: " + metadata.id);
    }
    ScriptInfo info;
    info.metadata = metadata;
    info.state = ScriptState::Discovered;
    scripts_.emplace(metadata.id, std::move(info));
    return Result<void>::success();
}

Result<void> ScriptManager::unregister(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = scripts_.find(id);
    if (it == scripts_.end()) {
        return Result<void>::failure(CoreError::NotFoundScript, "script not found: " + id);
    }
    scripts_.erase(it);
    return Result<void>::success();
}

std::vector<ScriptInfo> ScriptManager::list() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ScriptInfo> out;
    out.reserve(scripts_.size());
    for (const auto& kv : scripts_) {
        out.push_back(kv.second);
    }
    return out;
}

Result<ScriptInfo> ScriptManager::get(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = scripts_.find(id);
    if (it == scripts_.end()) {
        return Result<ScriptInfo>::failure(CoreError::NotFoundScript, "script not found: " + id);
    }
    return Result<ScriptInfo>::success(it->second);
}

Result<void> ScriptManager::set_state(const std::string& id, ScriptState new_state) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = scripts_.find(id);
    if (it == scripts_.end()) {
        return Result<void>::failure(CoreError::NotFoundScript, "script not found: " + id);
    }
    if (!is_valid_transition(it->second.state, new_state)) {
        return Result<void>::failure(CoreError::InvalidTransition,
            std::string("invalid transition from ") + to_string(it->second.state) +
            " to " + to_string(new_state));
    }
    it->second.state = new_state;
    return Result<void>::success();
}

Result<void> ScriptManager::run(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = scripts_.find(id);
    if (it == scripts_.end()) {
        return Result<void>::failure(CoreError::NotFoundScript, "script not found: " + id);
    }
    // No Lua runtime available: do not fake execution.
    return Result<void>::failure(CoreError::RuntimeUnavailable,
        "Lua runtime is not available; cannot run script: " + id);
}

Result<void> ScriptManager::stop(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = scripts_.find(id);
    if (it == scripts_.end()) {
        return Result<void>::failure(CoreError::NotFoundScript, "script not found: " + id);
    }
    if (it->second.state != ScriptState::Running) {
        return Result<void>::failure(CoreError::InvalidTransition, "script is not running: " + id);
    }
    it->second.state = ScriptState::Stopped;
    return Result<void>::success();
}

} // namespace arizona_neo::core
