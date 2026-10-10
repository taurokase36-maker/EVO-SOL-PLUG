# EVO SOL — Vocal Suite · EVO THE SUN

Plugin vocal todo-en-uno para Ableton Live 12 (VST3 y Audio Unit, Mac Intel y Apple Silicon).

| Módulo | Qué hace |
|---|---|
| **Afinación** | Corrección en key y escala (10 escalas), velocidad de afinación de 0 ms (efecto robot) a 400 ms (natural), Humanizar, Flex-Tune, vibrato natural, Garganta (formantes), transponer ±12, referencia de afinación, tipo de voz y gráfico de pitch en vivo. |
| **Inteligencia** | Asistente que escucha la voz y detecta key, registro, balance tonal, sibilancia y nivel, y propone un ajuste completo. Auto-ganancia (rider), suavizador de resonancias espectral y de-esser que aprende el brillo de tu voz. |
| **Núcleo analógico** | Low-cut, Cuerpo, Presencia, Aire, compresor FET + compresor óptico (macro de Compresión), Color, saturación válvula + cinta sobremuestreada 4x y mezcla en paralelo. |
| **Ear candy** | Dos voces de armonía diatónica, doble estéreo, delay ping-pong sincronizado con ducking, reverb FDN, filtro radio y ancho estéreo. |
| **Salida** | Clipper suave sobremuestreado + limitador con lookahead (techo ajustable). |

## Cómo conseguir el plugin listo para instalar (sin instalar nada en tu Mac)

1. Creá una cuenta gratis en [github.com](https://github.com) y un repositorio nuevo (puede ser privado), por ejemplo `evo-sol`.
2. Subí **todo el contenido** de esta carpeta al repositorio (botón *Add file > Upload files*, arrastrando las carpetas).
   - La carpeta `.github` empieza con punto y el Finder la oculta: en la ventana del Finder tocá **Cmd + Shift + .** para verla y arrastrala también.
3. Entrá a la pestaña **Actions** del repositorio. La compilación arranca sola (tarda ~10-15 minutos).
4. Cuando termine con ✓ verde, abrí la ejecución y descargá **EVO-SOL-mac** en *Artifacts*.
5. Descomprimí y seguí `LEEME.txt` (doble clic derecho > Abrir en `Instalar EVO SOL.command`).

## Compilar en tu propia Mac (opcional)

Requiere Xcode Command Line Tools y CMake:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="x86_64"
cmake --build build --config Release --parallel
```

Los plugins quedan en `build/EVOSOL_artefacts/Release/`.

## Estructura

- `Source/dsp/` — todo el procesamiento de audio (C++ puro, sin dependencias)
  - `PitchDetector.h` detector YIN · `PitchEngine.h` corrección + PSOLA + armonías
  - `Dynamics.h` FET, óptico, de-esser adaptativo, rider, limitador
  - `Saturation.h` válvula/cinta y clipper con sobremuestreo 4x
  - `Spectral.h` suavizador de resonancias (STFT) · `Assistant.h` asistente
  - `Effects.h` doble, delay, reverb, radio · `VocalEngine.h` la cadena completa
- `Source/Plugin*.cpp` — plugin JUCE e interfaz
- `tests/` — tests offline del DSP (afinación medida al cent, latencia, CPU)

## Notas

- Latencia: ~63 ms (Ableton la compensa sola al mezclar).
- La "inteligencia" es análisis adaptativo en tiempo real (detección de key por perfiles tonales, aprendizaje del brillo de la voz, detección espectral de resonancias, rider de nivel). No usa modelos de redes neuronales entrenados.
- Usa el framework JUCE 9 (licencia JUCE o AGPLv3). Si algún día lo vendés, revisá las condiciones en juce.com/legal.
