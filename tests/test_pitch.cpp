// Test offline del motor de afinación
#include "../Source/dsp/PitchEngine.h"
#include "TestUtils.h"
#include <chrono>
#include <iostream>

using namespace evo;

static std::vector<float> makeContour (int fs)
{
    std::vector<float> m;
    auto seg = [&] (double dur, auto fn) {
        const int len = (int) (dur * fs);
        for (int i = 0; i < len; ++i) m.push_back ((float) fn ((double) i / fs, dur));
    };
    const float sil = -1.0f, noise = NAN;
    seg (0.20, [&] (double, double) { return sil; });
    seg (0.70, [&] (double, double) { return 57.35; });
    seg (0.10, [&] (double t, double d) { return 57.35 + (59.6 - 57.35) * (t / d); });
    seg (0.70, [&] (double, double) { return 59.6; });
    seg (0.12, [&] (double, double) { return noise; });
    seg (0.98, [&] (double t, double) { return 62.2 + 0.3 * std::sin (2 * M_PI * 5.5 * t); });
    seg (0.20, [&] (double, double) { return sil; });
    seg (0.80, [&] (double, double) { return 51.7; });
    seg (0.20, [&] (double, double) { return sil; });
    return m;
}

struct Run
{
    std::string name;
    PitchEngine::Params p;
};

int main()
{
    const int fs = 44100;
    const auto contour = makeContour (fs);
    const auto voice = evotest::synthVoice (contour, fs);
    evotest::writeWav ("out/pitch_input.wav", { voice }, fs);

    std::vector<Run> runs;
    {
        Run r { "hard", {} };
        r.p.key = 0; r.p.scale = ScaleType::Major; r.p.retuneMs = 0.0f;
        runs.push_back (r);
    }
    {
        Run r { "natural", {} };
        r.p.key = 0; r.p.scale = ScaleType::Major; r.p.retuneMs = 60.0f; r.p.humanize = 0.5f;
        runs.push_back (r);
    }
    {
        Run r { "transpose3", {} };
        r.p.key = 0; r.p.scale = ScaleType::Major; r.p.retuneMs = 0.0f; r.p.transposeSemis = 3.0f;
        runs.push_back (r);
    }
    {
        Run r { "throat_up", {} };
        r.p.key = 0; r.p.scale = ScaleType::Major; r.p.retuneMs = 10.0f; r.p.throatSemis = 3.0f;
        runs.push_back (r);
    }
    {
        Run r { "throat_down", {} };
        r.p.key = 0; r.p.scale = ScaleType::Major; r.p.retuneMs = 10.0f; r.p.throatSemis = -3.0f;
        runs.push_back (r);
    }
    {
        Run r { "harmony", {} };
        r.p.key = 0; r.p.scale = ScaleType::Major; r.p.retuneMs = 5.0f;
        r.p.harmony[0] = HarmonyInterval::ThirdUp; r.p.harmony[1] = HarmonyInterval::FifthDown;
        runs.push_back (r);
    }
    {
        Run r { "bypass", {} };
        r.p.correctionOn = false;
        runs.push_back (r);
    }
    {
        Run r { "flex", {} };
        r.p.key = 0; r.p.scale = ScaleType::Major; r.p.retuneMs = 0.0f; r.p.flexTune = 0.8f;
        runs.push_back (r);
    }

    for (auto& run : runs)
    {
        PitchEngine eng;
        eng.prepare (fs, 512);
        eng.setParams (run.p);
        std::vector<float> lead (voice.size()), h1 (voice.size()), h2 (voice.size());
        FILE* csv = std::fopen (("out/pitch_" + run.name + "_meter.csv").c_str(), "w");
        const auto t0 = std::chrono::high_resolution_clock::now();
        const int block = 256;
        for (size_t pos = 0; pos < voice.size(); pos += block)
        {
            const int len = (int) std::min<size_t> (block, voice.size() - pos);
            eng.process (voice.data() + pos, lead.data() + pos, h1.data() + pos, h2.data() + pos, len);
            const auto& m = eng.getMeter();
            std::fprintf (csv, "%.4f,%d,%.3f,%.3f,%d\n", (double) pos / fs, m.voiced ? 1 : 0, m.inputMidi, m.outputMidi, m.targetNote);
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        std::fclose (csv);
        const double secs = std::chrono::duration<double> (t1 - t0).count();
        const double audio = (double) voice.size() / fs;

        // Validación numérica
        bool finite = true; float peak = 0.0f;
        for (float s : lead) { finite &= std::isfinite (s); peak = std::max (peak, std::fabs (s)); }
        std::cout << run.name << ": latency=" << eng.getLatencySamples() << " (" << 1000.0 * eng.getLatencySamples() / fs
                  << " ms) cpu=" << 100.0 * secs / audio << "% of realtime, finite=" << finite << " peak=" << peak << "\n";

        if (run.name == "harmony")
        {
            std::vector<float> mixL (voice.size()), mixR (voice.size());
            for (size_t i = 0; i < voice.size(); ++i)
            {
                mixL[i] = lead[i] + 0.6f * h1[i];
                mixR[i] = lead[i] + 0.6f * h2[i];
            }
            evotest::writeWav ("out/pitch_harmony_h1.wav", { h1 }, fs);
            evotest::writeWav ("out/pitch_harmony_h2.wav", { h2 }, fs);
            evotest::writeWav ("out/pitch_harmony_mix.wav", { mixL, mixR }, fs);
        }
        if (run.name == "bypass")
        {
            const int L = eng.getLatencySamples();
            double err = 0.0, ref = 0.0;
            for (size_t i = (size_t) L + 4000; i < voice.size(); ++i)
            {
                const double d = lead[i] - voice[i - (size_t) L];
                err += d * d; ref += (double) voice[i - (size_t) L] * voice[i - (size_t) L];
            }
            std::cout << "  bypass null residual: " << 10.0 * std::log10 (err / ref + 1e-30) << " dB\n";
        }
        evotest::writeWav ("out/pitch_" + run.name + ".wav", { lead }, fs);
    }
    return 0;
}
