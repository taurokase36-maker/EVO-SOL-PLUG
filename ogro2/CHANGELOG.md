# OGRO 2 — Cambios

Formato: cada fase del desarrollo suma una entrada. La versión publicada será la 2.0.0.

## 2.0.0 (en desarrollo) — Fase 0: convivencia con OGRO 1.0

OGRO 2 arranca como copia exacta de OGRO 1.0, con identidad propia para que los dos
plugins se vean y carguen a la vez en Ableton. **El sonido no cambia**: el test A/B da
diferencia 0 muestra a muestra en los 12 presets.

### Identidad
- Plugin nuevo "OGRO 2": código `Ogr2`, bundle `com.evothesun.ogro2`, target de CMake
  `OGRO2`, fabricante EVO THE SUN (`Evts`) como la 1.0.
- Archivos `OGRO 2.vst3`, `OGRO 2.component` y `OGRO 2.app`, distintos de los de la 1.0.
- Código en el namespace `ogro2` y DSP propio en `Source/dsp` (namespace `evo2`, copia
  del de EVO SOL): un cambio futuro en EVO SOL u OGRO 1.0 no cambia el sonido de OGRO 2.
- Símbolos ocultos en el binario, para que dos plugins en el mismo proceso no se crucen.
- Presets propios en `Música > OGRO 2 Presets`.
- Un "2" rojo en el logo para distinguirlo de la 1.0.

### Compilación y tests
- Workflow propio `build-ogro2.yml` (solo corre con cambios en `ogro2/`). Antes de
  compilar corre los tests del motor, el A/B contra la 1.0 y la prueba del instalador.
- Verifica que el ID de clase VST3, el subtype AU (`Ogr2`) y el bundle sean distintos
  de los de OGRO 1.0.
- auval y pluginval ahora son bloqueantes: si fallan, la compilación queda en rojo
  (el zip igual se sube para poder revisarlo).
- `tests/ab_v1.cpp`: compila OGRO 1.0 y OGRO 2 en el mismo programa y compara las
  mezclas y la "huella" de cada pista (nivel por paso y energía por banda).
- `tests/fingerprints_v1.json`: huellas de referencia de la 1.0.
- `tests/test_instalador.sh`: instala en una carpeta de mentira y comprueba que
  OGRO 1.0 quede intacto.

### Instalador
- `Instalar OGRO 2.command` solo copia OGRO 2. Nunca borra ni reemplaza `OGRO.vst3`
  ni `OGRO.component`.

### Línea base (Mac Intel i7-1068NG7, 44.1 kHz, buffers de 512)
- Timing: error máximo de 4 muestras. Swing: 1 muestra.
- CPU: 12.5 a 15.1 % de un núcleo por preset.
- Estado serializado: 8449 bytes.
