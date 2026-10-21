# OGRO 2 — Cambios

Formato: cada fase del desarrollo suma una entrada. La versión publicada será la 2.0.0.

## 2.0.0 (en desarrollo) — Fase 5: carácter analógico y escenas

### Instalación
- **Versión y build a la vista**, abajo del logo (por ejemplo `v2.0.24 · build-24-abc1234`).
  La versión cambia en cada compilación, así Ableton y macOS no se quedan con la anterior
  en caché.
- El instalador avisa si Ableton está abierto: **un plugin se carga una sola vez por sesión
  de Ableton**, así que con Live abierto sigue sonando la versión vieja hasta cerrarlo del
  todo (Cmd+Q) y volver a abrirlo.
- Una instancia nueva arranca con el preset "Inicial" completo (antes tomaba los valores
  por defecto de OGRO 2, con MODO TONAL encendido, y el bajo de "Inicial" sonaba distinto).

### CARÁCTER: ORIGINAL / LIMPIO (panel ESCENAS)
- **ORIGINAL** (por defecto): el sonido de siempre.
- **LIMPIO**: saturaciones de las voces con anti-aliasing (ADAA), perillas suavizadas en
  8 ms (sin "zipper" al automatizar) y, con CALIDAD ALTA, osciladores con polyBLEP de 4
  puntos (~10 dB menos de aliasing). Medido: un bajo agudo con drive pasa de -27.6 dB a
  -31.9 dB de aliasing; un salto brusco de DRIVE deja un escalón 7 veces más chico. Los 12
  presets cambian de nivel menos de 0.11 dB.

### Tira analógica por pista (pestaña TIRA del inspector)
- DRIVE con 4 tipos (cinta, válvula, transistor, diodo; sobremuestreo 2x), CUERPO con
  frecuencia, RUIDO que sigue a la envolvente de la pista (en silencio no suena),
  TRANSIENTE (+/-), BRILLO (exciter + aire) y COMP VCA o FET con makeup automático.
- Todo en 0 = la pista suena exactamente igual. Automatizable, modulable (MOD y MOTION) y
  con p-locks.

### Ideas 5, 6 y 7
- **Escenas A/B con MORPH** (panel ESCENAS): GUARDAR A / B, IR A A / B y MORPH
  automatizable entre las dos (las perillas iguales en A y B siguen a la perilla).
- **Deriva lenta**: las perillas de sonido se mueven solas en ciclos de 16, 32 o 64
  compases. Sale de la semilla: cada render sale igual.
- **Dado de sonido con candados**: botón VARIAR en el inspector (clic derecho: más fuerte)
  y un candadito en cada perilla para que no se mueva.

### Otros
- **Suma de consola** opcional (apagada): saturación y diafonía muy sutiles al sumar.
- El ring mod se configura en **FX** (portadora) y en el inspector de cada pista
  (cantidad); el panel RING pasó a ser ESCENAS.
- CPU: los sobremuestreos del drum bus y del master suman en 4 cadenas paralelas (el clipper
  quedó 2 veces más rápido). En la comparación exacta con la 1.0 se usa la suma original y
  los 12 presets siguen idénticos muestra a muestra; en uso normal la diferencia es solo de
  redondeo (por debajo de -70 dB). Tira completa: ~0.5 % de un núcleo por pista.
- Test nuevo `test_analogico`.

## 2.0.0 (en desarrollo) — Fase 4: samples de Orbhitz

### Librería
- Lector de `.mse` en C++ (solo CHD, PAD, HIT y SFX: nada de BAS ni VOX). Los 125 archivos
  de tu Orbhitz se leen bien (44.3 MB de PCM).
- **Carpeta configurable** (menú ≡ > Carpeta de samples). Por defecto
  `Música > OGRO Samples > Orbhitz`; si todavía no existe y la librería está en Descargas,
  OGRO 2 la usa ahí mismo y el menú ofrece **copiarla** (solo CHD, PAD, HIT y SFX).
- **Pool compartido** entre todas las instancias: el audio queda en memoria en 16 bits y se
  carga en un hilo de fondo (el audio nunca espera ni pide memoria).
