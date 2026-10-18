cmake_minimum_required(VERSION 3.15)

project(ripple VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(ripple_core STATIC
    src/analyzer.cpp
    src/app.cpp
    src/baseline.cpp
    src/graph.cpp
    src/hashing.cpp
    src/scanner.cpp
    src/tui.cpp
    src/utils.cpp
)
target_include_directories(ripple_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(MSVC)
    target_compile_options(ripple_core PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(ripple_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(ripple src/main.cpp)
target_link_libraries(ripple PRIVATE ripple_core)
