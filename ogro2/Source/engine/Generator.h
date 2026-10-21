// OGRO 2 — "INVENTAR": genera un patrón con sentido musical para una pista
#pragma once

#include "Sequencer.h"

namespace ogro2
{
inline void inventPattern (PatternBank& bank, int pattern, int track, uint32_t seed)
{
    uint32_t r = seed ? seed : 1u;
    auto rnd = [&r] () { r ^= r << 13; r ^= r >> 17; r ^= r << 5; return r; };
    auto chance = [&] (int pct) { return (int) (rnd() % 100u) < pct; };
    // Pentatónica menor + 9na: suena bien sobre cualquier acorde menor de deep house
    static const int scale[] = { 0, 3, 5, 7, 10, 12, 14, 15 };

    const int len = bank.getLength (pattern, track);
    for (int s = 0; s < kSteps; ++s)
    {
        Step st;
        const bool beat = s % 4 == 0, off = s % 4 == 2, odd = (s & 1) != 0;
        switch (track)
        {
            case Kick:
                st.on = beat || (s == 14 && chance (25)) || (s == 7 && chance (15)) || (s == 11 && chance (15));
                st.vel = beat ? (s == 0 ? 120 : 104) : 80;
                break;
            case Clap:
                st.on = s == 4 || s == 12 || (s == 15 && chance (20)) || (s == 7 && chance (12));
                st.vel = (s == 4 || s == 12) ? 105 : 60;
                break;
            case Rim:
                st.on = (odd && chance (30)) || (s == 3 && chance (60)) || (s == 10 && chance (50));
                st.vel = 50 + (int) (rnd() % 60u);
                st.prob = chance (30) ? 70 : 100;
                break;
            case HatC:
                st.on = ! beat || chance (40);
                st.vel = off ? 100 : (odd ? 55 + (int) (rnd() % 30u) : 70);
                st.ratchet = (s == 15 && chance (35)) ? 2 : 1;
                break;
            case HatO:
                st.on = off && chance (85);
                st.vel = 90;
                break;
            case Perc:
                st.on = chance (s % 3 == 0 ? 45 : 22);
                st.vel = 60 + (int) (rnd() % 60u);
                st.note = scale[rnd() % 8u] - (chance (30) ? 12 : 0);
                break;
            case Bass:
                st.on = (! beat && chance (55)) || (s == 0 && chance (40));
                st.vel = chance (25) ? 122 : 96;
                st.note = chance (55) ? 0 : (chance (50) ? scale[rnd() % 5u] : (chance (50) ? 12 : -2));
                st.slide = st.on && chance (18);
                break;
            case Chord:
                st.on = (s == 2 || s == 6 || s == 10 || s == 13) ? chance (55) : chance (8);
                st.vel = 92 + (int) (rnd() % 30u);
                st.note = chance (70) ? 0 : (chance (50) ? -2 : (chance (50) ? 5 : 3));
                break;
            default: break;
        }
        if (s >= len) st.on = false;
        bank.set (pattern, track, s, st);
    }
    // El bajo y los acordes siempre arrancan con algo
    if (track == Chord || track == Bass)
    {
        bool any = false;
        for (int s = 0; s < len; ++s) any |= bank.get (pattern, track, s).on;
        if (! any) { Step st; st.on = true; st.vel = 100; bank.set (pattern, track, track == Bass ? 2 : 0, st); }
    }
}

} // namespace ogro2
