cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "macOS architectures")

project(Loom VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: локально можно передать -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.10
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(LOOM_FORMATS VST3 AU Standalone)
else()
    set(LOOM_FORMATS VST3 Standalone)
endif()

juce_add_plugin(Loom
    COMPANY_NAME "Personal"
    BUNDLE_ID "com.personal.loom"
    PLUGIN_MANUFACTURER_CODE Prsn
    PLUGIN_CODE Loom
    FORMATS ${LOOM_FORMATS}
    PRODUCT_NAME "Loom"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

set(LOOM_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Params.cpp
    Source/Algorithms.cpp
    Source/PresetManager.cpp
    Source/FactoryPresets.cpp
    Source/DSP/Wavetable.cpp
    Source/DSP/SampleAsset.cpp
    Source/DSP/Synth.cpp
    Source/UI/LookAndFeel.cpp
    Source/UI/Widgets.cpp
    Source/UI/OperatorPanel.cpp
    Source/UI/RoutingPanel.cpp
    Source/UI/ModulationPanel.cpp
    Source/UI/Visualizer.cpp)

target_sources(Loom PRIVATE ${LOOM_SOURCES})
target_include_directories(Loom PRIVATE Source)

set(LOOM_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_FLAC=1
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_compile_definitions(Loom PUBLIC ${LOOM_DEFS})

set(LOOM_LIBS
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_dsp)

target_link_libraries(Loom
    PRIVATE ${LOOM_LIBS}
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Консольный тест: рендер нот в WAV + проверка на NaN/клиппинг/скорость
option(LOOM_BUILD_TESTS "Build offline render test" OFF)
if(LOOM_BUILD_TESTS)
    juce_add_console_app(LoomRenderTest PRODUCT_NAME "LoomRenderTest")
    target_sources(LoomRenderTest PRIVATE Tests/RenderTest.cpp ${LOOM_SOURCES})
    target_include_directories(LoomRenderTest PRIVATE Source)
    target_compile_definitions(LoomRenderTest PRIVATE ${LOOM_DEFS} LOOM_TEST_BUILD=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(LoomRenderTest PRIVATE ${LOOM_LIBS} juce::juce_recommended_config_flags)
endif()
