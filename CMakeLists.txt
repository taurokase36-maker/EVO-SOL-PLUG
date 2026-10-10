cmake_minimum_required(VERSION 3.22)

project(EVO_SOL VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Mac: binario universal (Intel + Apple Silicon) compatible desde macOS 10.13
if(APPLE)
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version" FORCE)
    endif()
endif()

# JUCE se descarga automáticamente (o se usa una copia local con -DFETCHCONTENT_SOURCE_DIR_JUCE=...)
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(EVOSOL
    COMPANY_NAME "EVO THE SUN"
    COMPANY_WEBSITE "https://www.instagram.com/"
    BUNDLE_ID com.evothesun.evosol
    PLUGIN_MANUFACTURER_CODE Evts
    PLUGIN_CODE Esol
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "EVO SOL"
    DESCRIPTION "Suite vocal: afinacion, cadena analogica, ear candy e inteligencia adaptativa"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics "Pitch Shift"
    AU_MAIN_TYPE kAudioUnitType_Effect
)

target_sources(EVOSOL PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_include_directories(EVOSOL PRIVATE Source)

target_compile_definitions(EVOSOL PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

target_link_libraries(EVOSOL
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
