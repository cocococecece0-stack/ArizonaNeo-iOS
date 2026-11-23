#include "arizona_neo/core/version.hpp"

#include <iostream>
#include <string_view>

int main() {
    using arizona_neo::core::version;

    // Доступность через публичный заголовок проверяется самим включением выше.
    const std::string_view current_version = version();

    // Проверки не должны исчезать в Release (NDEBUG).
    if (current_version.empty()) {
        std::cerr << "FAIL: version() returned an empty string\n";
        return 1;
    }

    if (current_version != "0.1.0-research") {
        std::cerr << "FAIL: unexpected version: " << current_version << "\n";
        return 1;
    }

    if (current_version.rfind("0.1.0", 0) != 0) {
        std::cerr << "FAIL: version does not start with project version 0.1.0\n";
        return 1;
    }

    std::cout << "arizona_neo_core version: " << current_version << "\n";
    std::cout << "version tests: PASSED\n";
    return 0;
}