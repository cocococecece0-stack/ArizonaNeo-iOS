#include "arizona_neo/core/filesystem.hpp"

#include <algorithm>
#include <fstream>
#include <system_error>

namespace arizona_neo::core {

SandboxedFilesystem::SandboxedFilesystem(std::filesystem::path root, bool create_root) {
    std::error_code ec;
    if (create_root) {
        std::filesystem::create_directories(root, ec);
    }
    auto canonical = std::filesystem::weakly_canonical(root, ec);
    if (ec) {
        canonical = root;
    }
    root_ = canonical;
}

const std::filesystem::path& SandboxedFilesystem::root() const noexcept {
    return root_;
}

bool SandboxedFilesystem::validate_relative(const std::string& relative,
                                           std::filesystem::path& out,
                                           CoreError& err,
                                           std::string& msg) const {
    if (relative.empty()) {
        // Allow empty to mean root for some operations? We'll treat empty as ".".
    }
    std::filesystem::path p(relative);
    if (p.is_absolute()) {
        err = CoreError::InvalidPath;
        msg = "absolute paths are not allowed";
        return false;
    }
    for (const auto& part : p) {
        if (part == "..") {
            err = CoreError::PathTraversal;
            msg = "path traversal ('..') is not allowed";
            return false;
        }
        // Reject drive letters / root components
        if (part == "/" || part == "\\") {
            err = CoreError::InvalidPath;
            msg = "invalid path component";
            return false;
        }
    }
    // Normalize lexically (remove "." etc.)
    out = p.lexically_normal();
    if (out.is_absolute()) {
        err = CoreError::InvalidPath;
        msg = "normalized path became absolute";
        return false;
    }
    // Ensure no traversal after normalization
    for (const auto& part : out) {
        if (part == "..") {
            err = CoreError::PathTraversal;
            msg = "path traversal after normalization";
            return false;
        }
    }
    return true;
}

bool SandboxedFilesystem::inside_root(const std::filesystem::path& resolved) const {
    std::error_code ec;
    auto rel = std::filesystem::relative(resolved, root_, ec);
    if (ec) return false;
    if (rel.empty()) return true; // resolved == root
    auto first = *rel.begin();
    if (first == "..") return false;
    // Also reject if any component is ".."
    for (const auto& part : rel) {
        if (part == "..") return false;
    }
    return true;
}

std::filesystem::path SandboxedFilesystem::resolve(const std::filesystem::path& relative) const {
    return root_ / relative;
}

Result<void> SandboxedFilesystem::exists(const std::string& relative) const {
    std::filesystem::path norm;
    CoreError err;
    std::string msg;
    if (!validate_relative(relative, norm, err, msg)) {
        return Result<void>::failure(err, msg);
    }
    auto candidate = resolve(norm);
    std::error_code ec;
    auto resolved = std::filesystem::weakly_canonical(candidate, ec);
    if (ec) {
        // Path doesn't exist; weakly_canonical failed.
        return Result<void>::failure(CoreError::NotFound, "path does not exist");
    }
    if (!inside_root(resolved)) {
        return Result<void>::failure(CoreError::SymlinkEscape, "path escapes sandbox");
    }
    if (!std::filesystem::exists(resolved, ec) || ec) {
        return Result<void>::failure(CoreError::NotFound, "path does not exist");
    }
    return Result<void>::success();
}

Result<void> SandboxedFilesystem::create_directories(const std::string& relative) {
    std::filesystem::path norm;
    CoreError err;
    std::string msg;
    if (!validate_relative(relative, norm, err, msg)) {
        return Result<void>::failure(err, msg);
    }
    auto candidate = resolve(norm);
    std::error_code ec;
    auto resolved = std::filesystem::weakly_canonical(candidate, ec);
    // For non-existing, weakly_canonical resolves existing prefix.
    if (!ec && !inside_root(resolved)) {
        // Check the resolved prefix; if candidate doesn't exist, resolved is prefix.
        // Re-check by ensuring root prefix of candidate string.
    }
    // Safety: ensure the candidate's resolved existing ancestors stay inside root.
    // We'll check the parent resolution.
    std::filesystem::path check = candidate;
    while (!check.empty() && !std::filesystem::exists(check, ec)) {
        check = check.parent_path();
    }
    if (!check.empty()) {
        auto r = std::filesystem::weakly_canonical(check, ec);
        if (!ec && !inside_root(r)) {
            return Result<void>::failure(CoreError::SymlinkEscape, "path escapes sandbox");
        }
    }
    std::filesystem::create_directories(candidate, ec);
    if (ec) {
        return Result<void>::failure(CoreError::IoError, ec.message());
    }
    // Final check
    auto final_resolved = std::filesystem::weakly_canonical(candidate, ec);
    if (!ec && !inside_root(final_resolved)) {
        // Rollback? Best effort: remove created? Skip; report.
        return Result<void>::failure(CoreError::SymlinkEscape, "created path escapes sandbox");
    }
    return Result<void>::success();
}

Result<std::string> SandboxedFilesystem::read_file(const std::string& relative) const {
    std::filesystem::path norm;
    CoreError err;
    std::string msg;
    if (!validate_relative(relative, norm, err, msg)) {
        return Result<std::string>::failure(err, msg);
    }
    auto candidate = resolve(norm);
    std::error_code ec;
    auto resolved = std::filesystem::weakly_canonical(candidate, ec);
    if (ec) {
        return Result<std::string>::failure(CoreError::NotFound, "file does not exist");
    }
    if (!inside_root(resolved)) {
        return Result<std::string>::failure(CoreError::SymlinkEscape, "path escapes sandbox");
    }
    if (!std::filesystem::is_regular_file(resolved, ec) || ec) {
        if (std::filesystem::is_directory(resolved, ec)) {
            return Result<std::string>::failure(CoreError::IsDirectory, "path is a directory");
        }
        return Result<std::string>::failure(CoreError::NotFound, "file does not exist");
    }
    std::ifstream in(resolved, std::ios::binary);
    if (!in) {
        return Result<std::string>::failure(CoreError::AccessDenied, "cannot open file for reading");
    }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (in.bad()) {
        return Result<std::string>::failure(CoreError::IoError, "error reading file");
    }
    return Result<std::string>::success(std::move(content));
}

Result<void> SandboxedFilesystem::write_file(const std::string& relative, std::string_view content) {
    std::filesystem::path norm;
    CoreError err;
    std::string msg;
    if (!validate_relative(relative, norm, err, msg)) {
        return Result<void>::failure(err, msg);
    }
    auto candidate = resolve(norm);
    // Ensure parent exists within root.
    auto parent = candidate.parent_path();
    if (!parent.empty() && parent != root_) {
        std::error_code ec;
        auto resolved_parent = std::filesystem::weakly_canonical(parent, ec);
        if (!ec && !inside_root(resolved_parent)) {
            return Result<void>::failure(CoreError::SymlinkEscape, "parent escapes sandbox");
        }
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            return Result<void>::failure(CoreError::IoError, ec.message());
        }
    }
    std::error_code ec;
    auto resolved = std::filesystem::weakly_canonical(candidate, ec);
    if (!ec && !inside_root(resolved)) {
        return Result<void>::failure(CoreError::SymlinkEscape, "path escapes sandbox");
    }
    // Prevent writing to root itself.
    if (norm.empty() || norm == ".") {
        return Result<void>::failure(CoreError::RootViolation, "cannot write to root");
    }
    std::ofstream out(candidate, std::ios::binary | std::ios::trunc);
    if (!out) {
        return Result<void>::failure(CoreError::AccessDenied, "cannot open file for writing");
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!out) {
        return Result<void>::failure(CoreError::IoError, "error writing file");
    }
    out.close();
    return Result<void>::success();
}

