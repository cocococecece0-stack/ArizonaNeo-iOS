#pragma once
#include "arizona_neo/runtime/lua_runtime_contract.hpp"

#include <functional>
#include <string>
#include <string_view>

struct lua_State;

namespace arizona_neo::core {

class LuaHost {
public:
    virtual ~LuaHost() = default;

    virtual bool initialize() = 0;
    virtual bool execute(std::string_view source, std::string_view chunk_name) = 0;
    virtual void shutdown() noexcept = 0;

    virtual arizona_neo::runtime::LuaExecutionResult execute_detailed(
        std::string_view source, std::string_view chunk_name) = 0;

    virtual bool register_function(const char* name, arizona_neo::runtime::LuaBridgeFunction function) = 0;
    virtual const std::string& last_error() const noexcept = 0;
    virtual bool is_initialized() const noexcept = 0;

    virtual double get_global_number(const char* name) const = 0;
    virtual std::string get_global_string(const char* name) const = 0;
    virtual int stack_top() const noexcept = 0;

    virtual lua_State* state() const noexcept = 0;

    using ModuleSourceReader = std::function<bool(const std::string&, std::string&)>;
    virtual void set_module_source_reader(ModuleSourceReader) {}
};

} // namespace arizona_neo::core
