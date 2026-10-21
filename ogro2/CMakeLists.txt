cmake_minimum_required(VERSION 3.22)

# OGRO 2 es un plugin aparte: convive con OGRO 1.0 (carpeta ogro/) en el mismo Ableton.
# Identidad propia (nombre, código, bundle y target) y DSP propio en Source/dsp.
project(OGRO2 VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version" FORCE)
    endif()
endif()

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(OGRO2
    COMPANY_NAME "EVO THE SUN"
    BUNDLE_ID com.evothesun.ogro2
    PLUGIN_MANUFACTURER_CODE Evts
    PLUGIN_CODE Ogr2
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "OGRO 2"
    DESCRIPTION "Groovebox analogico 2: minimal house, deep y micro-house"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Drum Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
)

target_sources(OGRO2 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_include_directories(OGRO2 PRIVATE Source Source/dsp)

# Tipografías incrustadas (licencia OFL): Michroma, Space Grotesk y VT323
juce_add_binary_data(OGRO2_BinaryData SOURCES
    Resources/fonts/MichromaRegular.ttf
    Resources/fonts/SpaceGroteskBold.ttf
    Resources/fonts/SpaceGroteskMedium.ttf
    Resources/fonts/VT323Regular.ttf
)

# Símbolos ocultos: OGRO y OGRO 2 se cargan en el mismo proceso de Ableton
set_target_properties(OGRO2 PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

target_compile_definitions(OGRO2 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
)

target_link_libraries(OGRO2
    PRIVATE
        OGRO2_BinaryData
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
