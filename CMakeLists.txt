cmake_minimum_required(VERSION 3.24)
project(ArizonaNeoIOS VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ARIZONA_NEO_WITH_LUAJIT "Enable LuaJIT runtime integration" OFF)
option(ARIZONA_NEO_WITH_LUA54 "Enable experimental Lua 5.4 backend" OFF)

set(LUAJIT_SOURCES "")
if(ARIZONA_NEO_WITH_LUAJIT)
    if(NOT LUAJIT_INCLUDE_DIR)
        find_path(LUAJIT_INCLUDE_DIR NAMES lua.h
            PATHS
            "$ENV{LUAJIT_ROOT}/include"
            "${LUAJIT_ROOT}/include"
        )
    endif()
    if(NOT LUAJIT_LIBRARY)
        find_library(LUAJIT_LIBRARY NAMES luajit
            PATHS
            "$ENV{LUAJIT_ROOT}/lib"
            "${LUAJIT_ROOT}/lib"
        )
    endif()
    if(NOT LUAJIT_INCLUDE_DIR OR NOT LUAJIT_LIBRARY)
        message(FATAL_ERROR "LuaJIT not found. Set LUAJIT_INCLUDE_DIR and LUAJIT_LIBRARY (or LUAJIT_ROOT).")
    endif()
    set(LUAJIT_SOURCES
        src/runtime/lua_jit_host.cpp
        src/runtime/portable_script_loader.cpp
        src/runtime/portable_script_runtime.cpp
        src/runtime/neo_loader_api.cpp
        src/runtime/neo_loader_host.cpp
    )
endif()


set(LUA54_SOURCES "")
if(ARIZONA_NEO_WITH_LUA54)
    file(GLOB LUA54_CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua-5.4.7/src/*.c")
    list(REMOVE_ITEM LUA54_CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua-5.4.7/src/lua.c")
    list(REMOVE_ITEM LUA54_CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua-5.4.7/src/luac.c")
    set(LUA54_SOURCES
        ${LUA54_CORE_SOURCES}
        src/runtime/lua54_host.cpp
    )
endif()

add_library(arizona_neo_core STATIC
    src/core/version.cpp
    src/core/lua_host.cpp
    src/core/logger.cpp
    src/core/filesystem.cpp
    src/core/scheduler.cpp
    src/core/event_bus.cpp
    src/core/user_documents_root.cpp
    src/game/game_bridge.cpp
    src/game/graphics_failure_adapter.cpp
    src/runtime/module_search_paths.cpp
    src/core/script_manager.cpp
    ${LUAJIT_SOURCES}
    ${LUA54_SOURCES}
)

target_include_directories(arizona_neo_core PUBLIC include)
if(ARIZONA_NEO_WITH_LUAJIT)
    target_include_directories(arizona_neo_core PUBLIC ${LUAJIT_INCLUDE_DIR})
    target_link_libraries(arizona_neo_core PUBLIC ${LUAJIT_LIBRARY} m)
    target_compile_definitions(arizona_neo_core PUBLIC ARIZONA_NEO_WITH_LUAJIT=1)
endif()
if(ARIZONA_NEO_WITH_LUA54)
    target_include_directories(arizona_neo_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lua-5.4.7/src)
    target_compile_definitions(arizona_neo_core PUBLIC ARIZONA_NEO_WITH_LUA54=1)
endif()

target_compile_features(arizona_neo_core PUBLIC cxx_std_17)
set_target_properties(arizona_neo_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Standalone iOS native bootstrap. Built as a shared library on every platform
# so the same target produces a real ARM64 iOS dylib in CI and a testable .so
# on Linux.
add_library(neo_loader_bootstrap SHARED
    ios/bootstrap/src/bootstrap.cpp
)
target_include_directories(neo_loader_bootstrap PUBLIC ios/bootstrap/include)
target_link_libraries(neo_loader_bootstrap PRIVATE arizona_neo_core)
target_compile_features(neo_loader_bootstrap PUBLIC cxx_std_17)
set_target_properties(neo_loader_bootstrap PROPERTIES
    OUTPUT_NAME "NeoLoaderBootstrap"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR APPLE)
    # Keep the constructor from being dead-stripped by the Apple linker.
    target_link_options(neo_loader_bootstrap PRIVATE
        -Wl,-u,_neo_bootstrap_library_loaded)
endif()

# iOS-only host adapter for the system Documents picker. It is deliberately
# separate from arizona_neo_core so the portable core stays UIKit-free.
if(APPLE)
    add_library(neo_ios_documents_host STATIC
        ios/host/ios_documents_host.mm
    )
    target_include_directories(neo_ios_documents_host PUBLIC
        ios/host
        include
    )
    target_link_libraries(neo_ios_documents_host PRIVATE
        "-framework Foundation"
        "-framework UIKit"
        "-framework UniformTypeIdentifiers"
    )
    target_compile_features(neo_ios_documents_host PUBLIC cxx_std_17)
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
