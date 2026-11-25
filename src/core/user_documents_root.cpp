#include "arizona_neo/core/user_documents_root.hpp"

#include <filesystem>

namespace arizona_neo::core {

namespace {

std::filesystem::path placeholder_root() {
    return std::filesystem::temp_directory_path() / ".arizona_neo_documents_placeholder";
}

} // namespace

UserDocumentsRoot::UserDocumentsRoot(DocumentsAccess& access, BookmarkStore& store)
    : access_(access), store_(store), fs_(placeholder_root(), false) {}

Result<void> UserDocumentsRoot::select(const std::string& selected_path) {
    if (selected_path.empty()) {
        return Result<void>::failure(CoreError::InvalidArgument, "selected path is empty");
    }

    std::error_code error;
    std::filesystem::path candidate(selected_path);
    if (!candidate.is_absolute()) {
        return Result<void>::failure(CoreError::InvalidArgument,
                                     "selected directory path must be absolute");
    }
    const auto canonical = std::filesystem::weakly_canonical(candidate, error);
    if (error) {
        return Result<void>::failure(CoreError::IoError,
                                     "cannot resolve selected directory: " + error.message());
    }
    if (!std::filesystem::is_directory(canonical, error) || error) {
        return Result<void>::failure(CoreError::NotDirectory,
                                     "selected path is not a directory");
    }

    const auto created = access_.create_bookmark(canonical.string());
    if (!created.ok) {
        return Result<void>::failure(created.error, created.message);
    }

    release();
    path_ = canonical.string();
    bookmark_ = created.value;
    fs_ = SandboxedFilesystem(canonical, false);
    const auto structure = ensure_structure();
    if (!structure.ok) {
        reset();
        return structure;
    }
    ready_ = true;
    return Result<void>::success();
}

Result<void> UserDocumentsRoot::restore(const std::string& key) {
    const auto stored = store_.load(key);
    if (!stored.ok) {
        return Result<void>::failure(stored.error, "no stored NeoLoader folder bookmark");
    }
    const auto resolved = access_.resolve_bookmark(stored.value);
    if (!resolved.ok) {
        // Permission was revoked: the host must show the picker again.
        return Result<void>::failure(resolved.error,
                                     "stored NeoLoader folder is no longer accessible");
    }

    release();
    path_ = resolved.value;
    bookmark_ = stored.value;
    fs_ = SandboxedFilesystem(path_, false);
    const auto structure = ensure_structure();
    if (!structure.ok) {
        reset();
        return structure;
    }
    ready_ = true;
    return Result<void>::success();
}

Result<void> UserDocumentsRoot::persist(const std::string& key) const {
    if (!ready_ || bookmark_.empty()) {
        return Result<void>::failure(CoreError::AccessDenied,
                                     "no selected NeoLoader folder to persist");
    }
    return store_.save(key, bookmark_);
}

void UserDocumentsRoot::release() noexcept {
    access_.stop_access();
    reset();
}

Result<void> UserDocumentsRoot::ensure_structure() {
    for (const char* directory : {kScripts, kLib, kConfig, kLogs, kResources}) {
        const auto result = fs_.create_directories(directory);
        if (!result.ok) {
            return Result<void>::failure(result.error,
                                         std::string("cannot create NeoLoader/") + directory +
                                             ": " + result.message);
        }
    }
    return Result<void>::success();
}

void UserDocumentsRoot::reset() noexcept {
    path_.clear();
    bookmark_.clear();
    ready_ = false;
    fs_ = SandboxedFilesystem(placeholder_root(), false);
}

} // namespace arizona_neo::core
