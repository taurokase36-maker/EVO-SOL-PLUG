// Test del motor completo de EVO SOL
#include "../Source/dsp/VocalEngine.h"
#include "TestUtils.h"
#include <chrono>
#include <iostream>

using namespace evo;

static std::vector<float> melody (int fs, double seconds)
{
    // Melodía en La menor / Do mayor, desafinada a propósito, con frases y "s"
    const float notes[] = { 57, 60, 62, 64, 62, 60, 59, 57, 64, 65, 64, 62, 60, 57 };
    const float off[] = { 0.3f, -0.35f, 0.2f, -0.25f, 0.4f, -0.2f, 0.3f, -0.4f, 0.15f, -0.3f, 0.25f, -0.15f, 0.35f, -0.3f };
    std::vector<float> m;
    const int noteLen = (int) (0.42 * fs);
    int k = 0;
    while ((double) m.size() < seconds * fs)
    {
        const int idx = k % 14;
        for (int i = 0; i < noteLen; ++i)
        {
            const double t = (double) i / fs;
            float v = notes[idx] + off[idx] + 0.18f * (float) std::sin (2 * M_PI * 5.2 * t) * (float) std::min (1.0, t * 3);
            if (i > noteLen - (int) (0.05 * fs)) v = (k % 4 == 3) ? NAN : -1.0f; // hueco o sibilante
            m.push_back (v);
        }
        ++k;
    }
    return m;
}

static double rmsDb (const std::vector<float>& x, size_t a, size_t b)
{
    double e = 0;
    for (size_t i = a; i < b && i < x.size(); ++i) e += (double) x[i] * x[i];
    return 10 * std::log10 (e / std::max<size_t> (1, b - a) + 1e-20);
}

