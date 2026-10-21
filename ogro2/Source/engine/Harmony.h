// OGRO 2 — armonía: de la NOTA de un paso a alturas reales (con MODO TONAL, grados de la
// escala) y armado de acordes (paralelo o diatónico, inversión, apertura y voice leading).
#pragma once

#include "VoiceMaps.h"
#include <algorithm>

namespace ogro2
{
struct Tonality
{
    bool tonal = false;   // la NOTA del paso es un grado de la escala
    int root = 9;         // tónica (0 = C)
    int scale = 0;
};

// Semitonos desde la nota base de la pista (sin la tónica): grado o semitono + octava
inline float stepSemis (const Tonality& k, float note, int octave)
{
    if (! k.tonal) return note + (float) (octave * 12);
    return (float) (degreeToSemis ((int) std::lround (note), k.scale) + octave * 12);
}

struct ChordShape
{
    int type = 0;
    bool diatonic = false;
    int inversion = 0;     // 0..3
    float spread = 0.0f;   // 0 = cerrado, 1 = abierto
    bool voiceLeading = false;
};

// Arma las notas MIDI (float) de un acorde. base = nota de la raíz (48 + tónica + semitonos del
// paso). En PARALELO, sin inversión ni apertura, da exactamente lo mismo que la 1.0.
// prev / prevCount: el acorde anterior (para el voice leading).
inline int buildChord (const ChordShape& sh, const Tonality& k, float base, float stepNote, int stepOctave,
                       const float* prev, int prevCount, float* out)
{
    int count = 0;
    if (sh.diatonic)
    {
        // Grado de la raíz: con MODO TONAL es la NOTA; si no, el grado más cercano a la altura
        const int rootDeg = k.tonal ? (int) std::lround (stepNote) : semisToDegree ((int) std::lround (stepNote), k.scale);
        const int* deg = chordDegrees (sh.type, count);
        const float tonic = base - stepSemis (k, stepNote, stepOctave);   // nota de la tónica en esta octava
        for (int i = 0; i < count; ++i)
            out[i] = tonic + (float) (degreeToSemis (rootDeg + deg[i], k.scale) + stepOctave * 12);
    }
    else
    {
        const float* iv = chordIntervals (sh.type, count);
        for (int i = 0; i < count; ++i) out[i] = base + iv[i];
    }
    if (sh.voiceLeading && prevCount > 0)
    {
        // Elige la inversión y la octava que menos se mueven respecto del acorde anterior
        float prevMean = 0.0f;
        for (int j = 0; j < prevCount; ++j) prevMean += prev[j];
        prevMean /= (float) prevCount;
        float best[kMaxChordNotes] {}, bestCost = 1e9f;
        for (int inv = 0; inv < count; ++inv)
            for (int oct = -2; oct <= 1; ++oct)
            {
                float cand[kMaxChordNotes];
                for (int i = 0; i < count; ++i) cand[i] = out[i] + (float) (12 * oct) + (i < inv ? 12.0f : 0.0f);
                float lo = 1e9f, hi = -1e9f, mean = 0.0f, cost = 0.0f;
                for (int i = 0; i < count; ++i)
                {
                    lo = std::min (lo, cand[i]); hi = std::max (hi, cand[i]); mean += cand[i];
                    float dmin = 1e9f;
                    for (int j = 0; j < prevCount; ++j) dmin = std::min (dmin, std::fabs (cand[i] - prev[j]));
                    cost += dmin;
                }
                mean /= (float) count;
                cost += 0.5f * std::fabs (mean - prevMean);
                if (lo < 40.0f || hi > 88.0f) cost += 100.0f;   // fuera del registro cómodo
                if (cost < bestCost) { bestCost = cost; std::copy (cand, cand + count, best); }
            }
        std::copy (best, best + count, out);
    }
    else
        for (int i = 0; i < std::min (sh.inversion, count - 1); ++i) out[i] += 12.0f;
    // Apertura: sube una octava las notas de posición impar (desde abajo)
    if (sh.spread > 0.01f && count >= 3)
    {
        std::sort (out, out + count);
        const int raise = (int) std::lround (sh.spread * (float) (count / 2));
        for (int r = 0, i = 1; r < raise && i < count; ++r, i += 2) out[i] += 12.0f;
    }
    if (sh.voiceLeading || sh.inversion > 0 || sh.spread > 0.01f) std::sort (out, out + count);
    return count;
}

} // namespace ogro2
