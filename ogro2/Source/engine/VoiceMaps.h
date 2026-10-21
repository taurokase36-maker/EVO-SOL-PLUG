// OGRO 2 — definición de las 10 pistas: nombres, controles y cómo se traduce
// cada perilla (0..1) a valores físicos. Lo usan el motor y la interfaz.
#pragma once

#include "../dsp/Common.h"
#include <cstdio>
#include <string>
#include <vector>

namespace ogro2
{
using namespace evo2;

constexpr int kTracks = 10;
constexpr int kSoundParams = 8;

// Las 8 de la 1.0 (MIDI C1-G1) y las nuevas a continuación (MELODÍA G#1, TEXTURAS A1)
enum Track : int { Kick = 0, Clap, Rim, HatC, HatO, Perc, Bass, Chord, Melody, Texture };

inline const char* trackName (int t)
{
    static const char* names[] = { "KICK", "CLAP", "RIM", "HAT C", "HAT A", "PERC", "BAJO", "ACORDES", "MELODIA", "TEXTURAS" };
    return names[clampv (t, 0, kTracks - 1)];
}

inline bool isDrum (int t) { return t <= Perc; }
// Pistas con salida estéreo propia (las demás son mono)
inline bool isStereoTrack (int t) { return t == Chord || t == Melody || t == Texture; }
// Pistas cuya NOTA es una altura (con MODO TONAL, un grado de la escala)
inline bool isPitched (int t) { return t == Perc || t == Bass || t == Chord || t == Melody || t == Texture; }

// Parámetros de disparo de un paso
struct Hit
{
    float vel = 1.0f;       // 0..1
    float note = 0.0f;      // semitonos (desde la nota base de la pista)
    bool slide = false;     // ligado / glide (bajo y acordes)
    bool accent = false;
    double gateSamples = 0; // largo de la nota (0 = sin gate: la envolvente cae sola)
    bool fixedSeed = false; // azar FIJO: el ruido y la VARIA salen de "seed"
    uint32_t seed = 0;
    int octave = 0;         // octava del paso (-3..+3)
    int chordType = -1;     // tipo de acorde del paso (-1 = el de la pista)
};

// Hash de 32 bits (azar reproducible: mismo paso + misma semilla = mismo resultado)
inline uint32_t hash32 (uint32_t a, uint32_t b = 0, uint32_t c = 0, uint32_t d = 0)
{
    uint64_t h = 0x9E3779B97F4A7C15ull ^ ((uint64_t) a << 32 | b);
    h ^= ((uint64_t) c << 32 | d) * 0xBF58476D1CE4E5B9ull;
    h ^= h >> 30; h *= 0xBF58476D1CE4E5B9ull;
    h ^= h >> 27; h *= 0x94D049BB133111EBull;
    h ^= h >> 31;
    const uint32_t r = (uint32_t) (h ^ (h >> 32));
    return r ? r : 1u;
}
inline float hashBipolar (uint32_t h) { return (float) (int32_t) h * (1.0f / 2147483648.0f); }
inline float hashUnit (uint32_t h) { return (float) (h >> 8) * (1.0f / 16777216.0f); }

// Mapas exponenciales / lineales
inline float expMap (float n, float lo, float hi) { return lo * std::pow (hi / lo, clampv (n, 0.0f, 1.0f)); }
inline float linMap (float n, float lo, float hi) { return lo + (hi - lo) * clampv (n, 0.0f, 1.0f); }
inline int stepMap (float n, int count) { return clampv ((int) std::floor (clampv (n, 0.0f, 1.0f) * count), 0, count - 1); }
inline float stepCentre (int idx, int count) { return ((float) idx + 0.5f) / (float) count; }

// Tipos de acorde (la lista solo crece al final: el parámetro tiene 32 lugares)
constexpr int kChordTypes = 23;
constexpr int kMaxChordNotes = 6;
inline const std::vector<std::string>& chordTypeNames()
{
    static const std::vector<std::string> n { "m7", "m9", "m11", "maj7", "maj9", "9", "sus7", "5ta+8va",
                                              "m", "M", "m6", "add9", "m add9", "sus2", "sus4", "Cuartas",
                                              "m9 Detroit", "m11 Detroit", "6/9", "7", "dim", "m7b5", "maj7#11" };
    return n;
}
inline const char* chordTypeName (int i)
{
    const auto& n = chordTypeNames();
    return n[(size_t) clampv (i, 0, (int) n.size() - 1)].c_str();
}

// Intervalos en semitonos (modo PARALELO: el acorde se transpone entero)
inline const float* chordIntervals (int i, int& count)
{
    static const float tab[kChordTypes][kMaxChordNotes] = {
        { 0, 3, 7, 10 },        // m7
        { 0, 3, 7, 10, 14 },    // m9
        { 0, 3, 10, 14, 17 },   // m11 (voicing abierto)
        { 0, 4, 7, 11 },        // maj7
        { 0, 4, 7, 11, 14 },    // maj9
        { 0, 4, 10, 14 },       // 9 (dominante, sin 5ta)
        { 0, 5, 7, 10 },        // sus4 con 7ma
        { 0, 7, 12, 19 },       // quintas apiladas
        { 0, 3, 7 },            // m
        { 0, 4, 7 },            // M
        { 0, 3, 7, 9 },         // m6
        { 0, 4, 7, 14 },        // add9
        { 0, 3, 7, 14 },        // m add9
        { 0, 2, 7 },            // sus2
        { 0, 5, 7 },            // sus4
        { 0, 5, 10, 15 },       // cuartas apiladas
        { 0, 7, 10, 14, 15 },   // m9 Detroit (5ta, 7ma y 9na abajo, 3ra arriba)
        { 0, 7, 10, 14, 17 },   // m11 Detroit
        { 0, 4, 9, 14 },        // 6/9
        { 0, 4, 7, 10 },        // 7
        { 0, 3, 6 },            // dim
        { 0, 3, 6, 10 },        // m7b5
        { 0, 4, 7, 11, 18 },    // maj7#11
    };
    static const int counts[kChordTypes] = { 4, 5, 5, 4, 5, 4, 4, 4, 3, 3, 4, 4, 4, 3, 3, 4, 5, 5, 4, 4, 3, 4, 5 };
    const int k = clampv (i, 0, kChordTypes - 1);
    count = counts[k];
    return tab[k];
}

// Grados de la escala desde la raíz del acorde (modo DIATÓNICO: todo queda en tono y la
// calidad del acorde la decide la escala; el tipo elige la forma)
inline const int* chordDegrees (int i, int& count)
{
    static const int tab[kChordTypes][kMaxChordNotes] = {
        { 0, 2, 4, 6 }, { 0, 2, 4, 6, 8 }, { 0, 2, 6, 8, 10 }, { 0, 2, 4, 6 }, { 0, 2, 4, 6, 8 }, { 0, 2, 6, 8 },
        { 0, 3, 4, 6 }, { 0, 4, 7, 11 }, { 0, 2, 4 }, { 0, 2, 4 }, { 0, 2, 4, 5 }, { 0, 2, 4, 8 }, { 0, 2, 4, 8 },
        { 0, 1, 4 }, { 0, 3, 4 }, { 0, 3, 6, 9 }, { 0, 4, 6, 8, 9 }, { 0, 4, 6, 8, 10 }, { 0, 2, 5, 8 }, { 0, 2, 4, 6 },
        { 0, 2, 4 }, { 0, 2, 4, 6 }, { 0, 2, 4, 6, 10 },
    };
    static const int counts[kChordTypes] = { 4, 5, 5, 4, 5, 4, 4, 4, 3, 3, 4, 4, 4, 3, 3, 4, 5, 5, 4, 4, 3, 4, 5 };
    const int k = clampv (i, 0, kChordTypes - 1);
    count = counts[k];
    return tab[k];
}

// Escalas (la lista solo crece al final)
inline const std::vector<std::string>& scaleNames()
{
    static const std::vector<std::string> n { "Menor", "Dorica", "Frigia", "Menor armonica", "Mayor", "Mixolidia", "Pentatonica menor" };
    return n;
}
inline const int* scaleSteps (int i, int& count)
{
    static const int tab[7][7] = {
        { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 1, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 8, 11 },
        { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 4, 5, 7, 9, 10 }, { 0, 3, 5, 7, 10 },
    };
    static const int counts[7] = { 7, 7, 7, 7, 7, 7, 5 };
    const int k = clampv (i, 0, 6);
    count = counts[k];
    return tab[k];
}
// Grado de la escala (puede ser negativo o pasar la octava) -> semitonos desde la tónica
inline int degreeToSemis (int degree, int scale)
{
    int n = 0;
    const int* st = scaleSteps (scale, n);
    const int oct = (int) std::floor ((double) degree / n);
    return 12 * oct + st[degree - oct * n];
}
// Grado más cercano a una altura en semitonos desde la tónica (empate: el de abajo)
inline int semisToDegree (int semis, int scale)
{
    int n = 0;
    scaleSteps (scale, n);
    const int approx = (int) std::floor ((double) semis * n / 12.0);
    int best = approx, bestD = 1 << 20;
    for (int d = approx - 2; d <= approx + 2; ++d)
    {
        const int dist = std::abs (degreeToSemis (d, scale) - semis);
        if (dist < bestD) { bestD = dist; best = d; }
    }
    return best;
}

inline const float* percRatios()
{
    static const float r[8] = { 0.5f, 1.0f, 1.41f, 2.0f, 2.76f, 3.5f, 4.2f, 5.4f };
    return r;
}

struct ParamInfo
{
    const char* name;
    float def;
};

// Nombre y valor por defecto (normalizado) de cada perilla de sonido
inline ParamInfo soundParamInfo (int t, int p)
{
    static const ParamInfo tab[kTracks][kSoundParams] = {
        // KICK
        { { "TONO", 0.38f }, { "DECAY", 0.45f }, { "PUNCH", 0.5f }, { "CURVA", 0.35f },
          { "CLICK", 0.35f }, { "DRIVE", 0.35f }, { "COLOR", 0.6f }, { "VARIA", 0.2f } },
        // CLAP
        { { "TONO", 0.45f }, { "DECAY", 0.35f }, { "SPREAD", 0.4f }, { "GOLPES", 0.5f },
          { "GRANO", 0.4f }, { "DRIVE", 0.3f }, { "ROOM", 0.3f }, { "VARIA", 0.25f } },
        // RIM
        { { "TONO", 0.5f }, { "DECAY", 0.2f }, { "RUIDO", 0.15f }, { "CUERPO", 0.5f },
          { "CLICK", 0.6f }, { "DRIVE", 0.4f }, { "BRILLO", 0.5f }, { "VARIA", 0.2f } },
        // HAT C
        { { "TONO", 0.5f }, { "DECAY", 0.25f }, { "METAL", 0.6f }, { "FILTRO", 0.5f },
          { "RESO", 0.3f }, { "DRIVE", 0.3f }, { "AIRE", 0.4f }, { "VARIA", 0.3f } },
        // HAT A
        { { "TONO", 0.5f }, { "DECAY", 0.35f }, { "METAL", 0.6f }, { "FILTRO", 0.45f },
          { "RESO", 0.3f }, { "DRIVE", 0.3f }, { "AIRE", 0.4f }, { "VARIA", 0.25f } },
        // PERC
        { { "TONO", 0.35f }, { "DECAY", 0.3f }, { "FM", 0.25f }, { "RATIO", 0.3f },
          { "PITCH", 0.3f }, { "DRIVE", 0.25f }, { "RUIDO", 0.15f }, { "VARIA", 0.25f } },
        // BAJO
        { { "OCTAVA", 0.375f }, { "DECAY", 0.35f }, { "CORTE", 0.3f }, { "RESO", 0.35f },
          { "ENV", 0.45f }, { "DRIVE", 0.35f }, { "ONDA", 0.2f }, { "GLIDE", 0.25f } },
        // ACORDES
        { { "TIPO", 0.1875f }, { "DECAY", 0.35f }, { "CORTE", 0.45f }, { "RESO", 0.2f },
          { "ENV", 0.4f }, { "DETUNE", 0.35f }, { "CHORUS", 0.5f }, { "ATAQUE", 0.05f } },
        // MELODIA
        { { "MODELO", 0.0f }, { "DECAY", 0.4f }, { "CORTE", 0.55f }, { "RESO", 0.2f },
          { "ENV", 0.4f }, { "TIMBRE", 0.4f }, { "VIBRATO", 0.15f }, { "GLIDE", 0.2f } },
        // TEXTURAS (samples)
        { { "INICIO", 0.0f }, { "LARGO", 1.0f }, { "PITCH", 0.5f }, { "CORTE", 1.0f },
          { "RESO", 0.1f }, { "ENV", 0.0f }, { "DRIVE", 0.0f }, { "P.ENV", 0.5f } },
    };
    return tab[clampv (t, 0, kTracks - 1)][clampv (p, 0, kSoundParams - 1)];
}

inline std::string fmt (const char* f, double v)
{
    char b[48];
    std::snprintf (b, sizeof (b), f, v);
    return b;
}
inline std::string fmtHz (float hz) { return hz >= 1000.0f ? fmt ("%.1f kHz", hz / 1000.0) : fmt ("%.0f Hz", hz); }
inline std::string fmtMs (float ms) { return ms >= 1000.0f ? fmt ("%.2f s", ms / 1000.0) : fmt ("%.0f ms", ms); }
inline std::string fmtPct (float n) { return fmt ("%.0f %%", n * 100.0); }

// Rangos físicos compartidos por motor e interfaz
namespace maps
{
    // Kick
    inline float kickTune (float n) { return expMap (n, 32.0f, 95.0f); }
    inline float kickDecay (float n) { return expMap (n, 60.0f, 1600.0f); }
    inline float kickPunch (float n) { return linMap (n, 0.0f, 48.0f); }
    inline float kickCurve (float n) { return expMap (n, 4.0f, 120.0f); }
    inline float colorHz (float n) { return expMap (n, 300.0f, 18000.0f); }
    // Clap
    inline float clapTone (float n) { return expMap (n, 600.0f, 3200.0f); }
    inline float clapDecay (float n) { return expMap (n, 40.0f, 900.0f); }
    inline float clapSpread (float n) { return expMap (n, 3.0f, 22.0f); }
    inline int clapBursts (float n) { return 2 + stepMap (n, 5); }
    // Rim
    inline float rimTone (float n) { return expMap (n, 180.0f, 950.0f); }
    inline float rimDecay (float n) { return expMap (n, 8.0f, 450.0f); }
    inline float rimBright (float n) { return expMap (n, 900.0f, 11000.0f); }
    // Hats
    inline float hatTone (float n) { return expMap (n, 0.6f, 1.8f); }
    inline float hatCDecay (float n) { return expMap (n, 8.0f, 260.0f); }
    inline float hatODecay (float n) { return expMap (n, 60.0f, 1600.0f); }
    inline float hatFilter (float n) { return expMap (n, 2500.0f, 12500.0f); }
    inline float hatReso (float n) { return expMap (n, 0.5f, 6.0f); }
    inline float airDb (float n) { return linMap (n, -6.0f, 12.0f); }
    // Perc
    inline float percTone (float n) { return expMap (n, 55.0f, 1400.0f); }
    inline float percDecay (float n) { return expMap (n, 15.0f, 900.0f); }
    inline float percFm (float n) { return linMap (n, 0.0f, 8.0f); }
    inline float percRatio (float n) { return percRatios()[stepMap (n, 8)]; }
    inline float percPitch (float n) { return linMap (n, 0.0f, 24.0f); }
    // Bajo
    inline int bassOctave (float n) { return stepMap (n, 4) - 1; } // -1..+2
    inline float synthDecay (float n) { return expMap (n, 30.0f, 3000.0f); }
    inline float bassCut (float n) { return expMap (n, 40.0f, 9000.0f); }
    inline float bassReso (float n) { return linMap (n, 0.0f, 0.97f); }
    inline float envOct (float n) { return linMap (n, 0.0f, 6.0f); }
    inline float glideMs (float n) { return expMap (n, 5.0f, 400.0f); }
    // Acordes
    inline int chordType (float n) { return stepMap (n, 8); }
    inline float chordCut (float n) { return expMap (n, 150.0f, 14000.0f); }
    inline float chordReso (float n) { return linMap (n, 0.0f, 0.9f); }
    inline float chordEnvOct (float n) { return linMap (n, 0.0f, 5.0f); }
    inline float detuneCents (float n) { return linMap (n, 0.0f, 32.0f); }
    inline float attackMs (float n) { return expMap (n, 1.0f, 600.0f); }
    // Melodía
    inline float melCut (float n) { return expMap (n, 200.0f, 16000.0f); }
    inline float melReso (float n) { return linMap (n, 0.0f, 0.9f); }
    inline float melEnvOct (float n) { return linMap (n, 0.0f, 5.0f); }
    inline float vibratoCents (float n) { return linMap (n, 0.0f, 45.0f); }
    // Texturas
    inline float texPitch (float n) { return linMap (n, -24.0f, 24.0f); }
    inline float texPitchEnv (float n) { return linMap (n, -24.0f, 24.0f); }
}

// Modelos de la voz de melodía (la lista solo crece al final)
inline const std::vector<std::string>& melodyModelNames()
{
    static const std::vector<std::string> n { "Pluck", "Lead", "Campana", "Organo" };
    return n;
}

// Texto que muestra cada perilla de sonido
inline std::string describeSound (int t, int p, float n)
{
    using namespace maps;
    switch (t)
    {
        case Kick:
            switch (p)
            {
                case 0: return fmtHz (kickTune (n));
                case 1: return fmtMs (kickDecay (n));
                case 2: return fmt ("%.0f st", kickPunch (n));
                case 3: return fmtMs (kickCurve (n));
                case 6: return fmtHz (colorHz (n));
                default: return fmtPct (n);
            }
        case Clap:
            switch (p)
            {
                case 0: return fmtHz (clapTone (n));
                case 1: return fmtMs (clapDecay (n));
                case 2: return fmtMs (clapSpread (n));
                case 3: return fmt ("%.0f", (double) clapBursts (n));
                default: return fmtPct (n);
            }
        case Rim:
            switch (p)
            {
                case 0: return fmtHz (rimTone (n));
                case 1: return fmtMs (rimDecay (n));
                case 6: return fmtHz (rimBright (n));
                default: return fmtPct (n);
            }
        case HatC:
        case HatO:
            switch (p)
            {
                case 0: return fmt ("x%.2f", hatTone (n));
                case 1: return fmtMs (t == HatC ? hatCDecay (n) : hatODecay (n));
                case 3: return fmtHz (hatFilter (n));
                case 4: return fmt ("Q %.1f", hatReso (n));
                case 6: return fmt ("%+.1f dB", airDb (n));
                default: return fmtPct (n);
            }
        case Perc:
            switch (p)
            {
                case 0: return fmtHz (percTone (n));
                case 1: return fmtMs (percDecay (n));
                case 2: return fmt ("%.1f", percFm (n));
                case 3: return fmt ("x%.2f", percRatio (n));
                case 4: return fmt ("%.0f st", percPitch (n));
                default: return fmtPct (n);
            }
        case Bass:
            switch (p)
            {
                case 0: return fmt ("%+.0f oct", (double) bassOctave (n));
                case 1: return fmtMs (synthDecay (n));
                case 2: return fmtHz (bassCut (n));
                case 4: return fmt ("%.1f oct", envOct (n));
                case 6: return n < 0.34f ? "SIERRA" : (n < 0.67f ? "CUADRADA" : "SUB");
                case 7: return fmtMs (glideMs (n));
                default: return fmtPct (n);
            }
        case Chord:
            switch (p)
            {
                case 0: return chordTypeName (chordType (n));
                case 1: return fmtMs (synthDecay (n));
                case 2: return fmtHz (chordCut (n));
                case 4: return fmt ("%.1f oct", chordEnvOct (n));
                case 5: return fmt ("%.0f ct", detuneCents (n));
                case 7: return fmtMs (attackMs (n));
                default: return fmtPct (n);
            }
        case Melody:
            switch (p)
            {
                case 1: return fmtMs (synthDecay (n));
                case 2: return fmtHz (melCut (n));
                case 4: return fmt ("%.1f oct", melEnvOct (n));
                case 6: return fmt ("%.0f ct", vibratoCents (n));
                case 7: return fmtMs (glideMs (n));
                default: return fmtPct (n);
            }
        case Texture:
            switch (p)
            {
                case 2: return fmt ("%+.1f st", texPitch (n));
                case 3: return fmtHz (expMap (n, 60.0f, 20000.0f));
                case 7: return fmt ("%+.0f st", texPitchEnv (n));
                default: return fmtPct (n);
            }
        default: return fmtPct (n);
    }
}

// Nivel de pista: 0 = silencio, 0.889 = 0 dB, 1 = +6 dB
inline float levelDb (float n) { return -48.0f + 54.0f * clampv (n, 0.0f, 1.0f); }
inline float levelGain (float n) { return n <= 0.001f ? 0.0f : dbToGain (levelDb (n)); }

inline const char* noteName (int midi)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[((midi % 12) + 12) % 12];
}

} // namespace ogro2
