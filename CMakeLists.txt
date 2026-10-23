
cmake_minimum_required(VERSION 3.20)

project(Numerick
    VERSION 0.1.0
    DESCRIPTION "A personal AI assistant"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(numerick
    src/main.cpp
)

target_compile_options(numerick PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

enable_testing()
