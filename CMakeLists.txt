
cmake_minimum_required(VERSION 3.20)

project(Numerick
    VERSION 0.3.0
    DESCRIPTION "A personal AI assistant"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(numerick_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    endif()
endfunction()

# Main application
add_executable(numerick
    src/main.cpp
    src/core/ai_client.cpp
    src/core/command_parser.cpp
)

target_include_directories(numerick PRIVATE include)

target_link_libraries(numerick PRIVATE winhttp)

numerick_enable_warnings(numerick)

# Automated command parser tests
enable_testing()

add_executable(command_parser_tests
    tests/command_parser_tests.cpp
    src/core/command_parser.cpp
)

target_include_directories(command_parser_tests PRIVATE include)

numerick_enable_warnings(command_parser_tests)

add_test(
    NAME command_parser_tests
    COMMAND command_parser_tests
)
