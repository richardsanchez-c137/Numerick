
cmake_minimum_required(VERSION 3.20)

project(Numerick
    VERSION 0.2.0
    DESCRIPTION "A personal AI assistant"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(numerick
    src/main.cpp
    src/core/ai_client.cpp
)

target_include_directories(numerick PRIVATE include)

target_link_libraries(numerick PRIVATE winhttp)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(numerick PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

enable_testing()
