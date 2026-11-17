cmake_minimum_required(VERSION 3.22)

# On a Mac, default to a universal (Apple Silicon + Intel) build.
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Mac architectures")
    endif()
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
endif()

project(XoxicoaFX VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(XFX_BUILD_TESTS "Build the offline render test harness" OFF)
set(XFX_JUCE_DIR "" CACHE PATH "Optional path to a local JUCE checkout (otherwise fetched from GitHub)")

if(XFX_JUCE_DIR)
    add_subdirectory(${XFX_JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(XoxicoaFX
    COMPANY_NAME              "Xoxicoa"
    PRODUCT_NAME              "Xoxicoa FX"
    BUNDLE_ID                 com.xoxicoa.xoxicoafx
    PLUGIN_MANUFACTURER_CODE  Xoxi
    PLUGIN_CODE               XxFx
    FORMATS                   AU VST3 Standalone
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           Fx Distortion
    AU_MAIN_TYPE              kAudioUnitType_Effect
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Xoxicoa FX Standalone uses audio input to process sound.")

target_sources(XoxicoaFX PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(XoxicoaFX PRIVATE Source)

target_compile_definitions(XoxicoaFX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(XoxicoaFX
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# Link-time optimisation on Mac (Apple clang handles it cleanly).
if(APPLE)
    target_link_libraries(XoxicoaFX PUBLIC juce::juce_recommended_lto_flags)
endif()

if(XFX_BUILD_TESTS)
    juce_add_console_app(XoxicoaFXRenderTest PRODUCT_NAME "XoxicoaFXRenderTest")
    target_sources(XoxicoaFXRenderTest PRIVATE tests/RenderTest.cpp)
    target_compile_definitions(XoxicoaFXRenderTest PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_AU=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(XoxicoaFXRenderTest
        PRIVATE
            juce::juce_audio_processors
            juce::juce_audio_formats
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()

# Headless screenshot of the editor (development only)
if(XFX_BUILD_TESTS)
    juce_add_console_app(XoxicoaFXUISnapshot PRODUCT_NAME "XoxicoaFXUISnapshot")
    target_sources(XoxicoaFXUISnapshot PRIVATE
        tests/UISnapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(XoxicoaFXUISnapshot PRIVATE Source)
    target_compile_definitions(XoxicoaFXUISnapshot PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(XoxicoaFXUISnapshot PRIVATE
        juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags)
endif()