- **Análisis de afinación** (cromagrama con saliencia armónica + nota del bajo + plantillas
  de acordes): raíz con octava, calidad (m, M, m7, maj7, sus, 5ta...) y si tiene altura
  clara. No confía en la nota raíz del archivo. Se guarda en `ogro2_samples.json` en la
  carpeta de samples: **lo podés corregir a mano** (raiz, calidad, tonal) y OGRO 2 lo
  respeta. Los acordes siempre se transponen; los SFX solo si la altura es muy clara.
  Herramienta para revisarlo en una tabla: `tools/orbhitz/analizar.cpp`.

### Motor de samples
- Interpolación **Hermite** en LIVE y **sinc de 16 puntos** con antialias al subir el pitch
  en ALTA. Parámetro **CALIDAD** (MASTER): LIVE / ALTA / AUTO (ALTA cuando Ableton congela
  o exporta). Conversión de 44.1/48 kHz a la frecuencia de Ableton.
- **TEXTURAS** (MIDI A1): INICIO, LARGO, PITCH, CORTE, RESO, ENV, DRIVE, P.ENV, REVERSA,
  filtro LADDER o SVF, ATAQUE / SUSTAIN / RELEASE. **EN TONO**: el sample va a la
  tonalidad y la NOTA de cada paso mueve la altura. **DADO** con filtro de categoría y
  **azar por paso** (sale de la semilla).
- **ACORDES**: FUENTE Síntesis / Sample / Capas (pestaña SAMPLE). El acorde de Orbhitz va a
  la raíz de cada paso; con azar por paso en modo diatónico elige samples menores o
  mayores según el grado.
- El estado guarda el **nombre** del sample, no la ruta: si en otra Mac falta, la sesión
  abre igual y el inspector avisa.

### Medidas
- A/B con la 1.0: los 12 presets importados siguen idénticos. Test nuevo `test_samples`
  (lector, análisis, pool y manifiesto editado a mano, afinación exacta en LIVE y ALTA,
  antialias, reversa / inicio / largo, sample que falta, acordes con sample, azar por paso).
  Los tests usan archivos sintéticos: la librería no está en el repo.

## 2.0.0 (en desarrollo) — Fase 3: tonalidad, bajo, acordes y melodía

### Tonalidad
- **Tónica + escala** (menor, dórica, frigia, menor armónica, mayor, mixolidia,
  pentatónica menor) en el display TONALIDAD (clic = menú).
- **MODO TONAL** (encendido por defecto): la NOTA de cada paso de bajo, acordes, perc,
  melodía y texturas es un grado de la escala, así todo queda en tono. Al importar un
  preset de la 1.0 (y en los 12 presets de fábrica de la 1.0) queda apagado: suenan igual.

### Bajo
- **5 formas**: Clásica (la de la 1.0, con ONDA), Pulso, Supersierra, Triángulo y Seno FM.
- **SUB** (seno una octava abajo, limpio, con nivel propio).
- **Acento 303**: más volumen y filtro, el filtro cierra rápido y los acentos seguidos se
  acumulan; el slide tarda siempre lo mismo. "Clásico (1.0)" sigue disponible.
- **LEGATO**: las notas que se pisan se ligan sin volver a disparar.
- GATE por paso (fase 2) + ATAQUE / DECAY / SUSTAIN / RELEASE.

