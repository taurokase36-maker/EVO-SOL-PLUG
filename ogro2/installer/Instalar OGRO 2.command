#!/bin/bash
# Instalador de OGRO 2 para Mac (copia el VST3 y el AU a tu carpeta de plugins).
# Solo instala OGRO 2: nunca borra ni reemplaza OGRO 1.0 (OGRO.vst3 / OGRO.component).
cd "$(dirname "$0")" || exit 1

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
VST3_NAME="OGRO 2.vst3"
AU_NAME="OGRO 2.component"

pausa() { read -n 1 -s -r -p "Tocá cualquier tecla para cerrar..."; echo; }

if [ ! -d "$VST3_NAME" ] || [ ! -d "$AU_NAME" ]; then
    echo "No encuentro \"$VST3_NAME\" o \"$AU_NAME\" al lado de este instalador."
    echo "Descomprimí el zip completo y volvé a abrir \"Instalar OGRO 2.command\"."
    pausa
    exit 1
fi

mkdir -p "$VST3_DIR" "$AU_DIR" "$HOME/Music/OGRO 2 Presets"

BUILD=""
[ -f "BUILD.txt" ] && BUILD="$(head -1 BUILD.txt)"
[ -n "$BUILD" ] && echo "Versión a instalar: $BUILD" && echo ""

# Ableton abierto: sigue usando la versión que cargó al abrirse hasta que se cierra del todo
LIVE_ABIERTO=0
pgrep -x "Live" >/dev/null 2>&1 && LIVE_ABIERTO=1

echo "Instalando OGRO 2..."
# Reemplaza solo una versión anterior de OGRO 2 (rutas exactas con el " 2")
rm -rf "$VST3_DIR/$VST3_NAME" "$AU_DIR/$AU_NAME"
cp -R "$VST3_NAME" "$VST3_DIR/"
cp -R "$AU_NAME" "$AU_DIR/"

# Quita la marca de "descargado de internet" para que macOS no lo bloquee
xattr -dr com.apple.quarantine "$VST3_DIR/$VST3_NAME" 2>/dev/null
xattr -dr com.apple.quarantine "$AU_DIR/$AU_NAME" 2>/dev/null
[ -d "OGRO 2.app" ] && xattr -dr com.apple.quarantine "OGRO 2.app" 2>/dev/null

killall -9 AudioComponentRegistrar 2>/dev/null

echo ""
echo "Listo. OGRO 2 quedó instalado en:"
echo "  $VST3_DIR/$VST3_NAME"
echo "  $AU_DIR/$AU_NAME"
if [ -d "$VST3_DIR/OGRO.vst3" ] || [ -d "$AU_DIR/OGRO.component" ]; then
    echo ""
    echo "OGRO 1.0 sigue instalado y no se tocó: tus sesiones viejas lo siguen usando."
fi
echo ""
if [ "$LIVE_ABIERTO" = "1" ]; then
    echo "=================================================================="
    echo "  ATENCIÓN: Ableton está abierto."
    echo "  Va a seguir usando la versión VIEJA de OGRO 2 hasta que lo cierres"
    echo "  del todo: guardá tu sesión, Cmd+Q en Ableton y volvé a abrirlo."
    echo "=================================================================="
    echo ""
fi
echo "Para comprobar la versión: abajo del logo de OGRO 2 dice la versión y el build"
[ -n "$BUILD" ] && echo "(tiene que decir: $BUILD)."
echo ""
echo "Abrí Ableton Live 12 > Settings > Plug-Ins y tocá 'Rescan'."
echo "Lo vas a encontrar en Plug-Ins > EVO THE SUN > OGRO 2 (es un instrumento: va en una pista MIDI)."
echo ""
pausa
