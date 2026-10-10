#!/bin/bash
# Instalador de EVO SOL para Mac (copia el VST3 y el AU a tu carpeta de plugins)
cd "$(dirname "$0")" || exit 1

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR" "$AU_DIR"

echo "Instalando EVO SOL..."
rm -rf "$VST3_DIR/EVO SOL.vst3" "$AU_DIR/EVO SOL.component"
cp -R "EVO SOL.vst3" "$VST3_DIR/"
cp -R "EVO SOL.component" "$AU_DIR/"

# Quita la marca de "descargado de internet" para que macOS no lo bloquee
xattr -dr com.apple.quarantine "$VST3_DIR/EVO SOL.vst3" 2>/dev/null
xattr -dr com.apple.quarantine "$AU_DIR/EVO SOL.component" 2>/dev/null
[ -d "EVO SOL.app" ] && xattr -dr com.apple.quarantine "EVO SOL.app" 2>/dev/null

# Refresca la caché de Audio Units
killall -9 AudioComponentRegistrar 2>/dev/null

echo ""
echo "Listo. EVO SOL quedó instalado en:"
echo "  $VST3_DIR/EVO SOL.vst3"
echo "  $AU_DIR/EVO SOL.component"
echo ""
echo "Abrí Ableton Live 12 > Settings > Plug-Ins y tocá 'Rescan'."
echo "Lo vas a encontrar en Plug-Ins > EVO THE SUN > EVO SOL."
echo ""
read -n 1 -s -r -p "Tocá cualquier tecla para cerrar..."
