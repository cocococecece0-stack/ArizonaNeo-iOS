cmake_minimum_required(VERSION 3.24)
project(ios_sim_runtime_test LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Standalone harness: links the already-built Simulator Core archive and the
# Simulator LuaJIT archive. No fake runtime, no Linux artifacts.
add_executable(sim_runtime_test sim_runtime_test.cpp)
target_compile_definitions(sim_runtime_test PRIVATE ARIZONA_NEO_WITH_LUAJIT=1)
target_include_directories(sim_runtime_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../include"
    "${LUAJIT_INCLUDE_DIR}"
)
target_link_libraries(sim_runtime_test PRIVATE "${CORE_LIB}" "${LUAJIT_LIB}" m)
