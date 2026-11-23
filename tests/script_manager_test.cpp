#include "arizona_neo/core/script_manager.hpp"

#include <filesystem>
#include <iostream>

using namespace arizona_neo::core;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << (msg) << "\n"; ++failures; } } while (0)

int main() {
    ScriptManager mgr;

    ScriptMetadata meta;
    meta.id = "s1";
    meta.name = "Test Script";
    meta.version = "1.0";
    meta.author = "dev";
    meta.description = "desc";
    meta.path = "/tmp/s1.lua";

    // register
    {
        auto r = mgr.register_script(meta);
        CHECK(r.ok, "register ok");
        auto dup = mgr.register_script(meta);
        CHECK(!dup.ok && dup.error == CoreError::Duplicate, "duplicate rejected");
    }

    // list/get
    {
        auto list = mgr.list();
        CHECK(list.size() == 1, "list size");
        CHECK(list[0].state == ScriptState::Discovered, "initial state Discovered");
        auto g = mgr.get("s1");
        CHECK(g.ok && g.value.metadata.name == "Test Script", "get metadata");
        auto missing = mgr.get("nope");
        CHECK(!missing.ok && missing.error == CoreError::NotFoundScript, "get missing");
    }

    // valid transition Discovered -> Loaded
    {
        auto r = mgr.set_state("s1", ScriptState::Loaded);
        CHECK(r.ok, "Discovered->Loaded");
    }

    // invalid transition Loaded -> Discovered? allowed? check table: Loaded->Discovered not allowed
    {
        auto r = mgr.set_state("s1", ScriptState::Discovered);
        CHECK(!r.ok && r.error == CoreError::InvalidTransition, "invalid transition rejected");
    }

    // run returns RuntimeUnavailable
    {
        auto r = mgr.run("s1");
        CHECK(!r.ok && r.error == CoreError::RuntimeUnavailable, "run RuntimeUnavailable");
        auto g = mgr.get("s1");
        CHECK(g.ok && g.value.state == ScriptState::Loaded, "state unchanged after run");
    }

    // Loaded -> Running via set_state allowed by table, but run didn't do it
    {
        auto r = mgr.set_state("s1", ScriptState::Running);
        CHECK(r.ok, "Loaded->Running allowed");
    }

    // stop
    {
        auto r = mgr.stop("s1");
        CHECK(r.ok, "stop ok");
        auto g = mgr.get("s1");
        CHECK(g.ok && g.value.state == ScriptState::Stopped, "state Stopped");
    }

    // stop non-running
    {
        auto r = mgr.stop("s1");
        CHECK(!r.ok, "stop non-running rejected");
    }

    // unregister
    {
        auto r = mgr.unregister("s1");
        CHECK(r.ok, "unregister ok");
        auto r2 = mgr.unregister("s1");
        CHECK(!r2.ok, "unregister twice rejected");
    }

    // empty id
    {
        ScriptMetadata m2;
        m2.id = "";
        auto r = mgr.register_script(m2);
        CHECK(!r.ok && r.error == CoreError::InvalidArgument, "empty id rejected");
    }

    // is_valid_transition static
    CHECK(ScriptManager::is_valid_transition(ScriptState::Discovered, ScriptState::Loaded), "valid static");
    CHECK(!ScriptManager::is_valid_transition(ScriptState::Discovered, ScriptState::Running), "invalid static");

    if (failures == 0) std::cout << "script_manager tests: PASSED\n";
    else std::cerr << "script_manager tests: " << failures << " FAILED\n";
    return failures == 0 ? 0 : 1;
}
