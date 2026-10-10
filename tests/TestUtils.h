// Utilidades para tests offline del DSP de EVO SOL
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace evotest
{
inline void writeWav (const std::string& path, const std::vector<std::vector<float>>& ch, int fs)
{
    const int numCh = (int) ch.size();
    const int n = (int) ch[0].size();
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    const uint32_t dataBytes = (uint32_t) (n * numCh * 4);
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); std::fwrite ("WAVE", 1, 4, f);
    std::fwrite ("fmt ", 1, 4, f); w32 (16); w16 (3); w16 ((uint16_t) numCh);
    w32 ((uint32_t) fs); w32 ((uint32_t) (fs * numCh * 4)); w16 ((uint16_t) (numCh * 4)); w16 (32);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (int i = 0; i < n; ++i)
        for (int c = 0; c < numCh; ++c)
            std::fwrite (&ch[(size_t) c][(size_t) i], 4, 1, f);
    std::fclose (f);
}

// Resonador de 2 polos para formantes
struct Formant
{
    double a1 = 0, a2 = 0, g = 1, y1 = 0, y2 = 0;
    void set (double fs, double f, double bw)
    {
        const double r = std::exp (-M_PI * bw / fs);
        a1 = -2 * r * std::cos (2 * M_PI * f / fs);
        a2 = r * r;
        g = (1 - r);
    }
    double process (double x)
    {
        const double y = g * x - a1 * y1 - a2 * y2;
        y2 = y1; y1 = y;
        return y;
    }
};

// Voz sintética: tren de pulsos glóticos (armónicos con caída) + formantes "a"
// pitchMidi: curva de pitch por muestra (NaN = no sonoro / ruido "s")
inline std::vector<float> synthVoice (const std::vector<float>& pitchMidi, int fs, float levelDb = -14.0f)
{
    std::vector<float> out (pitchMidi.size(), 0.0f);
    Formant f1, f2, f3;
    f1.set (fs, 730, 90); f2.set (fs, 1090, 110); f3.set (fs, 2440, 170);
    std::mt19937 rng (7);
    std::normal_distribution<float> nd (0.0f, 1.0f);
    double phase = 0.0;
    double hp = 0.0, hpPrev = 0.0;
    for (size_t i = 0; i < pitchMidi.size(); ++i)
    {
        const float m = pitchMidi[i];
        double src = 0.0;
        if (std::isfinite (m) && m < 0.0f) { out[i] = 0.0f; continue; }
        if (std::isfinite (m))
        {
            const double f0 = 440.0 * std::pow (2.0, (m - 69.0) / 12.0);
            phase += f0 / fs;
            if (phase >= 1.0) phase -= 1.0;
            const int nh = (int) std::min (40.0, (fs * 0.45) / f0);
            for (int h = 1; h <= nh; ++h)
                src += std::sin (2 * M_PI * h * phase) / std::pow ((double) h, 1.1);
            src *= 0.6;
            src += 0.01 * nd (rng); // aire
            const double v = f1.process (src) * 1.0 + f2.process (src) * 0.6 + f3.process (src) * 0.3;
            out[i] = (float) v;
        }
        else
        {
            // Sibilante: ruido pasa-altos
            const double x = nd (rng);
            hp = 0.85 * (hp + x - hpPrev); hpPrev = x;
            out[i] = (float) (hp * 0.05);
        }
    }
    // Normaliza a nivel RMS objetivo en tramos sonoros
    double e = 0; size_t cnt = 0;
    for (size_t i = 0; i < out.size(); ++i) if (std::isfinite (pitchMidi[i]) && pitchMidi[i] > 0.0f) { e += out[i] * out[i]; ++cnt; }
    const double rms = std::sqrt (e / std::max<size_t> (1, cnt));
    const double g = std::pow (10.0, levelDb / 20.0) / std::max (1e-9, rms);
    for (auto& s : out) s = (float) (s * g);
    return out;
}

} // namespace evotest
