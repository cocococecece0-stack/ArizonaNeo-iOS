#if defined(ARIZONA_NEO_WITH_LUAJIT)

#include "arizona_neo/runtime/portable_script_runtime.hpp"

namespace arizona_neo::runtime {

PortableScriptRuntime::PortableScriptRuntime(PortableScriptLoader& loader) noexcept
    : loader_(loader) {}

core::Result<void> PortableScriptRuntime::load(const std::string& id,
                                               const std::string& relative_path) {
    return loader_.load(id, relative_path);
}

core::Result<void> PortableScriptRuntime::execute(const std::string& id) {
    return loader_.execute(id);
}

core::Result<void> PortableScriptRuntime::unload(const std::string& id) {
    return loader_.unload(id);
}

core::Result<void> PortableScriptRuntime::reload(const std::string& id) {
    return loader_.reload(id);
}

std::string PortableScriptRuntime::last_error() const {
    return loader_.last_error();
}

} // namespace arizona_neo::runtime

#endif // ARIZONA_NEO_WITH_LUAJIT
