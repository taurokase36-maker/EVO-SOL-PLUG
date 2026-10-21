# OGRO 2 — Groovebox analógico · EVO THE SUN

Instrumento para Ableton Live 12 (VST3 / AU, Mac Intel y Apple Silicon). Es un plugin
aparte de OGRO 1.0 (carpeta `../ogro`): los dos conviven en el mismo Ableton y las
sesiones viejas siguen usando la 1.0.

> **Estado:** en desarrollo (fase 2 de 7). Ver [CHANGELOG.md](CHANGELOG.md).

## Compatibilidad hacia adelante (2.x)

- Cada parámetro tiene un ID de texto que no cambia nunca (`bass.cutoff`). Rango y mapa
  físico tampoco cambian. Los parámetros nuevos se agregan con una versión mayor.
- Las listas que pueden crecer tienen lugares reservados ("—").
- El estado (`<OGRO2 stateVersion="1">`) guarda los valores por ID, los patrones en texto
  con versión y el ruteo de MOD/MOTION por ID. Si cambia el formato, se sube
  `stateVersion` y se agrega la migración en `restoreFromXml`.
- Los índices internos (`P::...`) pueden cambiar entre versiones: no se guardan.

## Convivencia con OGRO 1.0

| | OGRO 1.0 | OGRO 2 |
|---|---|---|
| Carpeta | `ogro/` (no se toca) | `ogro2/` |
| Nombre | OGRO | OGRO 2 |
| Código de plugin / AU subtype | `Ogro` | `Ogr2` |
| Bundle | `com.evothesun.ogro` | `com.evothesun.ogro2` |
| Archivos | `OGRO.vst3`, `OGRO.component` | `OGRO 2.vst3`, `OGRO 2.component` |
| Presets | `Música > OGRO Presets` | `Música > OGRO 2 Presets` |
| Workflow | `build-ogro.yml` | `build-ogro2.yml` (solo `ogro2/**`) |

Fabricante: EVO THE SUN (`Evts`) en los dos, así aparecen juntos en Ableton.

## Código

- `Source/engine/` — motor de audio en C++ puro (voces, secuenciador, modulación, presets,
  efectos de fin de compás `BarFx.h`, generador `Generator.h`), namespace `ogro2`.
- `Source/dsp/` — DSP propio (copia del de EVO SOL, namespace `evo2`). OGRO 2 no depende de
  `../Source/dsp`, así un cambio en EVO SOL nunca cambia su sonido.
- `Source/Plugin*.cpp`, `Widgets.h`, `Ogro2LookAndFeel.h` — plugin JUCE e interfaz.
- `design/` — dirección visual ([ESTETICA.md](design/ESTETICA.md)).
- `installer/` — instalador y LEEME.

## Tests (C++ puro, sin JUCE)

Desde `ogro2/tests`:

```bash
mkdir -p bin out && clang++ -std=c++17 -O2 test_ogro.cpp -o bin/test_ogro && ./bin/test_ogro
```

Timing al sample, swing, CPU por preset y estado. Termina con `TODO OK` (~30 s).

```bash
clang++ -std=c++17 -O2 ab_v1.cpp -o bin/ab_v1 && ./bin/ab_v1 --exacto && ./bin/ab_v1 --normal
```

Compila OGRO 1.0 y OGRO 2 en el mismo programa. `--exacto`: los 12 presets importados de
la 1.0 tienen que dar iguales muestra a muestra. `--normal`: OGRO 2 como se usa (azar FIJO)
conserva el nivel y el color de cada pista. Terminan con `A/B OK`.

```bash
clang++ -std=c++17 -O2 test_host.cpp -o bin/test_host && ./bin/test_host
clang++ -std=c++17 -O2 test_estado.cpp -o bin/test_estado && ./bin/test_estado
```

Host simulado (loop de Ableton, tempo automatizado, buffers, frecuencias, azar, gate) y
estado guardado (patrones, IDs, importador de la 1.0).

```bash
clang++ -std=c++17 -O2 test_secuencia.cpp -o bin/test_secuencia && ./bin/test_secuencia
```

Secuenciador de la fase 2: condiciones, fills, cadena, cambio de patrón, polirritmos,
nudge, groove, humanizar, swing por pista, p-locks, FX de fin de compás y generador.

```bash
bash ../tools/snapshot/build.sh tests
```

Tests del plugin con JUCE (necesita JUCE en `build-ogro2/juce-src`): cargar presets sin
gestos, estado completo, importar `.ogro`, salidas.

```bash
bash test_instalador.sh
```

Instala en una carpeta de mentira y comprueba que OGRO 1.0 quede intacto.

`render_demos.cpp` renderiza 16 compases por preset; `solo.cpp`, cada pista sola. Los WAV
de referencia de la 1.0 (`tests/reference/v1/`) quedan solo en la Mac: no van al repo.

## Compilar

GitHub Actions lo compila solo con cada push que toque `ogro2/` (*Compilar OGRO 2 (Mac)*):
corre los tests, compila VST3 + AU + Standalone universal, valida con auval y pluginval y
deja `OGRO-2-mac` en *Artifacts*.

En una Mac propia (opcional, requiere CMake):

```bash
cmake -S ogro2 -B build-ogro2 -DCMAKE_BUILD_TYPE=Release
cmake --build build-ogro2 --config Release --target OGRO2_VST3 OGRO2_AU
```
