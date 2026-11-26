#include "arizona_neo/runtime/portable_script_loader.hpp"

namespace arizona_neo::runtime {

PortableScriptLoader::PortableScriptLoader(core::LuaHost& host,
                                           core::SandboxedFilesystem scripts)
    : host_(host), scripts_(std::move(scripts)) {}

core::Result<void> PortableScriptLoader::load(const std::string& id,
                                              const std::string& relative_path) {
    last_error_.clear();
    if (id.empty()) {
        return core::Result<void>::failure(core::CoreError::InvalidArgument,
                                           "script id must not be empty");
    }
    if (entries_.find(id) != entries_.end()) {
        return core::Result<void>::failure(core::CoreError::Duplicate,
                                           "script already registered: " + id);
    }

    // All path validation (absolute paths, "..", symlink escape, missing files)
    // is delegated to the existing sandbox boundary. No process-wide CWD change.
    auto read = scripts_.read_file(relative_path);
    if (!read.ok) {
        last_error_ = read.message;
        return core::Result<void>::failure(read.error, read.message);
    }
    if (read.value.size() > kMaxScriptBytes) {
        last_error_ = "script exceeds maximum allowed size";
        return core::Result<void>::failure(core::CoreError::InvalidArgument,
                                           last_error_);
    }

    // Shared Lua state: executing the source is the load step for this slice.
    // execute_detailed() preserves Lua errors and restores the stack.
    const auto execution = host_.execute_detailed(read.value, id);
    if (execution.status != LuaExecutionStatus::Success) {
        last_error_ = execution.message.empty()
                    ? std::string("Lua script failed: ") + to_string(execution.status)
                    : execution.message;
        // Failed loads leave no partially registered entry.
        return core::Result<void>::failure(core::CoreError::Unknown, last_error_);
    }

    Entry entry;
    entry.relative_path = relative_path;
    entry.source = std::move(read.value);
    entry.state = core::ScriptState::Loaded;
    entries_.emplace(id, std::move(entry));
    return core::Result<void>::success();
}

core::Result<void> PortableScriptLoader::execute(const std::string& id) {
    last_error_.clear();
    auto it = entries_.find(id);
    if (it == entries_.end()) {
        return core::Result<void>::failure(core::CoreError::NotFoundScript,
                                           "script not found: " + id);
    }
    if (it->second.state == core::ScriptState::Stopped) {
        return core::Result<void>::failure(core::CoreError::InvalidTransition,
                                           "script is stopped: " + id);
    }

    const auto execution = host_.execute_detailed(it->second.source, id);
    if (execution.status != LuaExecutionStatus::Success) {
        it->second.state = core::ScriptState::Failed;
        last_error_ = execution.message.empty()
                    ? std::string("Lua script failed: ") + to_string(execution.status)
                    : execution.message;
        return core::Result<void>::failure(core::CoreError::Unknown, last_error_);
    }
    return core::Result<void>::success();
}

core::Result<void> PortableScriptLoader::unload(const std::string& id) {
    last_error_.clear();
    const auto erased = entries_.erase(id);
    if (erased == 0) {
        return core::Result<void>::failure(core::CoreError::NotFoundScript,
                                           "script not found: " + id);
    }
    return core::Result<void>::success();
}

core::Result<void> PortableScriptLoader::reload(const std::string& id) {
    last_error_.clear();
    auto it = entries_.find(id);
    if (it == entries_.end()) {
        return core::Result<void>::failure(core::CoreError::NotFoundScript,
                                           "script not found: " + id);
    }
    const std::string path = it->second.relative_path;
    entries_.erase(it); // safe unload: no Lua state is destroyed
    return load(id, path);
}

core::Result<void> PortableScriptLoader::stop(const std::string& id) {
    last_error_.clear();
    auto it = entries_.find(id);
    if (it == entries_.end()) {
        return core::Result<void>::failure(core::CoreError::NotFoundScript,
                                           "script not found: " + id);
    }
    if (it->second.state != core::ScriptState::Loaded) {
        return core::Result<void>::failure(core::CoreError::InvalidTransition,
                                           "script is not loaded: " + id);
    }
    it->second.state = core::ScriptState::Stopped;
    return core::Result<void>::success();
}

core::Result<core::ScriptState> PortableScriptLoader::state(const std::string& id) const {
    auto it = entries_.find(id);
    if (it == entries_.end()) {
        return core::Result<core::ScriptState>::failure(core::CoreError::NotFoundScript,
                                                        "script not found: " + id);
    }
    return core::Result<core::ScriptState>::success(it->second.state);
}

bool PortableScriptLoader::is_active(const std::string& id) const noexcept {
    auto it = entries_.find(id);
    if (it == entries_.end()) return false;
    return it->second.state == core::ScriptState::Loaded ||
           it->second.state == core::ScriptState::Running;
}

std::size_t PortableScriptLoader::active_count() const noexcept {
    std::size_t count = 0;
    for (const auto& entry : entries_) {
        if (entry.second.state == core::ScriptState::Loaded ||
            entry.second.state == core::ScriptState::Running) {
            ++count;
        }
    }
    return count;
}

} // namespace arizona_neo::runtime

