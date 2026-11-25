#pragma once

#include "arizona_neo/core/script_runtime.hpp"
#include "arizona_neo/runtime/portable_script_loader.hpp"

namespace arizona_neo::runtime {

// Adapts the verified PortableScriptLoader to ScriptManager's non-owning
// runtime boundary. Does not own the loader (and therefore not the host).
class PortableScriptRuntime : public core::ScriptRuntime {
public:
    explicit PortableScriptRuntime(PortableScriptLoader& loader) noexcept;

    core::Result<void> load(const std::string& id,
                            const std::string& relative_path) override;
    core::Result<void> execute(const std::string& id) override;
    core::Result<void> unload(const std::string& id) override;
    core::Result<void> reload(const std::string& id) override;
    std::string last_error() const override;

private:
    PortableScriptLoader& loader_;
};

} // namespace arizona_neo::runtime
