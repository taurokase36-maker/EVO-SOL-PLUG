// EVO SOL — escalas, notas y detección de tonalidad (Krumhansl-Schmuckler)
#pragma once

#include "Common.h"
#include <array>
#include <string>

namespace evo
{
enum class ScaleType : int
{
    Chromatic = 0, Major, NaturalMinor, HarmonicMinor, PentaMajor, PentaMinor,
    Blues, Dorian, Phrygian, Mixolydian, NumScales
};

inline const char* const* scaleNames()
{
    static const char* names[] = { "Cromática", "Mayor", "Menor", "Menor armónica",
                                   "Pentatónica mayor", "Pentatónica menor", "Blues",
                                   "Dórica", "Frigia", "Mixolidia" };
    return names;
}

inline const char* const* noteNames()
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names;
}

// Intervalos (semitonos desde la tónica) de cada escala
inline std::array<bool, 12> scaleMask (ScaleType type)
{
    static const std::vector<std::vector<int>> intervals = {
        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, // cromática
        { 0, 2, 4, 5, 7, 9, 11 },                 // mayor
        { 0, 2, 3, 5, 7, 8, 10 },                 // menor natural
        { 0, 2, 3, 5, 7, 8, 11 },                 // menor armónica
        { 0, 2, 4, 7, 9 },                        // pentatónica mayor
        { 0, 3, 5, 7, 10 },                       // pentatónica menor
        { 0, 3, 5, 6, 7, 10 },                    // blues
        { 0, 2, 3, 5, 7, 9, 10 },                 // dórica
        { 0, 1, 3, 5, 7, 8, 10 },                 // frigia
        { 0, 2, 4, 5, 7, 9, 10 },                 // mixolidia
    };
    std::array<bool, 12> m {};
    const int idx = clampv ((int) type, 0, (int) ScaleType::NumScales - 1);
    for (int s : intervals[(size_t) idx]) m[(size_t) s] = true;
    return m;
}

// Máscara de notas permitidas (pitch class absoluto) para una key + escala
inline std::array<bool, 12> keyMask (int key, ScaleType type)
{
    const auto rel = scaleMask (type);
    std::array<bool, 12> m {};
    for (int i = 0; i < 12; ++i)
        m[(size_t) ((i + key) % 12)] = rel[(size_t) i];
    return m;
}

inline bool isAllowed (const std::array<bool, 12>& mask, int midiNote)
{
    return mask[(size_t) (((midiNote % 12) + 12) % 12)];
}

// Nota permitida más cercana a un valor MIDI continuo
inline int nearestAllowed (const std::array<bool, 12>& mask, float midi)
{
    const int base = (int) std::floor (midi);
    int best = base;
    float bestDist = 1.0e9f;
    for (int d = -7; d <= 8; ++d)
    {
        const int n = base + d;
        if (! isAllowed (mask, n)) continue;
        const float dist = std::fabs ((float) n - midi);
        if (dist < bestDist) { bestDist = dist; best = n; }
    }
    return best;
}

// Mueve una nota "steps" grados dentro de la escala (armonía diatónica)
inline int shiftInScale (const std::array<bool, 12>& mask, int note, int steps)
{
    int n = note;
    const int dir = steps > 0 ? 1 : -1;
    for (int s = 0; s < std::abs (steps); ++s)
    {
        int guard = 0;
        do { n += dir; } while (! isAllowed (mask, n) && ++guard < 13);
    }
    return n;
}

// ----------------------------------------------------------------------------
// Detección de tonalidad: histograma de clases de altura ponderado por duración,
// correlacionado con los perfiles de Krumhansl-Kessler (24 tonalidades).
// ----------------------------------------------------------------------------
class KeyDetector
{
public:
    struct Result { int key = 0; bool minor = false; float confidence = 0.0f; float totalSeconds = 0.0f; };

    void reset() { hist.fill (0.0); seconds = 0.0; }

    // Agrega un frame con pitch (MIDI continuo), peso y duración en segundos
    void addFrame (float midi, float weight, float durationSec)
    {
        // Solo cuenta notas razonablemente estables cerca del centro del semitono
        const float frac = midi - std::round (midi);
        const float centered = 1.0f - clampv (std::fabs (frac) * 2.0f, 0.0f, 1.0f); // 1 en el centro
        const int pc = (((int) std::lround (midi)) % 12 + 12) % 12;
        hist[(size_t) pc] += (double) (weight * durationSec * (0.35f + 0.65f * centered));
        seconds += durationSec;
    }

    Result analyse() const
    {
        static const double majorProfile[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        static const double minorProfile[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

        Result r;
        r.totalSeconds = (float) seconds;
        double total = 0.0;
        for (double v : hist) total += v;
        if (total <= 0.0) return r;

        double best = -2.0, second = -2.0;
        for (int mode = 0; mode < 2; ++mode)
        {
            const double* prof = mode == 0 ? majorProfile : minorProfile;
            for (int key = 0; key < 12; ++key)
            {
                const double c = correlate (prof, key);
                if (c > best) { second = best; best = c; r.key = key; r.minor = mode == 1; }
                else if (c > second) second = c;
            }
        }
        // Confianza: separación entre la mejor y la segunda + cantidad de material
        const double sep = clampv ((best - second) * 6.0, 0.0, 1.0);
        const double amount = clampv (seconds / 12.0, 0.0, 1.0);
        r.confidence = (float) clampv (0.25 + 0.45 * sep + 0.30 * amount, 0.0, 1.0) * (float) clampv (best, 0.0, 1.0);
        return r;
    }

    const std::array<double, 12>& histogram() const { return hist; }

private:
    double correlate (const double* profile, int key) const
    {
        double mx = 0, my = 0;
        for (int i = 0; i < 12; ++i) { mx += hist[(size_t) i]; my += profile[i]; }
        mx /= 12.0; my /= 12.0;
        double sxy = 0, sxx = 0, syy = 0;
        for (int i = 0; i < 12; ++i)
        {
            const double x = hist[(size_t) ((i + key) % 12)] - mx;
            const double y = profile[i] - my;
            sxy += x * y; sxx += x * x; syy += y * y;
        }
        return (sxx > 0 && syy > 0) ? sxy / std::sqrt (sxx * syy) : 0.0;
    }

    std::array<double, 12> hist {};
    double seconds = 0.0;
};

} // namespace evo
