cmake_minimum_required(VERSION 3.22)

# Must be set before project().
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment version")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Target architectures")
endif()

project(PolyPitch VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE 8: a local checkout if there is one (-DJUCE_DIR=/path/to/JUCE, default ~/JUCE), otherwise fetched.
# JUCE is used under its AGPLv3 option; see THIRD-PARTY.md for what that means for binaries.
set(JUCE_DIR "$ENV{HOME}/JUCE" CACHE PATH "JUCE checkout")
if(EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.10 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(PolyPitch
    COMPANY_NAME                "Ben Juodvalkis"
    PLUGIN_MANUFACTURER_CODE    Bjuo
    PLUGIN_CODE                 PlyP
    FORMATS                     VST3 AU
    PRODUCT_NAME                "PolyPitch"                         # the bundle's file name; scripts and tests use it
    PLUGIN_NAME                 "Ben's Polyphonic Pitch Shifter"    # the name hosts show
    DESCRIPTION                 "Polyphonic pitch shifter for live playing"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx Pitch
    AU_MAIN_TYPE                kAudioUnitType_Effect
    BUNDLE_ID                   "com.benjuodvalkis.polypitch"
    COPY_PLUGIN_AFTER_BUILD     FALSE
)

target_sources(PolyPitch PRIVATE Source/PluginProcessor.cpp)
# the engine's inner loops are plain sums; let the compiler reorder and vectorise them
set_source_files_properties(Source/PluginProcessor.cpp PROPERTIES COMPILE_OPTIONS "-ffast-math;-fno-finite-math-only")
target_include_directories(PolyPitch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source ${CMAKE_CURRENT_SOURCE_DIR}/../engine)
juce_generate_juce_header(PolyPitch)

target_compile_definitions(PolyPitch PUBLIC
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(PolyPitch
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
