# polypitch~ : the PolyPitch engine as a Max object (macOS .mxo, Windows .mxe64).
# Needs Cycling '74's max-sdk-base (MIT): https://github.com/Cycling74/max-sdk-base
#   cmake -S . -B ../../../../../build/max -DC74_SDK=/path/to/max-sdk-base -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
# The object is written to max/PolyPitch/externals/ (the package's externals folder).
cmake_minimum_required(VERSION 3.19)

set(C74_SDK "" CACHE PATH "Path to a checkout of Cycling '74's max-sdk-base")
if(NOT EXISTS "${C74_SDK}/script/max-pretarget.cmake")
    message(FATAL_ERROR "Set -DC74_SDK=/path/to/max-sdk-base (https://github.com/Cycling74/max-sdk-base)")
endif()

set(AUTHOR_DOMAIN "com.benjuodvalkis")
set(BUNDLE_IDENTIFIER "polypitch-tilde")
include(${C74_SDK}/script/max-pretarget.cmake)

# the SDK script asks for macOS 10.11, which current toolchains no longer support; 11.0 matches the plug-in
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment version" FORCE)
endif()

include_directories(
    "${MAX_SDK_INCLUDES}"
    "${MAX_SDK_MSP_INCLUDES}"
    "${MAX_SDK_JIT_INCLUDES}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../engine"
)

add_library(${PROJECT_NAME} MODULE polypitch_tilde.cpp)
# the engine's inner loops are plain sums; let the compiler reorder and vectorise them
if(NOT MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE -O3 -ffast-math -fno-finite-math-only)
endif()

include(${C74_SDK}/script/max-posttarget.cmake)
