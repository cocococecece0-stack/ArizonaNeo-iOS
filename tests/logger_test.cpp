#include "arizona_neo/core/logger.hpp"

#include <atomic>
#include <iostream>
#include <string>
#include <vector>

using namespace arizona_neo::core;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::cerr << "FAIL: " << (msg) << "\n"; ++failures; } } while (0)

int main() {
    // to_string
    CHECK(std::string(to_string(LogLevel::Trace)) == "TRACE", "to_string Trace");
    CHECK(std::string(to_string(LogLevel::Error)) == "ERROR", "to_string Error");

    // min level filtering
    {
        std::vector<LogMessage> msgs;
        Logger logger(LogLevel::Warning);
        logger.set_sink([&](const LogMessage& m) { msgs.push_back(m); });
        logger.trace("t");
        logger.debug("d");
        logger.info("i");
        logger.warning("w");
        logger.error("e");
        CHECK(msgs.size() == 2, "filtering count");
        CHECK(msgs[0].level == LogLevel::Warning, "warning level");
        CHECK(msgs[1].level == LogLevel::Error, "error level");
    }

    // set_min_level
    {
        std::vector<LogMessage> msgs;
        Logger logger(LogLevel::Info);
        logger.set_sink([&](const LogMessage& m) { msgs.push_back(m); });
        logger.set_min_level(LogLevel::Error);
        logger.info("i");
        logger.error("e");
        CHECK(msgs.size() == 1, "set_min_level");
    }

    // callback exception safety
    {
        std::vector<LogMessage> msgs;
        Logger logger(LogLevel::Info);
        logger.set_sink([&](const LogMessage&) { throw std::runtime_error("boom"); });
        // should not throw
        bool threw = false;
        try { logger.info("x"); } catch (...) { threw = true; }
        CHECK(!threw, "sink exception swallowed");
    }

    // no sink => no crash
    {
        Logger logger(LogLevel::Info);
        logger.info("no sink");
        CHECK(true, "no sink");
    }

    // sink not called under mutex deadlock check: callback re-enters set_sink
    {
        Logger logger(LogLevel::Info);
        logger.set_sink([&](const LogMessage&) {
            logger.set_min_level(LogLevel::Debug); // re-entrant
        });
        logger.info("reentrant");
        CHECK(logger.min_level() == LogLevel::Debug, "reentrant set_min_level");
    }

    if (failures == 0) std::cout << "logger tests: PASSED\n";
    else std::cerr << "logger tests: " << failures << " FAILED\n";
    return failures == 0 ? 0 : 1;
}
