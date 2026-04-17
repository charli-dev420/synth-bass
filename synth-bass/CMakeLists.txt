cmake_minimum_required(VERSION 3.22)

project(UWdeVST_bass VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(MBS_ENABLE_VST2 "Enable legacy VST2 format (requires legacy JUCE + VST2 SDK)" OFF)

# ---------------------------------------------------------------------------
# JUCE: use local copy if present, otherwise fetch from GitHub
# ---------------------------------------------------------------------------
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

set(MBS_PLUGIN_FORMATS Standalone VST3)
if (MBS_ENABLE_VST2)
    list(APPEND MBS_PLUGIN_FORMATS VST)
endif()

set(UWDEVST_SHARED_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets versions png" CACHE PATH "Path to the shared UI/assets directory")
if (IS_ABSOLUTE "${UWDEVST_SHARED_ASSETS_DIR}")
    set(ASSETS_DIR "${UWDEVST_SHARED_ASSETS_DIR}")
else()
    get_filename_component(ASSETS_DIR "${UWDEVST_SHARED_ASSETS_DIR}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

juce_add_plugin(UWdeVST_bass
    COMPANY_NAME "Musique"
    BUNDLE_ID "com.uwdevst.bass"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE MQBS
    PLUGIN_CODE MBS1
    FORMATS ${MBS_PLUGIN_FORMATS}
    PRODUCT_NAME "UWdeVST_bass"
    ICON_BIG   "${ASSETS_DIR}/logo,favic/logo_bass.png"
    ICON_SMALL "${ASSETS_DIR}/logo,favic/logo_bass.png"
)

juce_generate_juce_header(UWdeVST_bass)

target_sources(UWdeVST_bass PRIVATE
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/Engine/BassDefs.cpp
    Source/Engine/BassDefs.h
    Source/Engine/BassVoice.cpp
    Source/Engine/BassVoice.h
    Source/Engine/FactoryPresets.cpp
    Source/Engine/FactoryPresets.h
    "${CMAKE_CURRENT_SOURCE_DIR}/../Shared/SynthCommon.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Shared/SynthCommon.h"
)

target_compile_definitions(UWdeVST_bass PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

# ---------------------------------------------------------------------------
# MSVC: Release optimizations
# ---------------------------------------------------------------------------
if (MSVC)
    target_compile_options(UWdeVST_bass PRIVATE
        /utf-8
        $<$<CONFIG:Release>:/O2 /fp:fast /GL>
    )
    target_link_options(UWdeVST_bass PRIVATE
        $<$<CONFIG:Release>:/LTCG>
    )
endif ()

# ---------------------------------------------------------------------------
# Binary assets (background texture only — icons not rendered in current UI)
# ---------------------------------------------------------------------------
juce_add_binary_data(BassAssets SOURCES
    "${ASSETS_DIR}/fond_basse.png"
)

target_link_libraries(UWdeVST_bass PRIVATE
    BassAssets
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
    juce::juce_recommended_lto_flags
)

juce_add_console_app(UWdeVST_bass_renderer
    PRODUCT_NAME "UWdeVST_bass_renderer"
)

juce_generate_juce_header(UWdeVST_bass_renderer)

target_sources(UWdeVST_bass_renderer PRIVATE
    Source/Render/BassManifestRenderer.cpp
    Source/Engine/BassDefs.cpp
    Source/Engine/BassDefs.h
    Source/Engine/BassVoice.cpp
    Source/Engine/BassVoice.h
    Source/Engine/FactoryPresets.cpp
    Source/Engine/FactoryPresets.h
)

target_compile_definitions(UWdeVST_bass_renderer PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(UWdeVST_bass_renderer PRIVATE
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
    juce::juce_recommended_lto_flags
)

juce_add_console_app(UWdeVST_bass_tests
    PRODUCT_NAME "UWdeVST_bass_tests"
)

juce_generate_juce_header(UWdeVST_bass_tests)

target_sources(UWdeVST_bass_tests PRIVATE
    Source/Tests/BassProductionTests.cpp
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/Engine/BassDefs.cpp
    Source/Engine/BassDefs.h
    Source/Engine/BassVoice.cpp
    Source/Engine/BassVoice.h
    Source/Engine/FactoryPresets.cpp
    Source/Engine/FactoryPresets.h
    "${CMAKE_CURRENT_SOURCE_DIR}/../Shared/SynthCommon.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Shared/SynthCommon.h"
)

target_compile_definitions(UWdeVST_bass_tests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    UWDEVST_BASS_TEST_BUILD=1
)

target_link_libraries(UWdeVST_bass_tests PRIVATE
    BassAssets
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
    juce::juce_recommended_lto_flags
)
