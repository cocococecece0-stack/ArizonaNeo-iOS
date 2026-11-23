#pragma once

#include "arizona_neo/core/result.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace arizona_neo::core {

// A filesystem view restricted to a single root directory.
//
// Security model (best effort, not TOCTOU-proof):
// - User-supplied paths must be relative.
// - ".." components and absolute paths are rejected.
// - The resolved (weakly canonical) path must remain inside the root.
// - Symlinks that escape the root are rejected.
// - The root directory itself is never removed.
//
// Limitations: this implementation uses std::filesystem and cannot fully
// prevent race conditions between validation and the actual operation.
// It does not follow symlinks that escape the root, but a concurrent
// attacker with write access to the sandbox could still race. Treat as
// best-effort hardening, not absolute protection.
class SandboxedFilesystem {
public:
    // If create_root is true, the root directory is created if missing.
    explicit SandboxedFilesystem(std::filesystem::path root, bool create_root = false);

    const std::filesystem::path& root() const noexcept;

    // Returns ok=true if the path exists (and is safe).
    Result<void> exists(const std::string& relative) const;

    // Creates directories (including intermediate) inside the sandbox.
    Result<void> create_directories(const std::string& relative);

    // Reads a file as bytes (std::string). Rejects directories.
    Result<std::string> read_file(const std::string& relative) const;

    // Writes content to a file (creates parent dirs if needed). Overwrites.
    Result<void> write_file(const std::string& relative, std::string_view content);

    // Lists entries (file/directory names) of a directory.
    Result<std::vector<std::string>> list_directory(const std::string& relative) const;

    // Removes a single file or symlink inside the sandbox. Never removes root or directories.
    Result<void> remove_file(const std::string& relative);

private:
    std::filesystem::path root_;

    // Validates and normalizes a user path. On success, out contains the
    // normalized relative path (no "..", not absolute).
    bool validate_relative(const std::string& relative,
                           std::filesystem::path& out,
                           CoreError& err,
                           std::string& msg) const;

    // Checks that resolved path stays inside root_.
    bool inside_root(const std::filesystem::path& resolved) const;

    std::filesystem::path resolve(const std::filesystem::path& relative) const;
};

} // namespace arizona_neo::core
