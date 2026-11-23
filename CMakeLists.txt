cmake_minimum_required(VERSION 3.24)
project(ArizonaNeoIOS VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(arizona_neo_core STATIC
    src/core/version.cpp
    src/core/lua_host.cpp
)

target_include_directories(arizona_neo_core PUBLIC include)
target_compile_features(arizona_neo_core PUBLIC cxx_std_20)