Result<std::vector<std::string>> SandboxedFilesystem::list_directory(const std::string& relative) const {
    std::filesystem::path norm;
    CoreError err;
    std::string msg;
    if (!validate_relative(relative, norm, err, msg)) {
        return Result<std::vector<std::string>>::failure(err, msg);
    }
    auto candidate = resolve(norm);
    std::error_code ec;
    auto resolved = std::filesystem::weakly_canonical(candidate, ec);
    if (ec) {
        return Result<std::vector<std::string>>::failure(CoreError::NotFound, "directory does not exist");
    }
    if (!inside_root(resolved)) {
        return Result<std::vector<std::string>>::failure(CoreError::SymlinkEscape, "path escapes sandbox");
    }
    if (!std::filesystem::is_directory(resolved, ec) || ec) {
        return Result<std::vector<std::string>>::failure(CoreError::NotDirectory, "path is not a directory");
    }
    std::vector<std::string> entries;
    for (const auto& entry : std::filesystem::directory_iterator(resolved, ec)) {
        if (ec) {
            return Result<std::vector<std::string>>::failure(CoreError::IoError, ec.message());
        }
        entries.push_back(entry.path().filename().string());
    }
    std::sort(entries.begin(), entries.end());
    return Result<std::vector<std::string>>::success(std::move(entries));
}

Result<void> SandboxedFilesystem::remove_file(const std::string& relative) {
    std::filesystem::path norm;
    CoreError err;
    std::string msg;
    if (!validate_relative(relative, norm, err, msg)) {
        return Result<void>::failure(err, msg);
    }
    if (norm.empty() || norm == ".") {
        return Result<void>::failure(CoreError::RootViolation, "cannot remove root");
    }
    auto candidate = resolve(norm);
    std::error_code ec;
    auto resolved = std::filesystem::weakly_canonical(candidate, ec);
    if (ec) {
        return Result<void>::failure(CoreError::NotFound, "file does not exist");
    }
    if (!inside_root(resolved)) {
        return Result<void>::failure(CoreError::SymlinkEscape, "path escapes sandbox");
    }
    if (resolved == root_) {
        return Result<void>::failure(CoreError::RootViolation, "cannot remove root");
    }
    if (std::filesystem::is_directory(resolved, ec)) {
        return Result<void>::failure(CoreError::IsDirectory, "remove_file does not remove directories");
    }
    bool removed = std::filesystem::remove(candidate, ec);
    if (ec) {
        return Result<void>::failure(CoreError::IoError, ec.message());
    }
    if (!removed) {
        return Result<void>::failure(CoreError::NotFound, "file does not exist");
    }
    return Result<void>::success();
}

} // namespace arizona_neo::core
