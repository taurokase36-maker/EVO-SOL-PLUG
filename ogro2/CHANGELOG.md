# OGRO 2 — Cambios

Formato: cada fase del desarrollo suma una entrada. La versión publicada será la 2.0.0.

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
