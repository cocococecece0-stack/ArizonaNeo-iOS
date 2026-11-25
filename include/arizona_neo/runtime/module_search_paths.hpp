#pragma once

#include <string>
#include <vector>

namespace arizona_neo::runtime {

// NeoLoader-style module names use dots (`lib.samp.events`, `dkjson`).
// Returns candidate relative paths inside the user-selected NeoLoader folder,
// in priority order. No file access happens here.
std::vector<std::string> module_candidates(const std::string& module_name);

} // namespace arizona_neo::runtime
