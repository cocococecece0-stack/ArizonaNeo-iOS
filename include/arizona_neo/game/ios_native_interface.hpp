#pragma once

namespace arizona_neo::game {

// Confirmed native interface contract from the iOS Arizona Mach-O
// (Stage 7.0 IOS_ARIZONA_BINARY_AUDIT + llvm-otool disassembly):
//
//   _game_set_graphics_failure_handler:
//     stp context (x0) and callback (x1) into two globals
//   _game_notify_graphics_failure:
//     load callback, load context, `br x1` with x0 = context
//
// Therefore the confirmed ABI is:
//   void set_handler(void* context, void (*handler)(void* context));
//   game invokes: handler(context)
//
// No other argument or return value is used by the verified call site.
// This header defines the C++ shape only. The host that owns the game process
// must supply the real function pointer; this project never resolves symbols
// by address or name at runtime.
class IosGraphicsFailureInterface {
public:
    using SetHandlerFn = void (*)(void* context, void (*handler)(void* context));

    explicit IosGraphicsFailureInterface(SetHandlerFn set_handler) noexcept
        : set_handler_(set_handler) {}

    SetHandlerFn set_handler() const noexcept { return set_handler_; }

private:
    SetHandlerFn set_handler_;
};

} // namespace arizona_neo::game
