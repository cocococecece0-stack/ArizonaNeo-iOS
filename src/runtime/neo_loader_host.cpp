#if defined(ARIZONA_NEO_WITH_LUAJIT)

#include "arizona_neo/runtime/neo_loader_host.hpp"

#include "arizona_neo/game/graphics_failure_adapter.hpp"
#include "arizona_neo/runtime/module_search_paths.hpp"

#include <utility>

namespace arizona_neo::runtime {

NeoLoaderHost::NeoLoaderHost(LuaJITHost& host,
                             core::EventBus& bus,
                             core::Scheduler& scheduler,
                             core::ScriptManager& manager,
                             core::SandboxedFilesystem& fs)
    : host_(host),
      bus_(bus),
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

void NeoLoaderHost::attach_documents(const core::UserDocumentsRoot& documents) noexcept {
    documents_ = &documents;
    if (!documents.ready()) return;
    const auto& fs = documents.filesystem();
    host_.set_module_source_reader(
        [&fs](const std::string& module_name, std::string& out) -> bool {
            for (const auto& candidate : module_candidates(module_name)) {
                const auto read = fs.read_file(candidate);
                if (!read.ok) continue;
                out = std::move(read.value);
                return true;
            }
            return false;
        });
}

std::vector<std::string> NeoLoaderHost::discover_scripts() const {
    std::vector<std::string> scripts;
    if (documents_ == nullptr || !documents_->ready()) return scripts;
    const auto listed = documents_->filesystem().list_directory(
        documents_->scripts_dir());
    if (!listed.ok) return scripts;
    for (const auto& name : listed.value) {
        if (name.size() > 4 && name.compare(name.size() - 4, 4, ".lua") == 0) {
            scripts.push_back(name);
        }
    }
    return scripts;
}

bool NeoLoaderHost::ready() const noexcept {
    return started_ && bridge_.ready();
}

} // namespace arizona_neo::runtime

#endif // ARIZONA_NEO_WITH_LUAJIT
