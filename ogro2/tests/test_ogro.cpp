// Tests offline del motor de OGRO 2: renderiza cada preset y mide tiempos, CPU y niveles
#include "../Source/engine/Engine.h"
#include "../Source/engine/Presets.h"
#include "TestUtils.h"
#include <chrono>
#include <iostream>

using namespace ogro2;

static std::vector<float> presetValues (const Preset& p)
{
    const auto& defs = paramDefs();
    std::vector<float> v (defs.size());
    for (size_t i = 0; i < defs.size(); ++i) v[i] = defs[i].def;
    for (const auto& [id, val] : p.values)
    {
        const int idx = paramIndex (id);
        if (idx < 0) { std::cerr << "ID desconocido en preset " << p.name << ": " << id << "\n"; std::exit (1); }
        v[(size_t) idx] = val;
    }
    return v;
}

struct RenderResult { std::vector<float> l, r; double cpu; };

static RenderResult render (Engine& eng, std::vector<float> values, int fs, double bpm, int bars, int block,
                            int patternSwitchBar = -1, int newPattern = 0)
{
    eng.prepare (fs, block);
    const double samplesPerBar = 4.0 * 60.0 / bpm * fs;
    const int total = (int) (samplesPerBar * bars) + fs; // +1 s de cola
    RenderResult res;
    res.l.assign ((size_t) total, 0.0f);
    res.r.assign ((size_t) total, 0.0f);
    const auto t0 = std::chrono::high_resolution_clock::now();
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        HostInfo h;
        h.valid = true;
        h.playing = pos < (int) (samplesPerBar * bars);
        h.bpm = bpm;
        h.ppq = (double) pos / fs * bpm / 60.0;
        if (patternSwitchBar >= 0 && pos >= (int) (samplesPerBar * patternSwitchBar)) values[P::Pattern] = (float) newPattern;
        eng.setParams (values.data(), (int) values.size());
        eng.process (h, nullptr, 0, res.l.data() + pos, res.r.data() + pos, n);
    }
    res.cpu = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count() / ((double) total / fs) * 100.0;
    return res;
}

int main()
{
    const int fs = 44100;
    const double bpm = 124.0;
    bool ok = true;

    // ---- 1) Timing: kick solo, sin swing, cada negra exacta ----
    {
        Engine eng;
        auto v = presetValues (factoryPresets()[0]);
        for (int t = 1; t < kTracks; ++t) v[(size_t) P::track (t, P::Mute)] = 1.0f;
        v[P::Swing] = 0.5f;
        v[P::MComp] = 0; v[P::MWarmth] = 0; v[P::BusComp] = 0; v[P::BusCrudo] = 0;
        factoryPresets()[0].build (eng.bank);
        auto r = render (eng, v, fs, bpm, 2, 256);
        const int lat = eng.getLatencySamples();
        const double beat = 60.0 / bpm * fs;
        std::cout << "[timing] latencia=" << lat << " muestras\n";
        int found = 0; double maxErr = 0;
        for (int k = 0; k < 8; ++k)
        {
            const int expected = (int) std::lround (k * beat) + lat;
            int onset = -1;
            for (int i = std::max (0, expected - 200); i < expected + 400; ++i)
                if (std::fabs (r.l[(size_t) i]) > 0.02f) { onset = i; break; }
            if (onset >= 0) { ++found; maxErr = std::max (maxErr, std::fabs ((double) (onset - expected))); }
        }
        std::cout << "[timing] kicks encontrados=" << found << "/8, error max=" << maxErr << " muestras\n";
        ok &= found == 8 && maxErr < 6;
    }

    // ---- 2) Swing: hat cerrado en semicorcheas impares se retrasa ----
    {
        Engine eng;
        auto v = presetValues (factoryPresets()[0]);
        for (int t = 0; t < kTracks; ++t) v[(size_t) P::track (t, P::Mute)] = t == HatC ? 0.0f : 1.0f;
        v[P::Swing] = 0.66f;
        v[P::MComp] = 0; v[P::BusComp] = 0; v[P::BusCrudo] = 0;
        eng.bank.clearAll();
        Step s; s.on = true; s.vel = 110;
        eng.bank.set (0, HatC, 1, s);
        auto r = render (eng, v, fs, bpm, 1, 512);
        const int lat = eng.getLatencySamples();
        const double sixteenth = 60.0 / bpm * fs / 4.0;
        const double expected = sixteenth * (1.0 + (0.66 - 0.5) * 2.0) + lat;
        int onset = -1;
        for (int i = 0; i < (int) r.l.size(); ++i) if (std::fabs (r.l[(size_t) i]) > 0.005f) { onset = i; break; }
        std::cout << "[swing] esperado=" << (int) expected << " medido=" << onset << "\n";
        ok &= std::fabs (onset - expected) < 8;
    }

    // ---- 3) Todos los presets: sin NaN, nivel sano, CPU ----
    int idx = 0;
    for (const auto& p : factoryPresets())
    {
        Engine eng;
        p.build (eng.bank);
        auto v = presetValues (p);
        auto r = render (eng, v, fs, bpm, 8, 512, 4, 1);
        bool finite = true; float peak = 0; double e = 0;
        for (size_t i = 0; i < r.l.size(); ++i)
        {
            finite &= std::isfinite (r.l[i]) && std::isfinite (r.r[i]);
            peak = std::max ({ peak, std::fabs (r.l[i]), std::fabs (r.r[i]) });
            e += (double) r.l[i] * r.l[i] + (double) r.r[i] * r.r[i];
        }
        const double rms = 10 * std::log10 (e / (2.0 * (double) r.l.size()) + 1e-20);
        std::printf ("[preset %2d] %-18s cpu=%5.1f%%  pico=%6.2f dBFS  rms=%6.1f dB  finito=%d\n", idx, p.name.c_str(), r.cpu,
                     20 * std::log10 (peak + 1e-9), rms, finite ? 1 : 0);
        ok &= finite && peak < 1.0f && rms > -40.0;
        std::string fname = "out/ogro_" + std::to_string (idx) + ".wav";
        evotest::writeWav (fname, { r.l, r.r }, fs);
        ++idx;
    }

    // ---- 4) Estado: serializar y recuperar patrones ----
    {
        PatternBank a, b;
        factoryPresets()[3].build (a);
        const auto txt = a.serialise();
        b.deserialise (txt);
        bool same = true;
        for (int p = 0; p < kPatterns; ++p)
            for (int t = 0; t < kTracks; ++t)
            {
                same &= a.getLength (p, t) == b.getLength (p, t);
                for (int s = 0; s < kSteps; ++s) same &= a.get (p, t, s).pack() == b.get (p, t, s).pack();
            }
        for (int l = 0; l < kLanes; ++l) for (int i = 0; i < kLanePoints; ++i) same &= std::fabs (a.lane (l, i) - b.lane (l, i)) < 0.002f;
        std::cout << "[estado] serializacion " << (same ? "OK" : "FALLA") << " (" << txt.size() << " bytes)\n";
        ok &= same;
    }

    std::cout << (ok ? "TODO OK\n" : "HAY FALLAS\n");
    return ok ? 0 : 1;
}
