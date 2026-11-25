#if defined(ARIZONA_NEO_WITH_LUAJIT)

#include "arizona_neo/runtime/neo_loader_host.hpp"

#include "arizona_neo/game/graphics_failure_adapter.hpp"

namespace arizona_neo::runtime {

NeoLoaderHost::NeoLoaderHost(LuaJITHost& host,
                             core::EventBus& bus,
                             core::Scheduler& scheduler,
                             core::ScriptManager& manager,
                             core::SandboxedFilesystem& fs)
    : bus_(bus),
      bridge_(bus),
      api_(host, bus, scheduler, fs) {
    api_.set_script_manager(&manager);
    api_.set_game_bridge(&bridge_);
}

NeoLoaderHost::~NeoLoaderHost() {
    stop();
}

bool NeoLoaderHost::start(const NativeInterfaces& interfaces) {
    if (started_) return false;
    if (interfaces.graphics_failure_set_handler == nullptr) return false;

    // The adapter registers itself with the host-provided function pointer.
    game::IosGraphicsFailureInterface native(interfaces.graphics_failure_set_handler);
    auto adapter = std::make_unique<game::GraphicsFailureAdapter>(native);
    if (!adapter->ready()) return false;

    bridge_.attach(adapter.get());
    if (!bridge_.ready()) {
        adapter->shutdown();
        bridge_.detach();
        return false;
    }
    bridge_.confirm_capability(
        game::GameCapability::Events,
        "confirmed _game_set_graphics_failure_handler ABI supplied by production host");

    adapter_ = std::move(adapter);
    api_.install();
    started_ = true;
    return true;
}

void NeoLoaderHost::stop() noexcept {
    if (!started_) return;
    started_ = false;
    if (adapter_) adapter_->shutdown();
    bridge_.detach();
    adapter_.reset();
}

bool NeoLoaderHost::ready() const noexcept {
    return started_ && bridge_.ready();
}

} // namespace arizona_neo::runtime

#endif // ARIZONA_NEO_WITH_LUAJIT