### Acordes
- **23 tipos** (se suman m, M, m6, add9, m add9, sus2, sus4, cuartas, m9 y m11 Detroit,
  6/9, 7, dim, m7b5, maj7#11) y **tipo por paso** (modo TIPO del secuenciador).
- **DIATÓNICO** (nuevo, por defecto): el acorde se arma con las notas de la escala, así el
  III de La menor sale Do maj7 sin pensar. **PARALELO**: chord memory como la 1.0.
- **INVERSIÓN**, **APERTURA** (voicings abiertos), **VOICE LEADING** automático (elige la
  inversión que menos se mueve) y **STRUM** (0 a 80 ms). Octava por paso (fase 2).

### MELODÍA (pista nueva, MIDI G#1)
- Cuatro modelos de síntesis limpia: Pluck (Karplus-Strong), Lead, Campana (FM) y Órgano.
  Poly (4 voces), Mono o Mono legato; GLIDE, VIBRATO, octava. Diseño en
  [design/MELODIA.md](design/MELODIA.md).
- TEXTURAS (MIDI A1) ya aparece en el secuenciador; los samples llegan en la fase 4.

### Compatibilidad
- Estado versión 2. Las sesiones guardadas con las fases 1 y 2 abren con MODO TONAL
  apagado, acordes en paralelo, acento clásico y polirritmos continuos: suenan igual.
- A/B con la 1.0: los 12 presets importados siguen idénticos muestra a muestra (con 10
  pistas). Test nuevo `test_tonal` (escalas, modo tonal, acordes, voice leading, strum,
  bajo, legato, afinación de la melodía, voces, generador en tono, CPU ~12-16 %).

## 2.0.0 (en desarrollo) — Fase 2: secuenciador, groove y arreglo

### Secuenciador
- **32 pasos** por pista (LARGO hasta 32; botones 1-16 / 17-32). El largo del patrón sigue a
  la pista más larga.
- Modos nuevos en la pantalla de pasos: **OCT** (octava por paso), **NUDGE** (corrimiento
  fino de hasta medio paso), **GATE** (largo de la nota), **COND** (condiciones), **P-LOCK**
  y **MOTION**. RATCH llega a 8 repeticiones.
- **Condiciones por paso** (estilo Elektron): FILL / !FILL, PRE / !PRE (si sonó el paso
  anterior con condición), VEC / !VEC (la pista de arriba), 1ª / !1ª y vueltas A:B (1:2,
  2:2, 1:3... 8:8). Se ven en la tecla del paso.
- **P-locks**: cualquier perilla de la pista (o algunas globales: filtro DJ, delay, reverb,
  ring, drum bus...) puede tener un valor propio en cada paso; vale desde ese golpe hasta el
  próximo de la pista. Se dibujan en el modo P-LOCK; clic derecho borra.
- **MOTION** ahora vive en el secuenciador (modo MOTION, carriles M1-M4); el panel aparte
  se fue y su lugar lo ocupan GROOVE y ARREGLO.
- **Deshacer / rehacer** propio del secuenciador (pasos, p-locks, cadena y MOTION; 64
  pasos de historial). Los parámetros los sigue deshaciendo Ableton.
- **Euclidiano** (botón EUCLID): pulsos, pasos y rotación con vista previa.
- **INVENTAR por género** (minimal house, micro-house / rominimal, deep, Detroit, garage),
  para una pista o para todas (▾), y **MUTAR** (cambia un porcentaje sin tocar los tiempos
  del bombo; clic derecho: todas las pistas).

### Groove (panel GROOVE)
- Plantillas: Recto, MPC 60 (acentos), Shuffle 909 (swing en saltos), Suelto (Villalobos)
  y Empuje (garage), con CANTIDAD.
- HUMANIZAR (0 a 20 ms; sale de la semilla, así que con AZAR FIJO se repite igual).
- **Swing por pista** (PROPIO + cantidad).
- Polirritmos "desde el patrón" (las pistas de largo distinto arrancan juntas cuando entra
  el patrón) o "continuos" como la 1.0 (lo usan los presets importados).

### Arreglo (panel ARREGLO)
- **Cadena de patrones** (song mode): fichas patrón x vueltas; sale de la posición del tema,
  así que exportar o arrancar en cualquier compás da siempre lo mismo.
- **Cambio de patrón**: al compás, al final del patrón o inmediato.
- **Fills** cada 2, 4, 8 o 16 compases: solo condiciones FILL, o tocar otro patrón entero.
- **FX de fin de compás**: stutter (1/16 o 1/32), barrido de filtro, tiro de reverb, tiro
  de delay con feedback que sube, tape stop, bitcrush y riser de ruido. Cada uno con su
  probabilidad y su destino (master, batería, bajo y acordes o una pista); cada 1, 2, 4 u 8
  compases, en los últimos 1 a 4 pasos, con CANTIDAD general y botón TIRAR FX. Todo
  automatizable.

### Medidas
- Los 12 presets importados de la 1.0 siguen dando idénticos muestra a muestra.
- Test nuevo `test_secuencia` (condiciones, fills, cadena, cambio de patrón, polirritmos,
  nudge, groove, humanizar, swing por pista, p-locks, octava, los 7 FX en 4 destinos,
  azar FIJO, generador). CPU con 8 pistas y los 7 FX en cada compás: ~12 % de un núcleo.

## 2.0.0 (en desarrollo) — Fase 1: base y bugs

Desde esta versión los IDs de los parámetros son definitivos: **las sesiones y presets que
guardes con OGRO 2 siguen abriendo en todas las 2.x.**

### Base para todas las 2.x
- IDs por nombre que no cambian nunca (`kick.tune`, `bass.cutoff`, `chord.type`,
  `global.swing`, `master.vol`...), con pista de versión de JUCE.
- Las listas que pueden crecer (tipos de acorde, formas y velocidades de LFO, tiempos de
  delay, formas del ring) nacen con lugares reservados: agregar opciones no corre la
  automatización.
- Estado guardado con número de versión (`stateVersion`): valores por ID, patrones en
  formato de texto con versión (32 pasos, p-locks por ID) y ruteo de MOD y MOTION por ID.
  Lo que una versión no entiende se ignora; lo que falta queda en su valor por defecto.
- La matriz MOD y los carriles MOTION guardan fuente y destino por ID (en la 1.0 eran un
  índice: agregar una perilla corría todos los destinos).
- Pasos de 64 bits (lugar para gate, nudge, condición, octava y tipo de acorde por paso) y
  hasta 8 p-locks por paso. Los pasos de la 1.0 se leen igual.
- 12 salidas separadas declaradas desde ahora (por ahora en silencio; se activan en la
  fase 7), así no cambia la configuración de salidas más adelante.
- Importar preset de OGRO 1.0 (menú ≡): traduce IDs, destinos de MOD, tipo de acorde, ruteo
  del ring, MOTION y patrones. Queda en azar VIVO y macros x1: suena igual que en la 1.0.
  También se leen sesiones guardadas con la fase 0 de OGRO 2.
- Presets propios en `.ogro2` (en Música > OGRO 2 Presets).

### Bugs corregidos (cada uno con su test)
1. **Probabilidad + ratchet**: ahora hay una sola decisión por cada vez que pasa el paso y
   vale para todos sus subgolpes (en la 1.0 los golpes 2 a 4 sonaban siempre).
2. **Azar reproducible**: parámetros SEMILLA y AZAR (FIJO / VIVO). Con FIJO, la
   probabilidad, el S&H de los LFO, el azar por paso, la VARIA, el ruido de cada golpe, la
   fase de los hats, la deriva analógica del bajo y los acordes, el chorus y la textura de
   vinilo dependen solo de la semilla y de la posición en el tema: cada render sale igual,
   arranques donde arranques. VIVO se comporta como la 1.0.
3. **Destinos de modulación por ID** (ver arriba).
4. **CPU**: las pistas que terminaron de sonar y cuya cola ya no suena no se calculan; el
   saturador sobremuestreado calcula su curva solo cuando cambia el drive (mismo sonido).
5. **Envolventes**: BAJO con ATAQUE, SUSTAIN y RELEASE; ACORDES con SUSTAIN y RELEASE y
   LIGADO (el acorde cambia de notas sin volver a disparar). Con SUSTAIN en 0 suenan como en
   la 1.0. La nota dura lo que el paso (o lo que la nota MIDI: el note-off cierra el gate).
6. **Cargar un preset** aplica los valores sin gestos (como cuando Ableton abre la sesión):
   no llena el historial de deshacer ni escribe automatización.
7. **Hilos**: el nombre del preset propio se lee y escribe protegido.
8. **Revisión general**:
   - Loop de Ableton: OGRO 2 lee los puntos de loop y parte el bloque en el salto; antes, una
     pista con largo distinto de 16 perdía el golpe del inicio en cada vuelta.
   - Tempo automatizado: con buffers grandes y rampas de tempo ya no se saltea ningún paso.
   - Probado con buffers de 1 a 2048 (fijos y variables), a 44.1, 48 y 96 kHz, arranques a
     mitad de compás y cambios de patrón.

### Otros cambios
- Macros con el doble de recorrido (parámetro PROFUNDIDAD DE MACROS: x2 por defecto, x1
  como la 1.0). MOVIMIENTO por encima del medio también mueve cosas solo (filtro de los
  acordes, envolvente del bajo, decay del hat y tono de la perc), aunque el preset no
  tenga LFOs.
- Ring mod con cantidad por pista (0 a 100 %) en vez de encendido/apagado.
- Interfaz: tarjeta ENVOLVENTE en bajo y acordes, tipo de acorde en pantalla de fósforo,
  AZAR y SEMILLA (clic = tirar el dado) en GROOVE, cantidad de ring por pista.

### Tests
- `test_host.cpp`: host simulado (loop, tempo, buffers, frecuencias, arranque, cambio de
  patrón, probabilidad + ratchet, azar reproducible, gate del bajo).
- `test_estado.cpp`: pasos, banco, formato de la 1.0, IDs, importador.
- `ab_v1.cpp --exacto`: los 12 presets importados de la 1.0 dan iguales muestra a muestra.
  `--normal`: OGRO 2 con azar FIJO conserva nivel y color de cada pista.
- `tools/snapshot/plugin_tests.cpp` (con JUCE): presets sin gestos, estado completo,
  importar `.ogro`, sesiones de la fase 0, salidas y procesado con 13 salidas.

### Renders repetibles (azar FIJO)
- Al arrancar el transporte o saltar a otro lugar del tema (no en los loops), OGRO 2 vuelve
  todo al mismo estado inicial: voces, colas de delay y reverb, compresores, saturadores,
  limitador, textura y ring. Dos exportaciones desde el mismo lugar salen idénticas bit a
  bit, aunque antes haya sonado otra cosa (test: `test_host.cpp`, "azar").

### Medidas (Mac Intel i7-1068NG7, 44.1 kHz, buffers de 512)
- CPU: 10.6 a 13.1 % de un núcleo por preset (antes 12.5 a 15.1 %).
- Timing: error de 1 muestra como mucho (registro exacto de disparos), en 44.1/48/96 kHz
  y buffers de 1 a 2048.

## 2.0.0 (en desarrollo) — Estética nueva

La interfaz pasa a la dirección visual de las referencias (ver `design/ESTETICA.md` y la
maqueta aprobada en `design/maqueta/`). El sonido sigue idéntico a OGRO 1.0.

- Aparato de plástico crema con grilla fina, caño de cobre y logo cromado sobre placa
  azul con destellos rojo y amarillo.
- Pantallas de fósforo verde para preset, tempo, tonalidad, largo y secuencia.
- La pista elegida se edita en una ventana de plástico violeta translúcido (inspector):
  sonido, mezcla, envíos (con traza de cobre) y ring mod.
- FX, MOD, MOTION, RING y MASTER se abren como paneles en esa ventana; la matriz MOD
  queda en segundo plano.
- Perillas cromadas con anillo de valor. Shift o Cmd al arrastrar = ajuste fino.
- Volumen de cada pista como perilla en la lista de pistas, con más recorrido.
- Macros más grandes, con anillo desde el centro (al medio no cambian nada),
  explicación de lo que mueven y pantalla con cuánto actúan.
- Teclas de pasos tipo calculadora con LED, luz del color de la pista y nota.
- Orbe violeta de play y disco negro con anillo de neón para el nivel del master.
- Tipografías incrustadas (licencia OFL): Michroma, Space Grotesk y VT323.
- `tools/snapshot/`: compila la interfaz con JUCE en una Mac sin CMake y guarda una
  captura PNG, para revisar el diseño sin abrir Ableton.

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
