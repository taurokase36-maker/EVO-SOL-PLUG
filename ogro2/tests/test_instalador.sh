#!/bin/bash
# Prueba el instalador de OGRO 2 en un HOME de mentira con OGRO 1.0 "instalado":
# OGRO 2 tiene que quedar instalado y OGRO 1.0 intacto (mismo contenido).
set -e
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

export HOME="$TMP/home"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR/OGRO.vst3/Contents" "$AU_DIR/OGRO.component/Contents"
echo "ogro 1.0 vst3" > "$VST3_DIR/OGRO.vst3/Contents/marca"
echo "ogro 1.0 au" > "$AU_DIR/OGRO.component/Contents/marca"

# Paquete de mentira (como el zip de Actions) con una versión vieja de OGRO 2 ya instalada
PKG="$TMP/OGRO-2-mac"
mkdir -p "$PKG/OGRO 2.vst3/Contents" "$PKG/OGRO 2.component/Contents"
echo nuevo > "$PKG/OGRO 2.vst3/Contents/marca"
echo nuevo > "$PKG/OGRO 2.component/Contents/marca"
mkdir -p "$VST3_DIR/OGRO 2.vst3/Contents"
echo viejo > "$VST3_DIR/OGRO 2.vst3/Contents/viejo"
cp "$REPO/ogro2/installer/Instalar OGRO 2.command" "$PKG/"

bash "$PKG/Instalar OGRO 2.command" < /dev/null

fail() { echo "FALLA: $1"; exit 1; }
[ "$(cat "$VST3_DIR/OGRO.vst3/Contents/marca")" = "ogro 1.0 vst3" ] || fail "tocó OGRO.vst3"
[ "$(cat "$AU_DIR/OGRO.component/Contents/marca")" = "ogro 1.0 au" ] || fail "tocó OGRO.component"
[ "$(cat "$VST3_DIR/OGRO 2.vst3/Contents/marca")" = "nuevo" ] || fail "no instaló OGRO 2.vst3"
[ "$(cat "$AU_DIR/OGRO 2.component/Contents/marca")" = "nuevo" ] || fail "no instaló OGRO 2.component"
[ ! -e "$VST3_DIR/OGRO 2.vst3/Contents/viejo" ] || fail "no reemplazó la versión anterior de OGRO 2"
[ -d "$HOME/Music/OGRO 2 Presets" ] || fail "no creó Música > OGRO 2 Presets"
[ "$(ls "$VST3_DIR" | wc -l | tr -d ' ')" = "2" ] || fail "dejó archivos de más en VST3"
echo "INSTALADOR OK: OGRO 2 instalado, OGRO 1.0 intacto"
