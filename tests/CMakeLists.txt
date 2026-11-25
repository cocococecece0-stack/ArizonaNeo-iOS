add_executable(game_bridge_test game_bridge_test.cpp)
target_link_libraries(game_bridge_test PRIVATE arizona_neo_core)
target_compile_features(game_bridge_test PRIVATE cxx_std_17)
add_test(NAME game_bridge_test COMMAND game_bridge_test)

add_executable(version_test version_test.cpp)
target_link_libraries(version_test PRIVATE arizona_neo_core)
target_compile_features(version_test PRIVATE cxx_std_17)
add_test(NAME version_test COMMAND version_test)

add_executable(logger_test logger_test.cpp)
target_link_libraries(logger_test PRIVATE arizona_neo_core)
target_compile_features(logger_test PRIVATE cxx_std_17)
add_test(NAME logger_test COMMAND logger_test)

add_executable(filesystem_test filesystem_test.cpp)
target_link_libraries(filesystem_test PRIVATE arizona_neo_core)
target_compile_features(filesystem_test PRIVATE cxx_std_17)
add_test(NAME filesystem_test COMMAND filesystem_test)

add_executable(scheduler_test scheduler_test.cpp)
target_link_libraries(scheduler_test PRIVATE arizona_neo_core)
target_compile_features(scheduler_test PRIVATE cxx_std_17)
add_test(NAME scheduler_test COMMAND scheduler_test)

add_executable(event_bus_test event_bus_test.cpp)
target_link_libraries(event_bus_test PRIVATE arizona_neo_core)
target_compile_features(event_bus_test PRIVATE cxx_std_17)
add_test(NAME event_bus_test COMMAND event_bus_test)

add_executable(script_manager_test script_manager_test.cpp)
target_link_libraries(script_manager_test PRIVATE arizona_neo_core)
target_compile_features(script_manager_test PRIVATE cxx_std_17)
add_test(NAME script_manager_test COMMAND script_manager_test)

# LuaJIT runtime test is only registered when the LuaJIT integration is enabled.
# Without the option the test source self-skips, but not building it at all keeps
# the LuaJIT OFF configuration free of any LuaJIT dependency.
if(ARIZONA_NEO_WITH_LUAJIT)
    add_executable(lua_runtime_test lua_runtime_test.cpp)
    target_link_libraries(lua_runtime_test PRIVATE arizona_neo_core)
    target_compile_features(lua_runtime_test PRIVATE cxx_std_17)
    add_test(NAME lua_runtime_test COMMAND lua_runtime_test)

    add_executable(portable_script_loader_test portable_script_loader_test.cpp)
    target_link_libraries(portable_script_loader_test PRIVATE arizona_neo_core)
    target_compile_features(portable_script_loader_test PRIVATE cxx_std_17)
    add_test(NAME portable_script_loader_test COMMAND portable_script_loader_test)

    add_executable(script_manager_integration_test script_manager_integration_test.cpp)
    target_link_libraries(script_manager_integration_test PRIVATE arizona_neo_core)
    target_compile_features(script_manager_integration_test PRIVATE cxx_std_17)
    add_test(NAME script_manager_integration_test COMMAND script_manager_integration_test
             WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()
