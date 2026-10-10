// EVO SOL — Asistente inteligente: escucha la voz unos segundos, mide su
// tonalidad, registro, balance espectral, sibilancia y nivel, y propone un
// ajuste de partida para toda la cadena.
#pragma once

#include "Common.h"
#include "Scales.h"

namespace evo
{
class VocalAssistant
{
public:
    static constexpr int kBands = 7;

    struct Suggestion
    {
        bool valid = false;
        int key = 0;
        bool minor = false;
        float keyConfidence = 0.0f;
        int range = 1;              // 0 soprano, 1 alto/tenor, 2 grave
        float hpfHz = 80.0f;
        float bodyDb = 0.0f;
        float presenceDb = 0.0f;
        float airDb = 3.0f;
        float deEss = 0.4f;
        float inputGainDb = 0.0f;
        float secondsAnalysed = 0.0f;
    };

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        static const double centers[kBands] = { 150, 350, 700, 1400, 2800, 5600, 11200 };
        for (int b = 0; b < kBands; ++b)
            bands[b].set (Biquad::Type::BandPass, fs, std::min (centers[b], fs * 0.42), 1.4);
        reset();
    }

    void reset()
    {
        for (auto& f : bands) f.reset();
        std::fill (std::begin (energy), std::end (energy), 0.0);
        std::fill (std::begin (pitchHist), std::end (pitchHist), 0.0);
        voicedSamples = 0;
        levelSum = 0.0;
        levelCount = 0;
        rmsAcc = 0.0;
        rmsCount = 0;
    }

    // x: señal mono tras la ganancia de entrada (antes del rider)
    inline void push (float x, bool voiced, float midi) noexcept
    {
        float b[kBands];
        for (int i = 0; i < kBands; ++i) b[i] = bands[i].process (x);
        if (! voiced) return;
        for (int i = 0; i < kBands; ++i) energy[i] += (double) b[i] * b[i];
        ++voicedSamples;
        rmsAcc += (double) x * x;
        if (++rmsCount >= 2048)
        {
            levelSum += 10.0 * std::log10 (rmsAcc / rmsCount + 1.0e-12);
            ++levelCount;
            rmsAcc = 0.0; rmsCount = 0;
        }
        const int bin = clampv ((int) std::lround (midi * 4.0f), 0, 511);
        pitchHist[bin] += 1.0;
    }

    float secondsAnalysed() const { return (float) (voicedSamples / fs); }

    Suggestion compute (const KeyDetector::Result& key) const
    {
        Suggestion s;
        s.secondsAnalysed = secondsAnalysed();
        if (voicedSamples < (int64_t) (fs * 1.5)) return s;
        s.valid = true;
        s.key = key.key;
        s.minor = key.minor;
        s.keyConfidence = key.confidence;

        // Registro: percentiles de pitch
        double total = 0.0;
        for (double v : pitchHist) total += v;
        auto percentile = [&] (double p) {
            double accum = 0.0;
            for (int i = 0; i < 512; ++i)
            {
                accum += pitchHist[i];
                if (accum >= p * total) return (float) i / 4.0f;
            }
            return 60.0f;
        };
        const float p5 = percentile (0.05), p50 = percentile (0.5);
        const float f5 = 440.0f * std::pow (2.0f, (p5 - 69.0f) / 12.0f);
        const float f50 = 440.0f * std::pow (2.0f, (p50 - 69.0f) / 12.0f);
        s.range = f50 > 320.0f ? 0 : (f5 < 90.0f ? 2 : 1);
        s.hpfHz = clampv (0.6f * f5, 50.0f, 140.0f);

        // Balance espectral contra un perfil de voz moderna (dB relativos a 1.4 kHz)
        static const float target[kBands] = { 1.0f, 3.0f, 2.0f, 0.0f, -3.0f, -7.0f, -13.0f };
        float lv[kBands];
        for (int i = 0; i < kBands; ++i) lv[i] = (float) (10.0 * std::log10 (energy[i] / (double) voicedSamples + 1.0e-14));
        auto rel = [&] (int i) { return (lv[i] - lv[3]) - target[i]; };
        s.bodyDb = clampv (-0.6f * rel (1), -4.0f, 4.0f);
        s.presenceDb = clampv (-0.6f * rel (4), -4.0f, 4.0f);
        s.airDb = clampv (2.0f - 0.6f * rel (6), 0.0f, 7.0f);
        s.deEss = clampv (0.35f + rel (5) / 12.0f, 0.15f, 0.85f);

        // Nivel: llevar el RMS de la voz a unos -20 dBFS
        if (levelCount > 0)
            s.inputGainDb = clampv (-20.0f - (float) (levelSum / levelCount), -12.0f, 12.0f);
        return s;
    }

private:
    double fs = 44100.0;
    Biquad bands[kBands];
    double energy[kBands] {};
    double pitchHist[512] {};
    int64_t voicedSamples = 0;
    double levelSum = 0.0, rmsAcc = 0.0;
    int levelCount = 0, rmsCount = 0;
};

} // namespace evo
