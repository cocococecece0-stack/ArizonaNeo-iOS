cmake_minimum_required(VERSION 3.24)
project(ArizonaNeoIOS VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ARIZONA_NEO_WITH_LUAJIT "Enable LuaJIT runtime integration" OFF)

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
    )
endif()

add_library(arizona_neo_core STATIC
    src/core/version.cpp
    src/core/lua_host.cpp
    src/core/logger.cpp
    src/core/filesystem.cpp
    src/core/scheduler.cpp
    src/core/event_bus.cpp
    src/game/game_bridge.cpp
    src/core/script_manager.cpp
    ${LUAJIT_SOURCES}
)

target_include_directories(arizona_neo_core PUBLIC include)
if(ARIZONA_NEO_WITH_LUAJIT)
    target_include_directories(arizona_neo_core PUBLIC ${LUAJIT_INCLUDE_DIR})
    target_link_libraries(arizona_neo_core PUBLIC ${LUAJIT_LIBRARY} m)
    target_compile_definitions(arizona_neo_core PUBLIC ARIZONA_NEO_WITH_LUAJIT=1)
endif()

target_compile_features(arizona_neo_core PUBLIC cxx_std_17)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
