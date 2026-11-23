#include "arizona_neo/core/filesystem.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

using namespace arizona_neo::core;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << (msg) << "\n"; ++failures; } } while (0)

int main() {
    namespace fs = std::filesystem;
    auto root = fs::temp_directory_path() / "arz_fs_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);

    SandboxedFilesystem sfs(root, true);

    // write + read
    {
        auto w = sfs.write_file("a.txt", "hello");
        CHECK(w.ok, "write_file ok");
        auto r = sfs.read_file("a.txt");
        CHECK(r.ok && r.value == "hello", "read_file content");
    }

    // exists
    {
        auto e = sfs.exists("a.txt");
        CHECK(e.ok, "exists true");
        auto e2 = sfs.exists("nope.txt");
        CHECK(!e2.ok, "exists false");
    }

    // list_directory
    {
        sfs.write_file("b.txt", "x");
        auto l = sfs.list_directory("");
        CHECK(l.ok, "list ok");
        CHECK(l.value.size() >= 2, "list count");
    }

    // create_directories + nested write
    {
        auto c = sfs.create_directories("sub/dir");
        CHECK(c.ok, "create_directories");
        auto w = sfs.write_file("sub/dir/f.txt", "nested");
        CHECK(w.ok, "nested write");
        auto r = sfs.read_file("sub/dir/f.txt");
        CHECK(r.ok && r.value == "nested", "nested read");
    }

    // remove_file
    {
        auto rm = sfs.remove_file("b.txt");
        CHECK(rm.ok, "remove_file");
        auto e = sfs.exists("b.txt");
        CHECK(!e.ok, "removed gone");
    }

    // remove_file refuses directory
    {
        auto rm = sfs.remove_file("sub");
        CHECK(!rm.ok, "remove dir refused");
    }

    // traversal ".."
    {
        auto r = sfs.read_file("../etc/passwd");
        CHECK(!r.ok, "traversal read rejected");
        auto w = sfs.write_file("a/../escape.txt", "x");
        CHECK(!w.ok, "traversal write rejected");
    }

    // absolute path
    {
        auto r = sfs.read_file("/etc/passwd");
        CHECK(!r.ok, "absolute read rejected");
    }

    // symlink escape (if supported)
    {
        std::error_code ec2;
        auto link = root / "link_escape";
        fs::create_symlink("/etc", link, ec2);
        if (!ec2) {
            auto r = sfs.read_file("link_escape/hostname");
            CHECK(!r.ok, "symlink escape rejected");
            fs::remove(link, ec2);
        }
    }

    // missing file read
    {
        auto r = sfs.read_file("missing.txt");
        CHECK(!r.ok && r.error == CoreError::NotFound, "missing file NotFound");
    }

    // root removal refused
    {
        auto rm = sfs.remove_file("");
        CHECK(!rm.ok, "root removal refused");
        auto rm2 = sfs.remove_file(".");
        CHECK(!rm2.ok, "root dot removal refused");
    }

    // write to root refused
    {
        auto w = sfs.write_file("", "x");
        CHECK(!w.ok, "write root refused");
    }

    // remove_file removes symlink itself, not target
    {
        std::error_code ec2;
        auto target = root / "target.txt";
        {
            std::ofstream f(target); f << "data";
        }
        auto link = root / "link_remove";
        fs::create_symlink(target, link, ec2);
        if (!ec2) {
            auto rm = sfs.remove_file("link_remove");
            CHECK(rm.ok, "remove symlink ok");
            CHECK(fs::exists(target), "target preserved after symlink removal");
            CHECK(!fs::exists(link), "symlink removed");
        }
    }

    // create_directories symlink escape
    {
        std::error_code ec2;
        auto link = root / "escape_dir";
        fs::create_symlink("/etc", link, ec2);
        if (!ec2) {
            auto c = sfs.create_directories("escape_dir/sub");
            CHECK(!c.ok, "create_directories symlink escape rejected");
            fs::remove(link, ec2);
        }
    }

    fs::remove_all(root, ec);

    if (failures == 0) std::cout << "filesystem tests: PASSED\n";
    else std::cerr << "filesystem tests: " << failures << " FAILED\n";
    return failures == 0 ? 0 : 1;
}
