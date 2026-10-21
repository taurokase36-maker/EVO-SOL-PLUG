#!/bin/bash
# Compila el plugin de OGRO 2 con JUCE en una Mac sin CMake ni Xcode y:
#  - sin argumento "tests": guarda una captura PNG del editor (para revisar el diseño)
#  - con "tests" como primer argumento: corre los tests del plugin (plugin_tests.cpp)
# Uso: bash ogro2/tools/snapshot/build.sh [salida.png [escala]] | tests
# JUCE: build-ogro2/juce-src (git clone --depth 1 --branch 9.0.3) o la variable JUCE_DIR.
set -e
REPO="$(cd "$(dirname "$0")/../../.." && pwd)"
JUCE="${JUCE_DIR:-$REPO/build-ogro2/juce-src}/modules"
OBJ="$REPO/build-ogro2/snapshot"
mkdir -p "$OBJ"
MODULES="juce_core juce_events juce_data_structures juce_graphics juce_gui_basics juce_gui_extra juce_audio_basics juce_audio_processors_headless juce_audio_processors"
DEFS=(-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DJUCE_STANDALONE_APPLICATION=1 -DJUCE_WEB_BROWSER=0 -DJUCE_USE_CURL=0
 -DJUCE_PLUGINHOST_VST3=0 -DJUCE_PLUGINHOST_AU=0 -DJUCE_PLUGINHOST_LV2=0 -DJUCE_PLUGINHOST_ARA=0 -DJUCE_DISPLAY_SPLASH_SCREEN=0
 '-DJucePlugin_Name="OGRO 2"' '-DJucePlugin_VersionString="2.0.0"' -DNDEBUG=1)
for m in $MODULES; do DEFS+=("-DJUCE_MODULE_AVAILABLE_$m=1"); done
FLAGS=(-std=c++17 -O1 "-I$JUCE" "-I$REPO/ogro2/Source" "-I$REPO/ogro2/Source/dsp" "-I$OBJ" "${DEFS[@]}")
python3 "$REPO/ogro2/tools/snapshot/binarydata.py" "$OBJ" "$REPO"/ogro2/Resources/fonts/*.ttf
for m in $MODULES; do
  for src in "$JUCE/$m"/*.mm "$JUCE/$m"/*.cpp "$JUCE/$m"/*.c; do
    [ -f "$src" ] || continue
    base="$(basename "$src")"; stem="${base%.*}"
    case "$base" in *.cpp) [ -f "$JUCE/$m/$stem.mm" ] && continue ;; esac
    case "$stem" in *_ara|*_lv2_libs) continue ;; esac
    EXTRA=""
    if [ "$(uname -m)" = "x86_64" ]; then case "$stem" in *_avx2) EXTRA="-mavx2" ;; *_sse41) EXTRA="-msse4.1" ;; esac; fi
    [ -f "$OBJ/$stem.o" ] && continue
    echo "Compilando $base (solo la primera vez)..."
    case "$base" in
      *.c) clang $EXTRA "-I$JUCE" "${DEFS[@]}" -O1 -c "$src" -o "$OBJ/$stem.o" 2>&1 | grep -E " error" || true ;;
      *) clang++ "${FLAGS[@]}" -x objective-c++ -c "$src" -o "$OBJ/$stem.o" 2>&1 | grep -E " error" || true ;;
    esac
  done
done
echo "Compilando OGRO 2..."
clang++ "${FLAGS[@]}" -c "$OBJ/BinaryData.cpp" -o "$OBJ/BinaryData.o"
clang++ "${FLAGS[@]}" -x objective-c++ -c "$REPO/ogro2/Source/PluginProcessor.cpp" -o "$OBJ/PluginProcessor.o"
clang++ "${FLAGS[@]}" -x objective-c++ -c "$REPO/ogro2/Source/PluginEditor.cpp" -o "$OBJ/PluginEditor.o"
FRAMEWORKS=(-framework Cocoa -framework CoreText -framework CoreGraphics -framework QuartzCore
  -framework IOKit -framework Carbon -framework Accelerate -framework AudioToolbox -framework CoreAudio -framework CoreMIDI
  -framework Security -framework Metal -framework MetalKit -framework UniformTypeIdentifiers -framework CoreImage -lobjc)
# Los tests y las capturas nunca leen ni escriben la librería de samples del usuario
export OGRO2_SAMPLES="${OGRO2_SAMPLES:-$OBJ/samples_prueba}"
mkdir -p "$OGRO2_SAMPLES"
if [ "$1" = "tests" ]; then
  clang++ "${FLAGS[@]}" -x objective-c++ -c "$REPO/ogro2/tools/snapshot/plugin_tests.cpp" -o "$OBJ/plugin_tests.main"
  clang++ "$OBJ"/*.o "$OBJ/plugin_tests.main" -o "$OBJ/plugin_tests" "${FRAMEWORKS[@]}"
  "$OBJ/plugin_tests"
  exit $?
fi
clang++ "${FLAGS[@]}" -x objective-c++ -c "$REPO/ogro2/tools/snapshot/snapshot.cpp" -o "$OBJ/snapshot.main"
clang++ "$OBJ"/*.o "$OBJ/snapshot.main" -o "$OBJ/snapshot" -framework Cocoa -framework CoreText -framework CoreGraphics -framework QuartzCore \
  -framework IOKit -framework Carbon -framework Accelerate -framework AudioToolbox -framework CoreAudio -framework CoreMIDI \
  -framework Security -framework Metal -framework MetalKit -framework UniformTypeIdentifiers -framework CoreImage -lobjc
"$OBJ/snapshot" "${1:-$OBJ/editor.png}" "${2:-1}"