int main()
{
    const int fs = 48000;
    const int block = 480;

    // ---------- 1) Alineación / latencia con todo en neutro ----------
    {
        VocalEngine eng;
        eng.prepare (fs, 512);
        EngineParams p;
        p.riderOn = false; p.smooth = 0.0f; p.deEss = 0.0f; p.pitch.correctionOn = false;
        p.coreOn = false; p.hpfHz = 20.0f; p.bodyDb = 0; p.presenceDb = 0; p.airDb = 0;
        p.reverbMix = 0; p.delayMix = 0; p.doubler = 0; p.ceilingDb = 0.0f;
        eng.setParams (p);
        const int len = fs * 2;
        std::vector<float> x (len, 0.0f), yl (len), yr (len);
        std::mt19937 rng (3);
        std::normal_distribution<float> nd (0, 0.05f);
        Biquad lpA, lpB; lpA.set (Biquad::Type::LowPass, fs, 9000.0, 0.54); lpB.set (Biquad::Type::LowPass, fs, 9000.0, 1.31);
        Biquad hpA; hpA.set (Biquad::Type::HighPass, fs, 150.0, 0.7);
        for (int i = fs / 2; i < len; ++i) x[i] = hpA.process (lpB.process (lpA.process (nd (rng))));
        for (int s = 0; s < len; s += block)
            eng.process (x.data() + s, x.data() + s, yl.data() + s, yr.data() + s, std::min (block, len - s));
        const int L = eng.getLatencySamples();
        // Correlación cruzada alrededor de la latencia reportada
        int bestLag = -1; double best = -1;
        for (int lag = L - 40; lag <= L + 40; ++lag)
        {
            double c = 0;
            for (int i = fs; i < len - 100; ++i) c += (double) yl[i] * x[i - lag];
            if (c > best) { best = c; bestLag = lag; }
        }
        double err = 0, ref = 0;
        for (int i = fs; i < len - 100; ++i) { const double d = yl[i] - x[i - L]; err += d * d; ref += (double) x[i - L] * x[i - L]; }
        std::cout << "[latencia] reportada=" << L << " (" << 1000.0 * L / fs << " ms), medida=" << bestLag
                  << ", residuo neutro=" << 10 * std::log10 (err / ref) << " dB\n";
    }

    // ---------- 2) Cadena completa con voz sintética ----------
    const auto contour = melody (fs, 12.0);
    auto voice = evotest::synthVoice (contour, fs, -26.0f); // toma baja de nivel a propósito
    const int len = (int) voice.size();

    struct Preset { const char* name; EngineParams p; };
    std::vector<Preset> presets;
    {
        EngineParams p; p.pitch.key = 9; p.pitch.scale = ScaleType::NaturalMinor; p.pitch.retuneMs = 0.0f;
        p.comp = 0.7f; p.warmth = 0.5f; p.reverbMix = 0.2f; p.delayMix = 0.25f; p.delayBeats = 0.5f; p.doubler = 0.4f;
        presets.push_back ({ "trap_hard", p });
    }
    {
        EngineParams p; p.pitch.key = 9; p.pitch.scale = ScaleType::NaturalMinor; p.pitch.retuneMs = 45.0f; p.pitch.humanize = 0.4f;
        p.comp = 0.5f; p.warmth = 0.3f; p.reverbMix = 0.25f; p.smooth = 0.5f;
        p.pitch.harmony[0] = HarmonyInterval::ThirdUp; p.pitch.harmony[1] = HarmonyInterval::OctDown; p.harmLevel = 0.5f;
        presets.push_back ({ "rnb_harmony", p });
    }
    {
        EngineParams p; p.pitch.correctionOn = false; p.comp = 1.0f; p.warmth = 1.0f; p.color = 1.0f; p.smooth = 1.0f; p.deEss = 1.0f;
        p.radio = 1.0f; p.doubler = 1.0f; p.delayMix = 1.0f; p.delayFeedback = 0.9f; p.reverbMix = 1.0f; p.reverbDecay = 6.0f;
        p.width = 2.0f; p.inGainDb = 24.0f; p.outGainDb = 12.0f; p.airDb = 10.0f;
        presets.push_back ({ "extremo", p });
    }

    for (auto& pr : presets)
    {
        VocalEngine eng;
        eng.prepare (fs, 512);
        eng.setParams (pr.p);
        std::vector<float> yl (len), yr (len);
        float maxFet = 0, maxOpto = 0, maxDe = 0, maxSm = 0, maxLim = 0, minRider = 99, maxRider = -99;
        const auto t0 = std::chrono::high_resolution_clock::now();
        for (int s = 0; s < len; s += block)
        {
            eng.process (voice.data() + s, voice.data() + s, yl.data() + s, yr.data() + s, std::min (block, len - s));
            maxFet = std::max (maxFet, eng.meters.fetGr.load());
            maxOpto = std::max (maxOpto, eng.meters.optoGr.load());
            maxDe = std::max (maxDe, eng.meters.deEssGr.load());
            maxSm = std::max (maxSm, eng.meters.smoothGr.load());
            maxLim = std::max (maxLim, eng.meters.limGr.load());
            minRider = std::min (minRider, eng.meters.riderDb.load());
            maxRider = std::max (maxRider, eng.meters.riderDb.load());
        }
        const double secs = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();
        bool finite = true; float peak = 0;
        for (int i = 0; i < len; ++i) { finite &= std::isfinite (yl[i]) && std::isfinite (yr[i]); peak = std::max ({ peak, std::fabs (yl[i]), std::fabs (yr[i]) }); }
        std::cout << "[" << pr.name << "] cpu=" << 100.0 * secs / (len / (double) fs) << "% finite=" << finite
                  << " peak=" << gainToDb (peak) << " dBFS  in RMS=" << rmsDb (voice, fs, len) << " out RMS=" << rmsDb (yl, fs, len)
                  << " | GR fet=" << maxFet << " opto=" << maxOpto << " deess=" << maxDe << " smooth=" << maxSm
                  << " lim=" << maxLim << " rider=[" << minRider << "," << maxRider << "]\n";
        evotest::writeWav (std::string ("out/engine_") + pr.name + ".wav", { yl, yr }, fs);
    }
    evotest::writeWav ("out/engine_input.wav", { voice }, fs);

    // ---------- 3) Asistente ----------
    {
        VocalEngine eng;
        eng.prepare (fs, 512);
        EngineParams p;
        eng.setParams (p);
        eng.startLearning();
        p.pitch.learnKey = true;
        eng.setParams (p);
        std::vector<float> yl (len), yr (len);
        for (int s = 0; s < len; s += block)
            eng.process (voice.data() + s, voice.data() + s, yl.data() + s, yr.data() + s, std::min (block, len - s));
        const auto sug = eng.suggestion();
        std::cout << "[asistente] valido=" << sug.valid << " key=" << noteNames()[sug.key] << (sug.minor ? " menor" : " mayor")
                  << " conf=" << sug.keyConfidence << " rango=" << sug.range << " hpf=" << sug.hpfHz
                  << " cuerpo=" << sug.bodyDb << " presencia=" << sug.presenceDb << " aire=" << sug.airDb
                  << " deess=" << sug.deEss << " entrada=" << sug.inputGainDb << " seg=" << sug.secondsAnalysed << "\n";
    }

    // ---------- 4) Sobremuestreo: retardo exacto ----------
    {
        Oversampler4x os;
        std::vector<float> outv;
        for (int i = 0; i < 200; ++i)
        {
            float u[4];
            os.up (i == 10 ? 1.0f : 0.0f, u);
            outv.push_back (os.down (u));
        }
        int pk = 0;
        for (int i = 0; i < 200; ++i) if (std::fabs (outv[i]) > std::fabs (outv[pk])) pk = i;
        std::cout << "[oversampler] pico en " << pk - 10 << " muestras (esperado " << Oversampler4x::latencySamples()
                  << "), valor=" << outv[pk] << " vecinos=" << outv[pk - 1] << "," << outv[pk + 1] << "\n";
    }
    return 0;
}
