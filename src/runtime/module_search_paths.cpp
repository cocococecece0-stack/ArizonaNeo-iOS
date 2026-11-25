#include "arizona_neo/runtime/module_search_paths.hpp"

namespace arizona_neo::runtime {

std::vector<std::string> module_candidates(const std::string& module_name) {
    std::string name = module_name;
    if (name.rfind("lib.", 0) == 0) {
        name = name.substr(4);
    }
    for (char& character : name) {
        if (character == '.') character = '/';
    }

    std::vector<std::string> candidates;
    candidates.push_back("lib/" + name + ".lua");
    candidates.push_back("lib/" + name + "/init.lua");
    candidates.push_back(name + ".lua");
    candidates.push_back(name + "/init.lua");
    return candidates;
}

} // namespace arizona_neo::runtime
