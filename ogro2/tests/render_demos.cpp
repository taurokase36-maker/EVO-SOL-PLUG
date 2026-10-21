// Renderiza un demo de 16 compases por preset: A (8) -> B (4) -> D (4), con macros en movimiento
#include "../Source/engine/Engine.h"
#include "../Source/engine/Presets.h"
#include "TestUtils.h"
#include <iostream>

using namespace ogro2;

int main (int argc, char** argv)
{
    const int fs = 44100;
    const auto& list = factoryPresets();
    const auto& defs = paramDefs();
    for (size_t pi = 0; pi < list.size(); ++pi)
    {
        const auto& p = list[pi];
        Engine eng;
        p.build (eng.bank);
        std::vector<float> v (defs.size());
        for (size_t i = 0; i < defs.size(); ++i) v[i] = defs[i].def;
        for (const auto& [id, val] : p.values) v[(size_t) paramIndex (id)] = val;
        double bpm = v[P::Tempo];
        eng.prepare (fs, 512);
        const double spb = 4.0 * 60.0 / bpm * fs;
        const int bars = argc > 2 ? std::atoi (argv[2]) : 16;
        const int total = (int) (spb * bars) + fs;
        std::vector<float> l ((size_t) total), r ((size_t) total);
        for (int pos = 0; pos < total; pos += 512)
        {
            const double bar = pos / spb;
            v[P::Pattern] = bar < bars * 0.5 ? 0.0f : (bar < bars * 0.75 ? 1.0f : 3.0f);
            HostInfo h; h.valid = true; h.playing = pos < (int) (spb * bars); h.bpm = bpm; h.ppq = (double) pos / fs * bpm / 60.0;
            eng.setParams (v.data(), (int) v.size());
            eng.process (h, nullptr, 0, l.data() + pos, r.data() + pos, std::min (512, total - pos));
        }
        char name[256];
        std::snprintf (name, sizeof (name), "%s/%02d.wav", argc > 1 ? argv[1] : "out", (int) pi + 1);
        evotest::writeWav (name, { l, r }, fs);
        std::cout << name << " " << p.name << "\n";
    }
}
