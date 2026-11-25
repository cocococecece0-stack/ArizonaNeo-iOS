#pragma once

#include "arizona_neo/core/result.hpp"

#include <string>

namespace arizona_neo::core {

// Non-owning boundary used by ScriptManager to drive a real script runtime.
// Implementations must keep their own ownership of the underlying Lua state.
class ScriptRuntime {
public:
    virtual ~ScriptRuntime() = default;

    virtual Result<void> load(const std::string& id,
                              const std::string& relative_path) = 0;
    virtual Result<void> execute(const std::string& id) = 0;
    virtual Result<void> unload(const std::string& id) = 0;
    virtual Result<void> reload(const std::string& id) = 0;
    virtual std::string last_error() const = 0;
};

} // namespace arizona_neo::core
