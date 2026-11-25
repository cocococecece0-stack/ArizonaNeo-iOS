#include "arizona_neo/game/game_bridge.hpp"

namespace arizona_neo::game {

const char* to_string(GameCapability capability) noexcept {
    switch (capability) {
        case GameCapability::Lifecycle: return "lifecycle";
        case GameCapability::Events: return "events";
        case GameCapability::Tick: return "tick";
        case GameCapability::Chat: return "chat";
        case GameCapability::PlayerData: return "player_data";
        case GameCapability::VehicleData: return "vehicle_data";
        case GameCapability::UI: return "ui";
    }
    return "unknown";
}

const char* to_string(CapabilityStatus status) noexcept {
    switch (status) {
        case CapabilityStatus::Unavailable: return "unavailable";
        case CapabilityStatus::PendingConfirmedInterface: return "pending_confirmed_interface";
        case CapabilityStatus::Available: return "available";
    }
    return "unknown";
}

void GameCapabilities::set(GameCapability capability,
                           CapabilityStatus status,
                           std::string evidence) {
    entries_[capability] = CapabilityInfo{status, std::move(evidence)};
}

CapabilityInfo GameCapabilities::get(GameCapability capability) const {
    const auto it = entries_.find(capability);
    if (it == entries_.end()) return CapabilityInfo{};
    return it->second;
}

bool GameCapabilities::available(GameCapability capability) const {
    return get(capability).status == CapabilityStatus::Available;
}

GameCapabilities default_capabilities() {
    GameCapabilities capabilities;
    const std::string stage70 =
        "Stage 7.0 IOS_ARIZONA_BINARY_AUDIT: symbol exists in Mach-O but ABI/signature "
        "not confirmed; no safe call permitted";

    capabilities.set(GameCapability::Lifecycle,
                     CapabilityStatus::PendingConfirmedInterface,
                     stage70 + "; _game_boot/_game_start addresses observed but no confirmed prototype");
    capabilities.set(GameCapability::Events,
                     CapabilityStatus::PendingConfirmedInterface,
                     stage70 + "; no confirmed game-to-host event callback");
    capabilities.set(GameCapability::Tick,
                     CapabilityStatus::PendingConfirmedInterface,
                     stage70 + "; _ui_frame_tick observed but callback signature unconfirmed");
    capabilities.set(GameCapability::Chat,
                     CapabilityStatus::Unavailable,
                     stage70 + "; SA-MP chat hooks not confirmed on iOS");
    capabilities.set(GameCapability::PlayerData,
                     CapabilityStatus::Unavailable,
                     stage70 + "; no confirmed player data accessor or lifetime contract");
    capabilities.set(GameCapability::VehicleData,
                     CapabilityStatus::Unavailable,
                     stage70 + "; no confirmed vehicle data accessor");
    capabilities.set(GameCapability::UI,
                     CapabilityStatus::Unavailable,
                     stage70 + "; imgui/UIKit integration not part of this stage");
    return capabilities;
}

GameBridge::GameBridge(core::EventBus& bus, GameCapabilities capabilities)
    : bus_(bus), capabilities_(std::move(capabilities)) {}

void GameBridge::attach(GameInterface* interface) noexcept {
    interface_ = interface != nullptr ? interface : &null_interface_;
}

void GameBridge::detach() noexcept {
    interface_ = &null_interface_;
}

bool GameBridge::ready() const noexcept {
    return interface_ != nullptr && interface_ != &null_interface_ && interface_->ready();
}

bool GameBridge::can_use(GameCapability capability) const noexcept {
    return boundary_.allow(ready(), capabilities_.available(capability));
}

bool GameBridge::pump() {
    if (!can_use(GameCapability::Events)) return false;
    const auto event = interface_->poll_event();
    if (!event.has_value()) return false;
    bus_.publish(kEventName);
    return true;
}

std::size_t GameBridge::pump_all(std::size_t limit) {
    std::size_t delivered = 0;
    while (delivered < limit && pump()) {
        ++delivered;
    }
    return delivered;
}

bool GameBridge::capability_available(GameCapability capability) const {
    return capabilities_.available(capability);
}

bool GameBridge::confirm_capability(GameCapability capability,
                                    const std::string& evidence) {
    if (!ready()) return false;
    capabilities_.set(capability, CapabilityStatus::Available, evidence);
    return true;
}

std::string GameBridge::lifecycle_state() const {
    return interface_ != nullptr ? interface_->lifecycle_state() : std::string("unavailable");
}

} // namespace arizona_neo::game
