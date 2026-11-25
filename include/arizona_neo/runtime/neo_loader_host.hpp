#pragma once

#include "arizona_neo/core/event_bus.hpp"
#include "arizona_neo/core/filesystem.hpp"
#include "arizona_neo/core/scheduler.hpp"
#include "arizona_neo/core/script_manager.hpp"
#include "arizona_neo/game/game_bridge.hpp"
#include "arizona_neo/game/graphics_failure_adapter.hpp"
#include "arizona_neo/game/ios_native_interface.hpp"
#include "arizona_neo/runtime/lua_jit_host.hpp"
#include "arizona_neo/runtime/neo_loader_api.hpp"

#include <memory>

namespace arizona_neo::runtime {

// Production host integration contract.
//
// This class is the single place where a real Arizona RP host must supply
// confirmed native function pointers and drive the NeoLoader lifecycle.
// It does NOT resolve symbols, read game memory, or guess addresses.
//
// Required host actions:
//   1. Obtain the function pointer for the confirmed graphics-failure setter
//      from the game process (headers or a documented export).
//   2. Call start() only after the game interface is ready.
//   3. Call stop() before game teardown so the callback is unregistered.
//
// No implementation in this repository currently performs step 1: the
// available artifacts contain symbols and machine code, but no production
// Arizona RP host entrypoint that passes these pointers to NeoLoader.
class NeoLoaderHost {
public:
    struct NativeInterfaces {
        game::IosGraphicsFailureInterface::SetHandlerFn graphics_failure_set_handler = nullptr;
    };

    NeoLoaderHost(LuaJITHost& host,
                  core::EventBus& bus,
                  core::Scheduler& scheduler,
                  core::ScriptManager& manager,
                  core::SandboxedFilesystem& fs);
    ~NeoLoaderHost();

    NeoLoaderHost(const NeoLoaderHost&) = delete;
    NeoLoaderHost& operator=(const NeoLoaderHost&) = delete;

    // Returns false when no confirmed native interface is supplied or when
    // already started. After stop(), start() may be called again. Capabilities
    // are promoted only after the adapter is attached and reports ready.
    bool start(const NativeInterfaces& interfaces);
    void stop() noexcept;

    bool ready() const noexcept;
    game::GameBridge& bridge() noexcept { return bridge_; }
    NeoLoaderAPI& api() noexcept { return api_; }

private:
    core::EventBus& bus_;
    game::GameBridge bridge_;
    NeoLoaderAPI api_;
    std::unique_ptr<game::GraphicsFailureAdapter> adapter_;
    bool started_ = false;
};

} // namespace arizona_neo::runtime
