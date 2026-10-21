# Pista MELODÍA (fase 3) — diseño

Pista 9 del secuenciador, **MIDI G#1 (44)**. Las 8 pistas de la 1.0 siguen en C1-G1 y
TEXTURAS va en A1 (45).

## Por qué así

La prioridad 1 es variedad para melodías con síntesis limpia (nada de aliasing ni
"plástico"). En vez de un sinte con mil perillas, cuatro **modelos** con carácter distinto
que comparten las mismas 8 perillas, así lo que aprendés en uno sirve en los otros:

| Modelo | Cómo suena | Síntesis | TIMBRE | ENV |
|---|---|---|---|---|
| **Pluck** | cuerda pulsada, kalimba, pizzicato: el clásico de la minimal | Karplus-Strong afinado (retardo fraccional compensado) | apagado de la cuerda | brillo del filtro |
| **Lead** | lead suave / sintético | 2 sierras band-limited desafinadas + sub seno | desafinación | barrido del filtro |
| **Campana** | campanas, vidrio, bells de Detroit | FM de 2 operadores + parcial a 2.76x | relación (1 a 7) | índice de FM (brillo) |
| **Órgano** | órgano de drawbars, tonos limpios | 6 armónicos (1, 2, 3, 4, 6, 8) | drawbars (de seno puro a brillante) | click de tecla |

Perillas: MODELO · DECAY · CORTE · RESO · ENV · TIMBRE · VIBRATO (entra de a poco) · GLIDE.
Más ATAQUE / SUSTAIN / RELEASE (con sustain 0 cae sola como un pluck) y, en la fila de
abajo, **VOCES** (Poly 4 / Mono / Mono legato) y **OCTAVA**.

- Poly: hasta 4 notas, cada voz un poco abierta en estéreo. Un paso LIGADO hace glide desde
  la última nota en vez de abrir otra voz.
- Mono legato: las notas que se pisan (GATE largo o nota MIDI apretada) se ligan con GLIDE.
- Con MODO TONAL la NOTA de cada paso es un grado de la escala (no se puede desafinar).
- INVENTAR arma motivos cortos que se repiten con una variación en la segunda mitad.

Si querés otro reparto (por ejemplo un quinto modelo "pad"), la lista de modelos tiene 16
lugares reservados: se agrega sin romper sesiones ni automatización.
