#!/bin/bash
# Uso: ci_anotar.sh <notice|warning|error> <título> <archivo> [líneas]
# Publica las últimas líneas de un registro como anotación de GitHub Actions
# (las anotaciones de un repo público se pueden leer sin iniciar sesión).
nivel="$1"; titulo="$2"; archivo="$3"; lineas="${4:-40}"
[ -f "$archivo" ] || exit 0
msg=$(tail -n "$lineas" "$archivo" | sed -e 's/%/%25/g' -e 's/\r/%0D/g' | awk '{printf "%s%%0A", $0}')
echo "::${nivel} title=${titulo}::${msg}"
