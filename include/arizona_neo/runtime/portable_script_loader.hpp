#pragma once

#include "arizona_neo/core/filesystem.hpp"
#include "arizona_neo/core/result.hpp"
#include "arizona_neo/core/script_manager.hpp"
#include "arizona_neo/core/lua_host.hpp"

#include <cstddef>
#include <map>
#include <string>

namespace arizona_neo::runtime {

// Small portable script-loading contract on top of the device-verified
// LuaJITHost. Scripts share ONE Lua state (the host's state); this class does
// not provide script isolation and stores no Lua references after load.
class PortableScriptLoader {
public:
    // Scripts larger than 4 MiB are rejected before being handed to Lua.
    // The largest analysed real script (Market_Helper.lua) is about 1.7 MiB;
    // the old 1 MiB limit rejected it for size alone. The limit stays bounded.
    static constexpr std::size_t kMaxScriptBytes = 4u * 1024u * 1024u;

    PortableScriptLoader(core::LuaHost& host, core::SandboxedFilesystem scripts);
    PortableScriptLoader(const PortableScriptLoader&) = delete;
    PortableScriptLoader& operator=(const PortableScriptLoader&) = delete;

    // Reads a script from the sandbox root and executes it through the host.
    // Failed loads leave NO registered entry.
    core::Result<void> load(const std::string& id, const std::string& relative_path);

    // Re-executes an already loaded script's stored source.
    core::Result<void> execute(const std::string& id);

    // Removes the registry entry. Never closes or recreates the LuaJITHost.
    core::Result<void> unload(const std::string& id);

    // Safe unload followed by a fresh read+execute from the same relative path.
    core::Result<void> reload(const std::string& id);

    core::Result<void> stop(const std::string& id);

    core::Result<core::ScriptState> state(const std::string& id) const;
    bool is_active(const std::string& id) const noexcept;
    std::size_t active_count() const noexcept;

    const std::string& last_error() const noexcept { return last_error_; }
    const core::SandboxedFilesystem& scripts() const noexcept { return scripts_; }

private:
    struct Entry {
        std::string relative_path;
        std::string source;
        core::ScriptState state = core::ScriptState::Loaded;
    };

    core::LuaHost& host_;
    core::SandboxedFilesystem scripts_;
    std::map<std::string, Entry> entries_;
    std::string last_error_;
};

} // namespace arizona_neo::runtime
