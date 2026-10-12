#include "../Source/engine/Engine.h"
#include "../Source/engine/Presets.h"
#include "../../tests/TestUtils.h"
#include <iostream>
using namespace ogro;
int main (int argc, char** argv)
{
    const int fs = 44100; const double bpm = 124;
    int pi = argc > 1 ? std::atoi (argv[1]) : 3;
    const auto& p = factoryPresets()[(size_t) pi];
    const auto& defs = paramDefs();
    for (int solo = 0; solo < kTracks; ++solo)
    {
        Engine eng; p.build (eng.bank);
        std::vector<float> v (defs.size()); for (size_t i = 0; i < defs.size(); ++i) v[i] = defs[i].def;
        for (auto& [id, val] : p.values) v[(size_t) paramIndex (id)] = val;
        for (int t = 0; t < kTracks; ++t) v[(size_t) P::track (t, P::Mute)] = t == solo ? 0.f : 1.f;
        v[P::DlyRet] = 0; v[P::RevRet] = 0; v[P::MTexture] = 0;
        eng.prepare (fs, 512);
        const int total = (int) (4 * 4 * 60.0 / bpm * fs);
        std::vector<float> l (total), r (total);
        for (int pos = 0; pos < total; pos += 512)
        {
            HostInfo h; h.valid = true; h.playing = true; h.bpm = bpm; h.ppq = (double) pos / fs * bpm / 60.0;
            eng.setParams (v.data(), (int) v.size());
            eng.process (h, nullptr, 0, l.data() + pos, r.data() + pos, std::min (512, total - pos));
        }
        double e = 0; float pk = 0; for (int i = 0; i < total; ++i) { e += l[i]*l[i]; pk = std::max (pk, std::fabs (l[i])); }
        std::printf ("%-8s rms=%6.1f dB pico=%6.1f dB\n", trackName (solo), 10*std::log10 (e/total+1e-20), 20*std::log10(pk+1e-9));
        evotest::writeWav ("out/solo_" + std::to_string (solo) + ".wav", { l, r }, fs);
    }
}
