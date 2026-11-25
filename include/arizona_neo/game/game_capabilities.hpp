#pragma once

#include "arizona_neo/game/game_interface.hpp"

#include <map>
#include <string>

namespace arizona_neo::game {

enum class CapabilityStatus {
    Unavailable,
    PendingConfirmedInterface,
    Available
};

const char* to_string(CapabilityStatus status) noexcept;

struct CapabilityInfo {
    CapabilityStatus status = CapabilityStatus::Unavailable;
    std::string evidence;
};

// Explicit registry: a capability is never reported as Available merely
// because a symbol name exists in a binary. Evidence must name a confirmed
// interface or explain why the contract is missing.
class GameCapabilities {
public:
    void set(GameCapability capability, CapabilityStatus status, std::string evidence);

    CapabilityInfo get(GameCapability capability) const;
    bool available(GameCapability capability) const;

    const std::map<GameCapability, CapabilityInfo>& all() const noexcept { return entries_; }

private:
    std::map<GameCapability, CapabilityInfo> entries_;
};

// Baseline capability registry derived from Stage 7.0 binary research.
// All entries are explicitly unavailable/pending until a confirmed adapter
// with a verified calling convention exists.
GameCapabilities default_capabilities();

} // namespace arizona_neo::game
