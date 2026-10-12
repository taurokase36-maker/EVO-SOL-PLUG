#!/bin/bash
# Instalador de OGRO para Mac (copia el VST3 y el AU a tu carpeta de plugins)
cd "$(dirname "$0")" || exit 1

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR" "$AU_DIR" "$HOME/Music/OGRO Presets"

echo "Instalando OGRO..."
rm -rf "$VST3_DIR/OGRO.vst3" "$AU_DIR/OGRO.component"
cp -R "OGRO.vst3" "$VST3_DIR/"
cp -R "OGRO.component" "$AU_DIR/"

# Quita la marca de "descargado de internet" para que macOS no lo bloquee
xattr -dr com.apple.quarantine "$VST3_DIR/OGRO.vst3" 2>/dev/null
xattr -dr com.apple.quarantine "$AU_DIR/OGRO.component" 2>/dev/null
[ -d "OGRO.app" ] && xattr -dr com.apple.quarantine "OGRO.app" 2>/dev/null

killall -9 AudioComponentRegistrar 2>/dev/null

echo ""
echo "Listo. OGRO quedó instalado en:"
echo "  $VST3_DIR/OGRO.vst3"
echo "  $AU_DIR/OGRO.component"
echo ""
echo "Abrí Ableton Live 12 > Settings > Plug-Ins y tocá 'Rescan'."
echo "Lo vas a encontrar en Plug-Ins > EVO THE SUN > OGRO (es un instrumento: va en una pista MIDI)."
echo ""
read -n 1 -s -r -p "Tocá cualquier tecla para cerrar..."
