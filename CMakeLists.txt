cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "macOS architectures")

project(Mist VERSION 1.0.1 LANGUAGES C CXX)

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
    set(MIST_FORMATS VST3 AU Standalone)
else()
    set(MIST_FORMATS VST3 Standalone)
endif()

juce_add_plugin(Mist
    COMPANY_NAME "Personal"
    BUNDLE_ID "com.personal.mist"
    PLUGIN_MANUFACTURER_CODE Prsn
    PLUGIN_CODE Mist
    FORMATS ${MIST_FORMATS}
    PRODUCT_NAME "Mist"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

set(MIST_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Params.cpp
    Source/Algorithms.cpp
    Source/PresetManager.cpp
    Source/Randomizer.cpp
    Source/FactoryPresets.cpp
    Source/DSP/Wavetable.cpp
    Source/DSP/SampleAsset.cpp
    Source/DSP/Synth.cpp
    Source/UI/LookAndFeel.cpp
    Source/UI/Widgets.cpp
    Source/UI/OperatorPanel.cpp
    Source/UI/RoutingPanel.cpp
    Source/UI/ModulationPanel.cpp
    Source/UI/Visualizer.cpp
    Source/UI/About.cpp)

target_sources(Mist PRIVATE ${MIST_SOURCES})
target_include_directories(Mist PRIVATE Source)

set(MIST_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_FLAC=1
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_compile_definitions(Mist PUBLIC ${MIST_DEFS})

# Medeo Pixel (SIL OFL) — логотип и название Mist, встраивается в плагин
juce_add_binary_data(MistFonts
    NAMESPACE MistFonts
    HEADER_NAME BinaryData.h
    SOURCES
        Resources/Fonts/MedeoPixel-Regular.ttf)
set_target_properties(MistFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(MIST_LIBS
    MistFonts
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_dsp)

target_link_libraries(Mist
    PRIVATE ${MIST_LIBS}
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Консольный тест: рендер нот в WAV + проверка на NaN/клиппинг/скорость
option(MIST_BUILD_TESTS "Build offline render test" OFF)
if(MIST_BUILD_TESTS)
    juce_add_console_app(MistRenderTest PRODUCT_NAME "MistRenderTest")
    target_sources(MistRenderTest PRIVATE Tests/RenderTest.cpp ${MIST_SOURCES})
    target_include_directories(MistRenderTest PRIVATE Source)
    target_compile_definitions(MistRenderTest PRIVATE ${MIST_DEFS} MIST_TEST_BUILD=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(MistRenderTest PRIVATE ${MIST_LIBS} juce::juce_recommended_config_flags)
endif()
