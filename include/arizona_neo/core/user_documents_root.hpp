#pragma once

#include "arizona_neo/core/filesystem.hpp"
#include "arizona_neo/core/result.hpp"

#include <string>

namespace arizona_neo::core {

// Host-provided bridge to iOS security-scoped URLs. Core never imports UIKit
// or Foundation; the real Arizona host implements this with
// UIDocumentPickerViewController, bookmarkDataWithOptions and
// startAccessingSecurityScopedResource.
class DocumentsAccess {
public:
    virtual ~DocumentsAccess() = default;

    // Converts opaque bookmark bytes back into a usable directory path and
    // starts security-scoped access. Returns an error when permission was
    // revoked, so the host can show the picker again.
    virtual Result<std::string> resolve_bookmark(const std::string& bookmark) = 0;

    // Creates bookmark bytes for a user-selected directory path.
    virtual Result<std::string> create_bookmark(const std::string& path) = 0;

    virtual void stop_access() noexcept = 0;
};

// Persistent storage for the opaque bookmark. On iOS this is NSUserDefaults;
// in tests it can be an in-memory or file-backed implementation.
class BookmarkStore {
public:
    virtual ~BookmarkStore() = default;
    virtual Result<void> save(const std::string& key, const std::string& bookmark) = 0;
    virtual Result<std::string> load(const std::string& key) const = 0;
};

// Represents the user-selected `Documents/NeoLoader` folder. Owns the sandbox
// view used by PortableScriptLoader and ScriptManager. No default path is
// invented: selection or successful restoration is required.
class UserDocumentsRoot {
public:
    static constexpr const char* kDefaultKey = "arizona_neo.documents_root";

    explicit UserDocumentsRoot(DocumentsAccess& access, BookmarkStore& store);

    bool ready() const noexcept { return ready_; }
    const std::string& path() const noexcept { return path_; }
    SandboxedFilesystem& filesystem() noexcept { return fs_; }
    const SandboxedFilesystem& filesystem() const noexcept { return fs_; }

    // Called after the user picked a directory with the system picker.
    Result<void> select(const std::string& selected_path);

    // Restores access from the persisted bookmark. On failure the caller must
    // present the picker again.
    Result<void> restore(const std::string& key = kDefaultKey);

    // Persists the current selection bookmark.
    Result<void> persist(const std::string& key = kDefaultKey) const;

    void release() noexcept;

    const char* scripts_dir() const noexcept { return kScripts; }
    const char* lib_dir() const noexcept { return kLib; }
    const char* config_dir() const noexcept { return kConfig; }
    const char* logs_dir() const noexcept { return kLogs; }
    const char* resources_dir() const noexcept { return kResources; }

private:
    Result<void> ensure_structure();
    void reset() noexcept;

    static constexpr const char* kScripts = "scripts";
    static constexpr const char* kLib = "lib";
    static constexpr const char* kConfig = "config";
    static constexpr const char* kLogs = "logs";
    static constexpr const char* kResources = "resources";

    DocumentsAccess& access_;
    BookmarkStore& store_;
    std::string path_;
    std::string bookmark_;
    bool ready_ = false;
    SandboxedFilesystem fs_;
};

} // namespace arizona_neo::core
