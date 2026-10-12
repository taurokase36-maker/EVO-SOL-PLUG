# OGRO — Groovebox analógico · EVO THE SUN

Instrumento para Ableton Live 12 (VST3 / AU, Mac Intel y Apple Silicon) con 8 pistas sintetizadas, secuenciador, modulaciones, carriles dibujables, ring mod y la EQ + compresión de EVO SOL en el master.

## Qué trae

| Sección | Detalle |
|---|---|
| **8 pistas sintetizadas** | KICK (seno con pitch, click, drive), CLAP (ráfagas + room), RIM/snare, HAT C y HAT A (6 osciladores inarmónicos + ruido, con choke), PERC (FM de 2 operadores), BAJO (sierra/cuadrada/sub + filtro escalera de 4 polos, acento y glide), ACORDES (polifónico con 8 tipos de acorde, filtro y chorus). Cada una tiene 8 perillas y variación humana por golpe. |
| **Secuenciador** | 16 pasos × 8 patrones (A-H), velocidad, nota, probabilidad, ratchet (1-4), ligado, largo por pista (polirritmos), swing, cambio de patrón al compás, INVENTAR, copiar/pegar. Sigue el transporte de Ableton; PLAY interno para probar. |
| **Modulación** | 4 LFOs sincronizados (7 formas), matriz de 12 conexiones (LFOs, azar por paso, azar suave, envolvente del kick y macros como fuente) y más de 100 destinos. |
| **MOTION** | 4 carriles de automatización que se dibujan con el mouse, de 1 a 8 compases, con formas rápidas (rampa, seno, triángulo, azar). |
| **Macros** | CALOR, MOVIMIENTO, ESPACIO y CRUDEZA. |
| **Ring mod** | Sección propia: frecuencia, forma, mezcla, estéreo y ruteo por pista. |
| **Buses** | Drum bus con compresión fuerte y saturación cruda, pump (sidechain fantasma del kick) por pista, delay ping-pong y reverb. |
| **Master** | Filtro DJ (LP/HP), EQ (low-cut, cuerpo, presencia, aire) y dinámica FET + óptico + saturación estilo EVO SOL, textura de vinilo, clipper y limitador. |
| **Presets** | 12 moods: Inicial, Amanecer, Medianoche Mínima, Sudor, Terciopelo, Hipnosis, Nostalgia 92, Lluvia, Oscuro, Euforia, Sol de Verano, Underground. Guardar / abrir presets propios (.ogro). |

## Código

- `Source/engine/` — motor de audio en C++ puro (voces, secuenciador, modulación, presets)
- `Source/Plugin*.cpp`, `Widgets.h`, `OgroLookAndFeel.h` — plugin JUCE e interfaz
- `tests/` — tests offline (timing al sample, swing, estado, CPU) y render de demos

Comparte la EQ, compresión y saturación de `../Source/dsp` con EVO SOL.
