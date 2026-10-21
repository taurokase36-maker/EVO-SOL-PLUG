// OGRO 2 — análisis de afinación de un sample (C++ puro): cromagrama con saliencia armónica,
// nota del bajo y comparación con plantillas de acordes. Devuelve la raíz (nota MIDI), la
// calidad del acorde (m, M, m7, maj7, sus...) y si el sample tiene altura clara. No confía en
// la nota raíz del archivo: la usa solo cuando el audio no alcanza para decidir.
#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <string>
#include <vector>

namespace ogro2
{
struct SampleAnalysis
{
    int rootMidi = 60;        // raíz con octava (MIDI)
    int quality = 0;          // índice en analysisQualities()
    bool tonal = false;       // altura clara (si no, no se transpone)
    float confidence = 0.0f;  // 0..1
    float chroma[12] {};      // para revisar a mano
};

// Calidades que reconoce el análisis (el manifiesto las guarda por nombre)
inline const std::vector<std::string>& analysisQualities()
{
    static const std::vector<std::string> q { "nota", "M", "m", "7", "maj7", "m7", "m9", "maj9", "sus2", "sus4", "5", "dim", "m7b5", "6", "m6", "add9" };
    return q;
}
inline int qualityIndex (const std::string& name)
{
    const auto& q = analysisQualities();
    for (size_t i = 0; i < q.size(); ++i) if (q[i] == name) return (int) i;
    return 0;
}
// ¿La calidad es menor? (para elegir samples que encajen en un grado de la escala)
inline bool qualityIsMinor (int q) { return q == 2 || q == 5 || q == 6 || q == 11 || q == 12 || q == 14; }

namespace analysis
{
inline void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * 3.14159265358979323846 / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k)
            {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}
} // namespace analysis

