cmake_minimum_required(VERSION 3.24)

project(NumerickNvigiVoiceWorker
    VERSION 1.0.0
    DESCRIPTION "Native NVIGI Qwen3-TTS worker for NUMERICK"
    LANGUAGES CXX
)

if(NOT WIN32)
    message(FATAL_ERROR "The NUMERICK NVIGI voice worker currently supports Windows only.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(NVIGI_PACK_ROOT "$ENV{USERPROFILE}/NumerickVoiceLab/NVIDIA-Qwen3-TTS/_qwen_tts_pack"
    CACHE PATH "Root of the NVIDIA Qwen3-TTS pack")

set(NVIGI_PACK_INCLUDE_DIR "${NVIGI_PACK_ROOT}/include")
set(NVIGI_CORE_INCLUDE_DIR "${NVIGI_PACK_ROOT}/nvigi_core/include")

foreach(required_header
        "${NVIGI_PACK_INCLUDE_DIR}/nvigi_tts_qwen3.h"
        "${NVIGI_CORE_INCLUDE_DIR}/nvigi.h"
        "${NVIGI_CORE_INCLUDE_DIR}/nvigi_ai.h"
        "${NVIGI_CORE_INCLUDE_DIR}/nvigi_cpu.h")
    if(NOT EXISTS "${required_header}")
        message(FATAL_ERROR "Required NVIGI header not found: ${required_header}")
    endif()
endforeach()

add_executable(numerick-nvigi-voice-worker
    src/main.cpp
)

target_include_directories(numerick-nvigi-voice-worker PRIVATE
    "${NVIGI_PACK_INCLUDE_DIR}"
    "${NVIGI_CORE_INCLUDE_DIR}"
)

target_compile_definitions(numerick-nvigi-voice-worker PRIVATE
    UNICODE
    _UNICODE
)

target_compile_options(numerick-nvigi-voice-worker PRIVATE
    /W4
    /permissive-
    /EHsc
    /utf-8
)

# The NVIGI framework is loaded dynamically at runtime. This avoids linking
# an MSVC import library into NUMERICK's separate MSYS2/MinGW application.
target_link_libraries(numerick-nvigi-voice-worker PRIVATE
    winmm
)

set_target_properties(numerick-nvigi-voice-worker PROPERTIES
    OUTPUT_NAME "numerick-nvigi-voice-worker"
)
