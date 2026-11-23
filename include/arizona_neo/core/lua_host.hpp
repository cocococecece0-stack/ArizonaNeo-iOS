#pragma once
#include <string_view>

namespace arizona_neo::core {

class LuaHost {
public:
    virtual ~LuaHost() = default;
    virtual bool initialize() = 0;
    virtual bool execute(
        std::string_view source,
        std::string_view chunk_name
    ) = 0;
    virtual void shutdown() noexcept = 0;
};

}
