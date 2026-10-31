cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "macOS architectures")

project(Ferrite VERSION 0.1.0 LANGUAGES C CXX)

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
    set(FERRITE_FORMATS VST3 AU Standalone)
else()
    set(FERRITE_FORMATS VST3 Standalone)
endif()

juce_add_plugin(Ferrite
    COMPANY_NAME "Personal"
    BUNDLE_ID "com.personal.ferrite"
    PLUGIN_MANUFACTURER_CODE Prsn
    PLUGIN_CODE Frt1
    FORMATS ${FERRITE_FORMATS}
    PRODUCT_NAME "Ferrite"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

set(FERRITE_SOURCES
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
    Source/UI/ModulationPanel.cpp)

target_sources(Ferrite PRIVATE ${FERRITE_SOURCES})
target_include_directories(Ferrite PRIVATE Source)

set(FERRITE_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_FLAC=1
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_compile_definitions(Ferrite PUBLIC ${FERRITE_DEFS})

set(FERRITE_LIBS
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_dsp)

target_link_libraries(Ferrite
    PRIVATE ${FERRITE_LIBS}
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Консольный тест: рендер нот в WAV + проверка на NaN/клиппинг/скорость
option(FERRITE_BUILD_TESTS "Build offline render test" OFF)
if(FERRITE_BUILD_TESTS)
    juce_add_console_app(FerriteRenderTest PRODUCT_NAME "FerriteRenderTest")
    target_sources(FerriteRenderTest PRIVATE Tests/RenderTest.cpp ${FERRITE_SOURCES})
    target_include_directories(FerriteRenderTest PRIVATE Source)
    target_compile_definitions(FerriteRenderTest PRIVATE ${FERRITE_DEFS} FERRITE_TEST_BUILD=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(FerriteRenderTest PRIVATE ${FERRITE_LIBS} juce::juce_recommended_config_flags)
endif()