// mono: audio en float (-1..1); fileRoot: nota del archivo (se usa si el análisis duda)
inline SampleAnalysis analyseSample (const std::vector<float>& mono, double sampleRate, int fileRoot = 60)
{
    using namespace analysis;
    SampleAnalysis res;
    res.rootMidi = fileRoot;
    const size_t N = 16384;
    const size_t start = std::min (mono.size(), (size_t) (0.025 * sampleRate));
    const size_t end = std::min (mono.size(), start + (size_t) (2.5 * sampleRate));
    if (end <= start + 256) return res;

    // Espectro promedio (Hann, salto de media ventana; los samples cortos se rellenan con ceros)
    std::vector<double> mag (N / 2, 0.0);
    for (size_t pos = start; pos < end; pos += N / 2)
    {
        std::vector<std::complex<double>> buf (N);
        const size_t len = std::min (N, end - pos);
        for (size_t i = 0; i < len; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * (double) i / (double) (len - 1));
            buf[i] = mono[pos + i] * w;
        }
        fft (buf);
        for (size_t k = 0; k < N / 2; ++k) mag[k] += std::abs (buf[k]);
        if (len < N) break;
    }
    // Magnitud por semitono (notas MIDI 24..108): el máximo cerca de cada nota
    const double binHz = sampleRate / (double) N;
    double S[128] {};
    double maxS = 0.0;
    for (int n = 24; n <= 108; ++n)
    {
        const double f = 440.0 * std::pow (2.0, (n - 69) / 12.0);
        const int lo = (int) std::floor (f * std::pow (2.0, -0.4 / 12.0) / binHz), hi = (int) std::ceil (f * std::pow (2.0, 0.4 / 12.0) / binHz);
        double m = 0.0;
        for (int k = std::max (1, lo); k <= std::min ((int) N / 2 - 1, hi); ++k) m = std::max (m, mag[(size_t) k]);
        S[n] = m;
        maxS = std::max (maxS, m);
    }
    if (maxS <= 1e-9) return res;
    // Saliencia: cada nota suma sus armónicos; las notas sin fundamental pesan poco.
    // De grave a agudo, cada nota que suena le descuenta sus armónicos a las de arriba
    // (así el 3er armónico de un Do no aparece como un Sol que no está).
    double H[128] {};
    static const int harmSemis[6] = { 0, 12, 19, 24, 28, 31 };
    double R[128];
    std::copy (S, S + 128, R);
    for (int n = 24; n <= 96; ++n)
    {
        double h = 0.0;
        for (int k = 0; k < 6; ++k)
            if (n + harmSemis[k] <= 108) h += R[n + harmSemis[k]] * std::pow (0.8, k);
        H[n] = h * std::sqrt (R[n] / maxS);
        if (R[n] > 0.1 * maxS)
            for (int k = 1; k < 6; ++k)
                if (n + harmSemis[k] <= 108) R[n + harmSemis[k]] = std::max (0.0, R[n + harmSemis[k]] - R[n] * 0.6 / (double) (k + 1));
    }
    double maxH = 0.0;
    for (int n = 24; n <= 96; ++n) maxH = std::max (maxH, H[n]);
    // Cromagrama con las notas salientes
    double chroma[12] {};
    for (int n = 24; n <= 96; ++n)
        if (H[n] > 0.12 * maxH) chroma[n % 12] += H[n];
    double csum = 0.0, cmax = 0.0;
    for (double c : chroma) { csum += c; cmax = std::max (cmax, c); }
    if (csum <= 0.0) return res;
    for (int i = 0; i < 12; ++i) res.chroma[i] = (float) (chroma[i] / cmax);
    // Nota del bajo: la más grave con peso
    int bass = -1;
    for (int n = 28; n <= 84; ++n)
        if (H[n] > 0.3 * maxH && S[n] > 0.15 * maxS) { bass = n; break; }

    // Plantillas de acordes (intervalo, peso)
    struct Tpl { int q; int iv[5]; float w[5]; int count; };
    static const Tpl tpls[] = {
        { 0, { 0 }, { 1.0f }, 1 },
        { 1, { 0, 4, 7 }, { 1.0f, 0.9f, 0.8f }, 3 },
        { 2, { 0, 3, 7 }, { 1.0f, 0.9f, 0.8f }, 3 },
        { 3, { 0, 4, 7, 10 }, { 1.0f, 0.9f, 0.8f, 0.75f }, 4 },
        { 4, { 0, 4, 7, 11 }, { 1.0f, 0.9f, 0.8f, 0.75f }, 4 },
        { 5, { 0, 3, 7, 10 }, { 1.0f, 0.9f, 0.8f, 0.75f }, 4 },
        { 6, { 0, 3, 7, 10, 2 }, { 1.0f, 0.9f, 0.7f, 0.75f, 0.6f }, 5 },
        { 7, { 0, 4, 7, 11, 2 }, { 1.0f, 0.9f, 0.7f, 0.75f, 0.6f }, 5 },
        { 8, { 0, 2, 7 }, { 1.0f, 0.85f, 0.85f }, 3 },
        { 9, { 0, 5, 7 }, { 1.0f, 0.85f, 0.85f }, 3 },
        { 10, { 0, 7 }, { 1.0f, 0.9f }, 2 },
        { 11, { 0, 3, 6 }, { 1.0f, 0.9f, 0.85f }, 3 },
        { 12, { 0, 3, 6, 10 }, { 1.0f, 0.9f, 0.85f, 0.75f }, 4 },
        { 13, { 0, 4, 7, 9 }, { 1.0f, 0.9f, 0.8f, 0.7f }, 4 },
        { 14, { 0, 3, 7, 9 }, { 1.0f, 0.9f, 0.8f, 0.7f }, 4 },
        { 15, { 0, 4, 7, 2 }, { 1.0f, 0.9f, 0.8f, 0.6f }, 4 },
    };
    double cn[12], norm = 0.0;
    // Comprimido (raíz cuadrada): la tónica repetida en varias octavas no tapa a la 3ra y la 5ta
    for (int i = 0; i < 12; ++i) { cn[i] = std::sqrt (chroma[i] / cmax); norm += cn[i] * cn[i]; }
    norm = std::sqrt (norm);
    double best = -1.0, second = -1.0;
    int bestRoot = 0, bestQ = 0;
    for (const auto& t : tpls)
        for (int r = 0; r < 12; ++r)
        {
            double v[12] {}, tn = 0.0, dot = 0.0;
            for (int k = 0; k < t.count; ++k) v[(r + t.iv[k]) % 12] = t.w[k];
            for (int i = 0; i < 12; ++i) { tn += v[i] * v[i]; dot += v[i] * cn[i]; }
            double score = dot / (std::sqrt (tn) * norm + 1e-12);
            if (bass >= 0 && bass % 12 == r) score += 0.12;
            score -= 0.015 * t.count;   // con empate, el acorde más simple
            if (score > best) { second = best; best = score; bestRoot = r; bestQ = t.q; }
            else if (score > second && ! (r == bestRoot && t.q == bestQ)) second = score;
        }
    // ¿Tiene altura clara? Concentración del cromagrama y planitud del espectro
    double sorted[12];
    std::copy (cn, cn + 12, sorted);
    std::sort (sorted, sorted + 12, [] (double a, double b) { return a > b; });
    double top = 0.0, all = 0.0;
    for (int i = 0; i < 12; ++i) { all += sorted[i]; if (i < 4) top += sorted[i]; }
    double logSum = 0.0, linSum = 0.0;
    int cnt = 0;
    for (size_t k = (size_t) (100.0 / binHz); k < (size_t) std::min ((double) N / 2, 5000.0 / binHz); ++k)
    { logSum += std::log (mag[k] + 1e-12); linSum += mag[k]; ++cnt; }
    const double flatness = cnt > 0 ? std::exp (logSum / cnt) / (linSum / cnt + 1e-12) : 1.0;
    const double concentration = top / (all + 1e-12);
    res.confidence = (float) std::clamp ((best - second) * 4.0 + (concentration - 0.5), 0.0, 1.0);
    res.tonal = concentration > 0.5 && flatness < 0.4 && best > 0.72;
    res.quality = bestQ;
    // Raíz con octava: la nota más grave de esa clase que suena
    int rootMidi = -1;
    for (int n = 28; n <= 96; ++n)
        if (n % 12 == bestRoot && H[n] > 0.15 * maxH) { rootMidi = n; break; }
    if (rootMidi < 0) rootMidi = 48 + bestRoot;
    res.rootMidi = rootMidi;
    if (! res.tonal && fileRoot != 60) res.rootMidi = fileRoot;
    return res;
}

} // namespace ogro2
