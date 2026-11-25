#include "arizona_neo/core/filesystem.hpp"
#include "arizona_neo/core/user_documents_root.hpp"
#include "arizona_neo/runtime/module_search_paths.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace arizona_neo;

namespace {

int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << (msg) << "\n"; ++failures; } } while (0)

class TestDocumentsAccess final : public core::DocumentsAccess {
public:
    core::Result<std::string> create_bookmark(const std::string& path) override {
        return core::Result<std::string>::success("bookmark:" + path);
    }
    core::Result<std::string> resolve_bookmark(const std::string& bookmark) override {
        if (!allow_resolve) {
            return core::Result<std::string>::failure(core::CoreError::AccessDenied,
                                                      "security scope expired");
        }
        if (bookmark.rfind("bookmark:", 0) != 0) {
            return core::Result<std::string>::failure(core::CoreError::InvalidArgument,
                                                      "bad bookmark");
        }
        return core::Result<std::string>::success(bookmark.substr(9));
    }
    void stop_access() noexcept override { ++stop_calls; }
    bool allow_resolve = true;
    int stop_calls = 0;
};

class TestBookmarkStore final : public core::BookmarkStore {
public:
    core::Result<void> save(const std::string&, const std::string& bookmark) override {
        value = bookmark;
        return core::Result<void>::success();
    }
    core::Result<std::string> load(const std::string&) const override {
        if (value.empty()) {
            return core::Result<std::string>::failure(core::CoreError::NotFound, "no bookmark");
        }
        return core::Result<std::string>::success(value);
    }
    std::string value;
};

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "arizona_neo_documents_test";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root, error);

    TestDocumentsAccess access;
    TestBookmarkStore store;
    core::UserDocumentsRoot documents(access, store);

    CHECK(!documents.ready(), "not ready before selection");
    CHECK(!documents.restore().ok, "restore without bookmark fails");
    CHECK(!documents.persist().ok, "persist without selection fails");

    CHECK(documents.select(root.string()).ok, "select user folder");
    CHECK(documents.ready(), "ready after selection");
    CHECK(documents.path() == root.string(), "selected path recorded");
    for (const char* directory : {"scripts", "lib", "config", "logs", "resources"}) {
        CHECK(std::filesystem::is_directory(root / directory),
              (std::string("created ") + directory).c_str());
    }

    CHECK(documents.persist().ok, "persist bookmark");
    CHECK(documents.restore().ok, "restore bookmark");
    CHECK(documents.ready(), "ready after restore");
    CHECK(access.stop_calls >= 2, "security access released before re-resolve");

    CHECK(documents.filesystem().write_file("scripts/hello.lua", "value = 42\n").ok,
          "write test script into selected folder");
    const auto read = documents.filesystem().read_file("scripts/hello.lua");
    CHECK(read.ok && read.value.find("value = 42") != std::string::npos,
          "read script from selected folder");
    const auto escape = documents.filesystem().read_file("../outside.lua");
    CHECK(!escape.ok && escape.error == core::CoreError::PathTraversal,
          "path traversal rejected");

    access.allow_resolve = false;
    documents.release();
    CHECK(!documents.ready(), "release clears readiness");
    CHECK(!documents.restore().ok, "revoked security scope fails restore");
    CHECK(!documents.ready(), "not ready after revoked bookmark");
    access.allow_resolve = true;

    auto candidates = runtime::module_candidates("lib.foo.bar");
    CHECK(candidates.size() >= 2, "module candidates generated");
    CHECK(candidates[0] == "lib/foo/bar.lua", "first candidate uses lib/?.lua");
    CHECK(candidates[1] == "lib/foo/bar/init.lua", "second candidate uses lib/?/init.lua");

    std::filesystem::remove_all(root, error);
    if (failures == 0) std::cout << "user_documents_root tests: PASSED\n";
    else std::cerr << "user_documents_root tests: " << failures << " FAILED\n";
    return failures == 0 ? 0 : 1;
}
