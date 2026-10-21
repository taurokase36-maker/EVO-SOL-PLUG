// OGRO 2 — presets de fábrica por mood (minimal, house y deep house).
// Cada preset trae kit, mezcla, modulaciones, carriles dibujados y 8 patrones.
// Los 12 de la 1.0 están escritos con los IDs de la 1.0 y se cargan con v1::convert(),
// así suenan igual que en OGRO 1.0.
#pragma once

#include "Generator.h"
#include "Legacy.h"
#include <tuple>

namespace ogro2
{
struct Preset
{
    std::string name, mood;
    std::vector<std::pair<std::string, float>> values; // IDs de la 1.0
    std::function<void (PatternBank&)> build;
    // Presets de OGRO 2 (fase 7): valores por ID de la 2.0, samples por nombre y ruteo por ID
    bool v2 = false;
    std::vector<std::pair<std::string, float>> values2;
    std::string texSample, chordSample;
    std::vector<std::tuple<std::string, std::string, float>> mods;   // fuente, destino, cantidad
    std::vector<std::string> lanes;                                   // destino de M1..M4 ("" = nada)
};

namespace pb
{
    // Notación de pasos:  -  apagado   x  normal   X  acento   o  suave   .  ghost
    //                     r  ratchet 2 R ratchet 3 ?  50 %     s  ligado (slide)
    inline void row (PatternBank& b, int p, int t, const char* g, std::vector<int> notes = {}, int len = 16)
    {
        b.setLength (p, t, len);
        for (int s = 0; s < 16; ++s)
        {
            Step st;
            const char c = s < (int) std::strlen (g) ? g[s] : '-';
            st.on = c != '-' && c != ' ';
            switch (c)
            {
                case 'X': st.vel = 127; break;
                case 'o': st.vel = 72; break;
                case '.': st.vel = 44; break;
                case 'r': st.vel = 96; st.ratchet = 2; break;
                case 'R': st.vel = 92; st.ratchet = 3; break;
                case '?': st.vel = 90; st.prob = 50; break;
                case 's': st.vel = 100; st.slide = true; break;
                default: st.vel = 100; break;
            }
            if (! notes.empty()) st.note = notes[(size_t) s % notes.size()];
            b.set (p, t, s, st);
        }
    }

    inline uint32_t hashRand (uint32_t& s)
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return s;
    }

    // Deriva B (variación), C (break) y D (fill) desde A; E-H arrancan como copias
    inline void derive (PatternBank& b, uint32_t seed)
    {
        uint32_t r = seed ? seed : 1u;
        // B: más percusión fantasma y un hat abierto extra
        b.copyPattern (0, 1);
        for (int t : { (int) Rim, (int) Perc, (int) HatC })
            for (int s = 0; s < 16; ++s)
            {
                Step st = b.get (1, t, s);
                if (! st.on && hashRand (r) % 100 < 22) { st.on = true; st.vel = 40 + (int) (hashRand (r) % 30); st.prob = 80; b.set (1, t, s, st); }
            }
        { Step st = b.get (1, HatO, 14); st.on = true; st.vel = 90; b.set (1, HatO, 14, st); }

        // C: break sin kick ni clap, el bajo respira
        b.copyPattern (0, 2);
        for (int s = 0; s < 16; ++s)
        {
            Step k = b.get (2, Kick, s); k.on = false; b.set (2, Kick, s, k);
            Step c = b.get (2, Clap, s); c.on = false; b.set (2, Clap, s, c);
            if (s < 8) { Step ba = b.get (2, Bass, s); ba.on = false; b.set (2, Bass, s, ba); }
        }
        // D: fill con redobles al final del compás
        b.copyPattern (0, 3);
        for (int s = 12; s < 16; ++s)
        {
            Step c = b.get (3, Clap, s); c.on = true; c.vel = 70 + (s - 12) * 15; c.ratchet = s >= 14 ? 3 : 2; b.set (3, Clap, s, c);
            Step h = b.get (3, HatC, s); h.on = true; h.ratchet = 2; b.set (3, HatC, s, h);
        }
        { Step k = b.get (3, Kick, 14); k.on = true; k.vel = 90; b.set (3, Kick, 14, k); }
        for (int p = 0; p < 4; ++p) b.copyPattern (p, p + 4);
    }

    // Formas para los carriles dibujables
    inline void lane (PatternBank& b, int l, const std::function<float (float)>& f)
    {
        for (int i = 0; i < kLanePoints; ++i) b.setLane (l, i, f ((float) i / (float) kLanePoints));
    }
    inline float rampUp (float x) { return x; }
    inline float swell (float x) { return 0.5f - 0.5f * std::cos ((float) kTwoPi * x); }
    inline float sine (float x) { return 0.5f + 0.5f * std::sin ((float) kTwoPi * x); }
    inline float tri (float x) { return x < 0.5f ? 2.0f * x : 2.0f - 2.0f * x; }
    inline std::function<float (float)> steps (uint32_t seed, int count)
    {
        return [seed, count] (float x) {
            uint32_t s = seed + (uint32_t) (x * count) * 2654435761u;
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            return 0.2f + 0.6f * (float) (s % 1000u) / 1000.0f;
        };
    }

    // Ajuste fino de un paso (condición, gate, octava, tipo de acorde...)
    inline void tweak (PatternBank& b, int p, int t, int s, const std::function<void (Step&)>& f)
    {
        Step st = b.get (p, t, s);
        f (st);
        b.set (p, t, s, st);
    }
    inline void cond (PatternBank& b, int p, int t, int s, int c) { tweak (b, p, t, s, [c] (Step& st) { st.cond = c; }); }
    inline void gate (PatternBank& b, int p, int t, int s, int g) { tweak (b, p, t, s, [g] (Step& st) { st.gate = g; }); }
    inline constexpr int c12 = CondRatioBase + 0, c22 = CondRatioBase + 1, c14 = CondRatioBase + 5, c34 = CondRatioBase + 7, c44 = CondRatioBase + 8;

    inline float dst (int v1Dest) { return (float) v1Dest; }
    inline std::string sid (int t, int p) { return "t" + std::to_string (t + 1) + "_p" + std::to_string (p + 1); }
    inline std::string tid (int t, const char* f) { return "t" + std::to_string (t + 1) + "_" + f; }
}

inline const std::vector<Preset>& factoryPresets()
{
    using namespace pb;
    static const std::vector<Preset> list = [] {
        std::vector<Preset> v;

        // 1 ─ INICIAL ───────────────────────────────────────────────────
        v.push_back ({ "Inicial", "House clásico, punto de partida",
            { { "root", 9 }, { "swing", 0.54f } },
            [] (PatternBank& b) {
                b.clearAll();
                row (b, 0, Kick, "x---x---x---x---");
                row (b, 0, Clap, "----x-------x---");
                row (b, 0, HatC, "o-x-o-x-o-x-o-x.");
                row (b, 0, HatO, "--x---x---x---x-");
                row (b, 0, Bass, "--x--x----x--x-x", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 5 });
                derive (b, 11);
            } });

        // 2 ─ AMANECER ──────────────────────────────────────────────────
        v.push_back ({ "Amanecer", "Deep house cálido de terraza al salir el sol",
            { { "root", 9 }, { "swing", 0.58f }, { "tempo", 120 },
              { sid (Kick, 0), 0.32f }, { sid (Kick, 1), 0.5f }, { sid (Kick, 4), 0.2f }, { sid (Kick, 5), 0.25f }, { sid (Kick, 6), 0.45f },
              { sid (HatC, 1), 0.3f }, { sid (HatC, 6), 0.35f }, { tid (HatC, "lvl"), 0.78f }, { tid (HatC, "pan"), 0.6f },
              { sid (HatO, 1), 0.42f }, { tid (HatO, "lvl"), 0.74f },
              { tid (Clap, "lvl"), 0.8f }, { tid (Clap, "rev"), 0.35f }, { sid (Clap, 6), 0.45f },
              { sid (Perc, 0), 0.32f }, { sid (Perc, 2), 0.1f }, { sid (Perc, 4), 0.35f }, { tid (Perc, "pan"), 0.35f }, { tid (Perc, "lvl"), 0.76f },
              { sid (Bass, 2), 0.28f }, { sid (Bass, 3), 0.15f }, { sid (Bass, 4), 0.3f }, { sid (Bass, 6), 0.55f }, { sid (Bass, 1), 0.45f },
              { sid (Chord, 0), stepCentre (1, 8) }, { sid (Chord, 1), 0.55f }, { sid (Chord, 2), 0.42f }, { sid (Chord, 4), 0.35f },
              { sid (Chord, 6), 0.75f }, { tid (Chord, "rev"), 0.35f }, { tid (Chord, "dly"), 0.2f }, { tid (Chord, "lvl"), 0.83f }, { tid (Chord, "pump"), 0.45f },
              { "m_texture", 0.22f }, { "macro1", 0.62f }, { "dly_div", 3 }, { "rev_decay", 2.8f },
              { "mod1_src", SrcLfo1 }, { "mod1_dst", dst (v1::dest (Chord, 2)) }, { "mod1_amt", 0.18f }, { "lfo1_shape", 0 }, { "lfo1_rate", 1 },
              { "lane1_dst", dst (v1::dest (Chord, v1::Delay)) }, { "lane1_bars", 2 }, { "lane1_depth", 0.6f } },
            [] (PatternBank& b) {
                b.clearAll();
                row (b, 0, Kick, "x---x---x---x---");
                row (b, 0, Clap, "----x-------x---");
                row (b, 0, HatC, "..x...x...x...x.");
                row (b, 0, HatO, "--o---o---o---x-");
                row (b, 0, Perc, "---o--.-o--o----", { 0, 0, 0, 7, 0, 0, 5, 0, 0, 0, 0, 12, 0, 0, 0, 0 });
                row (b, 0, Bass, "--x--o-x--x--o--", { 0, 0, 0, 0, 0, 12, 0, 0, 0, 0, -2, 0, 0, 10, 0, 0 });
                row (b, 0, Chord, "--x-----x--x----", { 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, -2, 0, 0, 0, 0 });
                derive (b, 22);
                lane (b, 0, swell);
            } });

        // 3 ─ MEDIANOCHE MINIMA ─────────────────────────────────────────
        v.push_back ({ "Medianoche Mínima", "Minimal seco y preciso, polirritmos y clicks",
            { { "root", 5 }, { "swing", 0.52f }, { "tempo", 126 },
              { sid (Kick, 0), 0.42f }, { sid (Kick, 1), 0.32f }, { sid (Kick, 4), 0.55f }, { sid (Kick, 5), 0.3f },
              { sid (Rim, 0), 0.62f }, { sid (Rim, 1), 0.12f }, { sid (Rim, 4), 0.8f }, { tid (Rim, "pan"), 0.38f }, { tid (Rim, "ring"), 1 },
              { sid (HatC, 1), 0.15f }, { sid (HatC, 2), 0.8f }, { sid (HatC, 3), 0.65f }, { tid (HatC, "pan"), 0.62f },
              { sid (Perc, 0), 0.62f }, { sid (Perc, 1), 0.18f }, { sid (Perc, 2), 0.55f }, { sid (Perc, 3), 0.62f }, { sid (Perc, 4), 0.1f },
              { tid (Perc, "ring"), 1 }, { tid (Perc, "dly"), 0.3f }, { tid (Perc, "pan"), 0.7f },
              { sid (Bass, 1), 0.2f }, { sid (Bass, 2), 0.22f }, { sid (Bass, 3), 0.62f }, { sid (Bass, 4), 0.55f }, { sid (Bass, 6), 0.1f },
              { tid (Clap, "lvl"), 0.7f }, { sid (Clap, 1), 0.2f }, { sid (Clap, 6), 0.1f },
              { "ring_freq", 1180.0f }, { "ring_mix", 0.55f }, { "ring_wave", 1 },
              { "dly_div", 5 }, { "dly_fb", 0.45f }, { "rev_size", 0.3f }, { "bus_comp", 0.7f },
              { "lfo1_shape", 5 }, { "lfo1_rate", 7 }, { "mod1_src", SrcLfo1 }, { "mod1_dst", dst (v1::dest (Perc, 0)) }, { "mod1_amt", 0.2f },
              { "mod2_src", SrcRandStep }, { "mod2_dst", dst (v1::dest (HatC, 1)) }, { "mod2_amt", 0.25f },
              { "lfo2_shape", 1 }, { "lfo2_rate", 1 }, { "mod3_src", SrcLfo2 }, { "mod3_dst", dst (v1::RingFreqDest) }, { "mod3_amt", 0.25f },
              { "lane1_dst", dst (v1::dest (Bass, 2)) }, { "lane1_bars", 1 }, { "lane1_depth", 0.45f } },
            [] (PatternBank& b) {
                b.clearAll();
                row (b, 0, Kick, "x---x---x---x---");
                row (b, 0, Clap, "------------x---");
                row (b, 0, Rim, "---x--x-.--x-x--", {}, 13);
                row (b, 0, HatC, "x.x.xxx.x.x.xx.", {}, 15);
                row (b, 0, Perc, "x--x-.x--x-.", { 0, 0, 0, 5, 0, 7, 12, 0, 0, 3, 0, 7 }, 12);
                row (b, 0, Bass, "x-----x---s-x---", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, -2, 0, 0, 0 });
                derive (b, 33);
                lane (b, 0, steps (77, 8));
            } });

        // 4 ─ SUDOR ─────────────────────────────────────────────────────
        v.push_back ({ "Sudor", "House de pico de noche, todo empuja",
            { { "root", 7 }, { "swing", 0.54f }, { "tempo", 126 },
              { sid (Kick, 1), 0.5f }, { sid (Kick, 2), 0.6f }, { sid (Kick, 4), 0.5f }, { sid (Kick, 5), 0.55f },
              { sid (Clap, 0), 0.5f }, { sid (Clap, 1), 0.45f }, { sid (Clap, 6), 0.5f }, { tid (Clap, "rev"), 0.25f },
              { sid (HatO, 1), 0.38f }, { sid (HatO, 5), 0.45f }, { tid (HatO, "lvl"), 0.86f },
              { sid (HatC, 5), 0.5f }, { tid (HatC, "pan"), 0.58f },
              { sid (Bass, 2), 0.35f }, { sid (Bass, 3), 0.45f }, { sid (Bass, 4), 0.5f }, { sid (Bass, 5), 0.55f }, { sid (Bass, 6), 0.0f },
              { tid (Bass, "pump"), 0.55f },
              { sid (Chord, 0), stepCentre (5, 8) }, { sid (Chord, 1), 0.22f }, { sid (Chord, 2), 0.6f }, { sid (Chord, 6), 0.3f },
              { tid (Chord, "pump"), 0.6f }, { tid (Chord, "dly"), 0.18f }, { tid (Chord, "lvl"), 0.8f },
              { "bus_comp", 0.85f }, { "bus_crudo", 0.55f }, { "m_comp", 0.6f }, { "m_color", 0.65f }, { "macro4", 0.62f },
              { "pump_rel", 160.0f },
              { "lane1_dst", dst (v1::MFilterDest) }, { "lane1_bars", 3 }, { "lane1_depth", 0.3f } },
            [] (PatternBank& b) {
                b.clearAll();
                row (b, 0, Kick, "X---x---X---x---");
                row (b, 0, Clap, "----X-------X---");
                row (b, 0, HatC, "x.xox.xox.xox.xo");
                row (b, 0, HatO, "--X---X---X---X-");
                row (b, 0, Perc, "------x-----x--x", { 0, 0, 0, 0, 0, 0, 12, 0, 0, 0, 0, 0, 7, 0, 0, 12 });
                row (b, 0, Bass, "--x-x-x-x-x-x-xx", { 0, 0, 0, 0, 12, 0, 0, 0, 0, 0, 10, 0, 0, 0, 7, 10 });
                row (b, 0, Chord, "---x-----x----x-", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0 });
                derive (b, 44);
                lane (b, 0, [] (float x) { return 0.5f + 0.5f * x * x; });
            } });

        // 5 ─ TERCIOPELO ────────────────────────────────────────────────
        v.push_back ({ "Terciopelo", "Deep house aterciopelado, pads largos y sub",
            { { "root", 2 }, { "swing", 0.56f }, { "tempo", 118 },
              { sid (Kick, 0), 0.28f }, { sid (Kick, 1), 0.58f }, { sid (Kick, 4), 0.15f }, { sid (Kick, 6), 0.35f },
              { sid (HatC, 1), 0.2f }, { sid (HatC, 3), 0.62f }, { tid (HatC, "lvl"), 0.72f },
              { sid (Rim, 1), 0.3f }, { sid (Rim, 2), 0.35f }, { tid (Rim, "rev"), 0.4f }, { tid (Rim, "lvl"), 0.7f },
              { sid (Bass, 0), 0.125f }, { sid (Bass, 2), 0.18f }, { sid (Bass, 4), 0.15f }, { sid (Bass, 6), 0.85f }, { sid (Bass, 1), 0.6f },
              { sid (Chord, 0), stepCentre (2, 8) }, { sid (Chord, 1), 0.78f }, { sid (Chord, 2), 0.38f }, { sid (Chord, 4), 0.25f },
              { sid (Chord, 6), 0.85f }, { sid (Chord, 7), 0.45f }, { tid (Chord, "rev"), 0.5f }, { tid (Chord, "dly"), 0.25f },
              { tid (Chord, "pump"), 0.35f },
              { "rev_decay", 4.2f }, { "rev_size", 0.8f }, { "dly_div", 1 }, { "dly_fb", 0.5f }, { "m_texture", 0.18f }, { "m_warmth", 0.45f },
              { "macro3", 0.62f },
              { "lfo1_shape", 6 }, { "lfo1_rate", 2 }, { "mod1_src", SrcLfo1 }, { "mod1_dst", dst (v1::dest (Chord, 2)) }, { "mod1_amt", 0.22f },
              { "lane1_dst", dst (v1::dest (Chord, 5)) }, { "lane1_bars", 2 }, { "lane1_depth", 0.5f } },
            [] (PatternBank& b) {
                b.clearAll();
                row (b, 0, Kick, "x---x---x---x---");
                row (b, 0, Rim, "----o-------o--.");
                row (b, 0, HatC, ".-o-.-o-.-o-.-oo");
                row (b, 0, HatO, "------o-------o-");
                row (b, 0, Bass, "x------sx-----x-", { 0, 0, 0, 0, 0, 0, 0, -2, -2, 0, 0, 0, 0, 0, -5, 0 });
                row (b, 0, Chord, "x-------x-------", { 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, 0, 0, 0 });
                derive (b, 55);
                lane (b, 0, sine);
            } });

        // 6 ─ HIPNOSIS ──────────────────────────────────────────────────
        v.push_back ({ "Hipnosis", "Minimal hipnótico: loops desfasados que nunca se repiten igual",
            { { "root", 4 }, { "swing", 0.5f }, { "tempo", 124 },
              { sid (Kick, 1), 0.4f }, { sid (Kick, 4), 0.45f },
              { sid (Perc, 0), 0.5f }, { sid (Perc, 1), 0.22f }, { sid (Perc, 2), 0.42f }, { sid (Perc, 3), 0.85f }, { tid (Perc, "ring"), 1 },
              { tid (Perc, "dly"), 0.35f }, { tid (Perc, "pan"), 0.32f },
              { sid (HatC, 2), 0.85f }, { sid (HatC, 1), 0.18f }, { tid (HatC, "ring"), 1 }, { tid (HatC, "pan"), 0.68f },
              { sid (Rim, 0), 0.72f }, { tid (Rim, "dly"), 0.25f },
              { sid (Bass, 1), 0.18f }, { sid (Bass, 3), 0.7f }, { sid (Bass, 4), 0.35f }, { sid (Bass, 2), 0.25f },
              { "ring_freq", 640.0f }, { "ring_mix", 0.6f }, { "ring_spread", 0.6f },
              { "dly_div", 3 }, { "dly_fb", 0.62f },
              { "lfo1_shape", 1 }, { "lfo1_rate", 0 }, { "mod1_src", SrcLfo1 }, { "mod1_dst", dst (v1::RingFreqDest) }, { "mod1_amt", 0.35f },
              { "lfo2_shape", 0 }, { "lfo2_rate", 8 }, { "mod2_src", SrcLfo2 }, { "mod2_dst", dst (v1::dest (HatC, 3)) }, { "mod2_amt", 0.3f },
              { "mod3_src", SrcRandSmooth }, { "mod3_dst", dst (v1::dest (Perc, 2)) }, { "mod3_amt", 0.3f },
              { "lane1_dst", dst (v1::dest (Perc, 0)) }, { "lane1_bars", 1 }, { "lane1_depth", 0.5f },
              { "lane2_dst", dst (v1::MFilterDest) }, { "lane2_bars", 3 }, { "lane2_depth", 0.25f } },
            [] (PatternBank& b) {
                b.clearAll();
                row (b, 0, Kick, "x---x---x---x---");
                row (b, 0, HatC, "x-xx-x-xx-x-", {}, 12);
                row (b, 0, Perc, "x-r-x-.", { 0, 0, 7, 0, 3, 0, 12 }, 7);
                row (b, 0, Rim, "--x--x--x--x-", {}, 13);
                row (b, 0, Bass, "x--x--x--x--x-x-", { 0 });
                row (b, 0, Clap, "----------------");
                derive (b, 66);
                lane (b, 0, steps (5, 16));
                lane (b, 1, tri);
            } });

        // 7 ─ NOSTALGIA 92 ──────────────────────────────────────────────
        v.push_back ({ "Nostalgia 92", "House clásico de Chicago y Nueva York, stabs y bajo ligado",
            { { "root", 0 }, { "swing", 0.6f }, { "tempo", 122 },
              { sid (Kick, 1), 0.42f }, { sid (Kick, 4), 0.45f }, { sid (Kick, 5), 0.45f },
              { sid (Clap, 0), 0.4f }, { sid (Clap, 6), 0.4f },
              { sid (HatO, 1), 0.3f }, { sid (HatC, 1), 0.3f },
              { sid (Bass, 2), 0.3f }, { sid (Bass, 3), 0.75f }, { sid (Bass, 4), 0.6f }, { sid (Bass, 6), 0.0f }, { sid (Bass, 7), 0.4f },
              { sid (Chord, 0), stepCentre (3, 8) }, { sid (Chord, 1), 0.2f }, { sid (Chord, 2), 0.7f }, { sid (Chord, 4), 0.1f },
              { sid (Chord, 5), 0.15f }, { sid (Chord, 6), 0.15f }, { tid (Chord, "rev"), 0.2f }, { tid (Chord, "lvl"), 0.84f },
              { "m_color", 0.6f }, { "m_texture", 0.12f },
              { "lfo1_shape", 0 }, { "lfo1_rate", 2 }, { "mod1_src", SrcLfo1 }, { "mod1_dst", dst (v1::dest (Bass, 2)) }, { "mod1_amt", 0.2f } },
            [] (PatternBank& b) {
                b.clearAll();
                row (b, 0, Kick, "x---x---x---x---");
                row (b, 0, Clap, "----x-------x---");
                row (b, 0, HatC, "x-x-x-x-x-x-x-x-");
                row (b, 0, HatO, "--x---x---x---x-");
                row (b, 0, Bass, "x-sx-x-sx-x-s-x-", { 0, 0, 12, 0, 0, 7, 0, 10, 0, 0, 0, 0, 3, 0, 5, 0 });
                row (b, 0, Chord, "---x--x----x--x-", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 5, 0 });
                derive (b, 77);
            } });

        // 8 ─ LLUVIA ────────────────────────────────────────────────────
        v.push_back ({ "Lluvia", "Dub house melancólico, ecos largos y polvo de vinilo",
            { { "root", 11 }, { "swing", 0.56f }, { "tempo", 118 },
              { sid (Kick, 0), 0.3f }, { sid (Kick, 4), 0.15f }, { sid (Kick, 6), 0.4f },
              { sid (Rim, 1), 0.25f }, { tid (Rim, "dly"), 0.45f }, { tid (Rim, "rev"), 0.3f },
              { sid (HatC, 1), 0.22f }, { sid (HatC, 6), 0.3f }, { tid (HatC, "lvl"), 0.7f },
              { sid (Bass, 6), 0.75f }, { sid (Bass, 2), 0.2f }, { sid (Bass, 1), 0.5f },
              { sid (Chord, 0), stepCentre (0, 8) }, { sid (Chord, 1), 0.3f }, { sid (Chord, 2), 0.35f }, { sid (Chord, 3), 0.45f },
              { tid (Chord, "dly"), 0.65f }, { tid (Chord, "rev"), 0.4f }, { tid (Chord, "lvl"), 0.8f },
              { "dly_fb", 0.68f }, { "dly_tone", 0.3f }, { "dly_div", 1 }, { "rev_decay", 3.5f }, { "m_texture", 0.6f },
              { "m_filter", -0.12f }, { "macro3", 0.7f },
              { "lfo1_shape", 6 }, { "lfo1_rate", 0 }, { "mod1_src", SrcLfo1 }, { "mod1_dst", dst (v1::DlyFbDest) }, { "mod1_amt", 0.15f },
              { "lane1_dst", dst (v1::dest (Chord, 2)) }, { "lane1_bars", 2 }, { "lane1_depth", 0.55f } },
            [] (PatternBank& b) {
                b.clearAll();
                row (b, 0, Kick, "x---x---x---x---");
                row (b, 0, Rim, "----x-------x--?");
                row (b, 0, HatC, "--o---o---o---o-");
                row (b, 0, Bass, "x---------x-----", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -5 });
                row (b, 0, Chord, "--x-------x-----", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3 });
                derive (b, 88);
                lane (b, 0, swell);
            } });

        // 9 ─ OSCURO ────────────────────────────────────────────────────
        v.push_back ({ "Oscuro", "Minimal oscuro y metálico para el after",
            { { "root", 6 }, { "swing", 0.52f }, { "tempo", 125 },
              { sid (Kick, 0), 0.3f }, { sid (Kick, 1), 0.55f }, { sid (Kick, 5), 0.6f }, { sid (Kick, 6), 0.4f },
              { sid (HatC, 5), 0.7f }, { sid (HatC, 2), 0.9f }, { sid (HatO, 5), 0.6f },
              { sid (Clap, 0), 0.3f }, { sid (Clap, 1), 0.22f }, { sid (Clap, 6), 0.2f },
              { sid (Bass, 0), 0.125f }, { sid (Bass, 2), 0.2f }, { sid (Bass, 3), 0.55f }, { sid (Bass, 5), 0.75f }, { sid (Bass, 6), 0.45f },
              { sid (Chord, 0), stepCentre (7, 8) }, { sid (Chord, 1), 0.15f }, { sid (Chord, 2), 0.5f }, { tid (Chord, "ring"), 1 },
              { tid (Chord, "dly"), 0.35f }, { tid (Chord, "lvl"), 0.74f },
              { "ring_freq", 172.0f }, { "ring_wave", 2 }, { "ring_mix", 0.8f },
              { "bus_crudo", 0.6f }, { "rev_size", 0.7f }, { "dly_tone", 0.25f }, { "m_air", 0.5f }, { "macro4", 0.65f },
              { "lfo1_shape", 5 }, { "lfo1_rate", 6 }, { "mod1_src", SrcLfo1 }, { "mod1_dst", dst (v1::dest (Chord, 2)) }, { "mod1_amt", 0.3f },
              { "lane1_dst", dst (v1::RingFreqDest) }, { "lane1_bars", 1 }, { "lane1_depth", 0.4f } },
            [] (PatternBank& b) {
                b.clearAll();
                row (b, 0, Kick, "x---x---x---x---");
                row (b, 0, Clap, "--------x-------");
                row (b, 0, HatC, "x-x-xx-xx-x-xx-x");
                row (b, 0, HatO, "--x-------x-----");
                row (b, 0, Bass, "-x-x--x--x-x--x-", { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0, 0 });
                row (b, 0, Chord, "x-----x-----", { 0, 0, 0, 0, 0, 0, 1 }, 12);
                derive (b, 99);
                lane (b, 0, steps (13, 4));
            } });

        // 10 ─ EUFORIA ──────────────────────────────────────────────────
        v.push_back ({ "Euforia", "House luminoso con barridos de filtro y acordes mayores",
            { { "root", 5 }, { "swing", 0.54f }, { "tempo", 124 },
              { sid (Kick, 4), 0.45f }, { sid (Kick, 5), 0.45f },
              { sid (Clap, 6), 0.45f }, { tid (Clap, "rev"), 0.35f },
              { sid (HatO, 1), 0.42f }, { tid (HatO, "lvl"), 0.84f },
              { sid (Bass, 2), 0.38f }, { sid (Bass, 6), 0.3f }, { tid (Bass, "pump"), 0.5f },
              { sid (Chord, 0), stepCentre (4, 8) }, { sid (Chord, 1), 0.45f }, { sid (Chord, 2), 0.6f }, { sid (Chord, 6), 0.6f },
              { tid (Chord, "pump"), 0.7f }, { tid (Chord, "rev"), 0.3f }, { tid (Chord, "dly"), 0.2f }, { tid (Chord, "lvl"), 0.86f },
              { "pump_rel", 220.0f }, { "m_air", 2.5f }, { "macro1", 0.6f },
              { "lane1_dst", dst (v1::dest (Chord, 2)) }, { "lane1_bars", 3 }, { "lane1_depth", 0.7f },
              { "lfo1_shape", 0 }, { "lfo1_rate", 4 }, { "mod1_src", SrcLfo1 }, { "mod1_dst", dst (v1::dest (Chord, v1::Pan)) }, { "mod1_amt", 0.25f } },
            [] (PatternBank& b) {
                b.clearAll();
                row (b, 0, Kick, "x---x---x---x---");
                row (b, 0, Clap, "----x-------x---");
                row (b, 0, HatC, "xox.xox.xox.xoxo");
                row (b, 0, HatO, "--x---x---x---x-");
                row (b, 0, Bass, "--x---x---x---x-", { 0, 0, -12, 0, 0, 0, 0, 0, 0, 0, -5, 0, 0, 0, -3, 0 });
                row (b, 0, Chord, "x--x--x---x--x--", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -5, 0, 0, -3, 0, 0 });
                derive (b, 111);
                lane (b, 0, rampUp);
            } });

        // 11 ─ SOL DE VERANO ────────────────────────────────────────────
        v.push_back ({ "Sol de Verano", "House con congas y clave, el sello de EVO THE SUN",
            { { "root", 2 }, { "swing", 0.57f }, { "tempo", 123 },
              { sid (Kick, 1), 0.4f },
              { sid (Rim, 0), 0.68f }, { sid (Rim, 1), 0.18f }, { tid (Rim, "pan"), 0.4f },
              { sid (Perc, 0), 0.38f }, { sid (Perc, 1), 0.32f }, { sid (Perc, 2), 0.08f }, { sid (Perc, 4), 0.42f }, { sid (Perc, 6), 0.25f },
              { tid (Perc, "pan"), 0.62f }, { tid (Perc, "rev"), 0.2f },
              { sid (HatC, 0), 0.62f }, { sid (HatC, 1), 0.2f }, { sid (HatC, 2), 0.25f }, { tid (HatC, "pan"), 0.42f },
              { sid (Bass, 2), 0.32f }, { sid (Bass, 6), 0.4f },
              { sid (Chord, 0), stepCentre (3, 8) }, { sid (Chord, 1), 0.32f }, { sid (Chord, 2), 0.62f }, { sid (Chord, 6), 0.55f },
              { tid (Chord, "dly"), 0.25f }, { tid (Chord, "rev"), 0.25f }, { tid (Chord, "lvl"), 0.8f },
              { "m_air", 2.2f }, { "dly_div", 3 },
              { "mod1_src", SrcRandStep }, { "mod1_dst", dst (v1::dest (Perc, 0)) }, { "mod1_amt", 0.08f },
              { "lane1_dst", dst (v1::dest (Perc, v1::Pan)) }, { "lane1_bars", 0 }, { "lane1_depth", 0.5f } },
            [] (PatternBank& b) {
                b.clearAll();
                row (b, 0, Kick, "x---x---x---x---");
                row (b, 0, Rim, "x--x--x---x-x---");
                row (b, 0, HatC, "xoooxoooxoooxoor");
                row (b, 0, HatO, "--o---o---o---o-");
                row (b, 0, Perc, "--x-.xx---x-.xx-", { 0, 0, 0, 0, 7, 7, 0, 0, 0, 0, 12, 0, 7, 0, 7, 0 });
                row (b, 0, Bass, "x--x---x--x--x--", { 0, 0, 0, 7, 0, 0, 0, 5, 0, 0, 7, 0, 0, 10, 0, 0 });
                row (b, 0, Chord, "--x--x----x--x--", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 5, 0, 0 });
                derive (b, 121);
                lane (b, 0, sine);
            } });

        // 12 ─ UNDERGROUND ──────────────────────────────────────────────
        v.push_back ({ "Underground", "Sótano crudo: todo saturado, comprimido y sucio",
            { { "root", 10 }, { "swing", 0.55f }, { "tempo", 127 },
              { sid (Kick, 2), 0.62f }, { sid (Kick, 4), 0.6f }, { sid (Kick, 5), 0.72f }, { sid (Kick, 6), 0.5f },
              { sid (Clap, 5), 0.6f }, { sid (Clap, 6), 0.55f }, { sid (Clap, 4), 0.7f },
              { sid (HatC, 5), 0.75f }, { sid (HatO, 5), 0.7f }, { sid (HatC, 2), 0.4f },
              { sid (Bass, 5), 0.6f }, { sid (Bass, 6), 0.7f }, { sid (Bass, 2), 0.25f },
              { sid (Chord, 0), stepCentre (0, 8) }, { sid (Chord, 1), 0.18f }, { sid (Chord, 2), 0.45f }, { sid (Chord, 6), 0.2f },
              { tid (Chord, "dly"), 0.3f }, { tid (Chord, "lvl"), 0.78f },
              { "bus_comp", 0.92f }, { "bus_crudo", 0.8f }, { "m_comp", 0.7f }, { "m_color", 0.85f }, { "m_warmth", 0.55f },
              { "m_texture", 0.35f }, { "macro4", 0.7f },
              { "mod1_src", SrcKickEnv }, { "mod1_dst", dst (v1::dest (HatO, 3)) }, { "mod1_amt", 0.25f },
              { "lane1_dst", dst (v1::dest (Clap, 1)) }, { "lane1_bars", 0 }, { "lane1_depth", 0.4f } },
            [] (PatternBank& b) {
                b.clearAll();
                row (b, 0, Kick, "X---x---X---x--.");
                row (b, 0, Clap, "----X-------X---");
                row (b, 0, HatC, "xxoxxxoxxxoxxxox");
                row (b, 0, HatO, "--x---x---x---x-");
                row (b, 0, Rim, "-?----?---?---?-");
                row (b, 0, Bass, "--x--xx---x--xx-", { 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 5, 3, 0 });
                row (b, 0, Chord, "---x------x-----", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2 });
                derive (b, 131);
                lane (b, 0, steps (99, 16));
            } });

        // ════════════════════════════════════════════════════════════════
        // OGRO 2 · MINIMAL 127 (fase 7): tonalidad, acordes en tono, groove,
        // FX de fin de compás y capas de Orbhitz (si falta el sample, suena el sinte)
        // ════════════════════════════════════════════════════════════════
        auto v2preset = [&v] (std::string name, std::string mood, std::vector<std::pair<std::string, float>> vals,
                              std::function<void (PatternBank&)> build, std::string tex = {}, std::string chd = {},
                              std::vector<std::tuple<std::string, std::string, float>> mods = {}, std::vector<std::string> lanes = {}) {
            // Base común: 127 BPM, modo tonal, acordes diatónicos con voice leading, azar fijo
            std::vector<std::pair<std::string, float>> all { { "global.tempo", 127 }, { "global.tonal", 1 }, { "chord.mode", 0 },
                                                             { "chord.voicelead", 1 }, { "global.random", 0 }, { "global.polymode", 0 } };
            all.insert (all.end(), vals.begin(), vals.end());
            Preset p;
            p.name = std::move (name); p.mood = std::move (mood); p.build = std::move (build);
            p.v2 = true; p.values2 = std::move (all); p.texSample = std::move (tex); p.chordSample = std::move (chd);
            p.mods = std::move (mods); p.lanes = std::move (lanes);
            v.push_back (std::move (p));
        };

        // 13 ─ SÓTANO 127 ────────────────────────────────────────────────
        v2preset ("Sótano 127", "Minimal de madrugada: rim sincopado, groove suelto y stab de Orbhitz",
            { { "global.root", 9 }, { "global.scale", 0 }, { "global.swing", 0.56f }, { "global.groove", 3 }, { "global.grooveamt", 0.6f },
              { "kick.tune", 0.36f }, { "kick.decay", 0.42f }, { "kick.click", 0.25f }, { "clap.level", 0.7f }, { "clap.rev", 0.25f },
              { "rim.tone", 0.55f }, { "rim.decay", 0.18f }, { "rim.rev", 0.15f }, { "rim.pan", 0.62f },
              { "hatc.decay", 0.2f }, { "hatc.air", 0.5f }, { "hato.level", 0.72f }, { "perc.tone", 0.42f }, { "perc.fm", 0.35f }, { "perc.pan", 0.35f },
              { "bass.cutoff", 0.32f }, { "bass.reso", 0.45f }, { "bass.envamt", 0.4f }, { "bass.decay", 0.3f }, { "bass.sub", 0.35f },
              { "chord.type", 16 }, { "chord.decay", 0.4f }, { "chord.cutoff", 0.5f }, { "chord.dly", 0.28f }, { "chord.rev", 0.3f }, { "chord.level", 0.78f },
              { "chord.source", 2 }, { "chord.samplelevel", 0.55f },
              { "dly.div", 3 }, { "dly.fb", 0.45f }, { "rev.decay", 2.4f },
              { "bfx.every", 4 }, { "bfx.len", 2 }, { "bfx.stutter.prob", 0.6f }, { "bfx.stutter.target", 1 }, { "bfx.filter.prob", 0.3f } },
            [] (PatternBank& b) {
                b.clearAll();
                row (b, 0, Kick, "x---x---x---x---");
                row (b, 0, Clap, "----o-------o---");
                row (b, 0, Rim,  "---x--x---x--x-.");
                cond (b, 0, Rim, 13, c12);
                row (b, 0, HatC, ".-x-.-x-.-x-.-xr");
                row (b, 0, HatO, "--x---x---x---x-");
                row (b, 0, Perc, "x--x-", { 0, 0, 0, 4, 0 }, 5);
                row (b, 0, Bass, "--x--x-s--x---x-", { 0, 0, 0, 0, 0, 2, 0, 3, 0, 0, -1, 0, 0, 0, 4, 0 });
                row (b, 0, Chord, "---X------x-----", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, 0 });
                cond (b, 0, Chord, 10, c12);
                gate (b, 0, Chord, 3, 10);
                derive (b, 13);
            }, {}, "CHD-Stabbers");

        // 14 ─ LOMONTE ───────────────────────────────────────────────────
        v2preset ("Lomonte", "Micro-house rebotado: bajo 303, pluck y ghost notes",
            { { "global.root", 5 }, { "global.scale", 0 }, { "global.swing", 0.58f }, { "global.groove", 1 }, { "global.humanize", 3.0f },
              { "kick.decay", 0.38f }, { "kick.punch", 0.42f }, { "clap.decay", 0.3f }, { "clap.room", 0.4f },
              { "rim.decay", 0.12f }, { "rim.bright", 0.62f }, { "hatc.decay", 0.15f }, { "hatc.metal", 0.7f },
              { "perc.tone", 0.5f }, { "perc.fm", 0.5f }, { "perc.ratio", 0.55f }, { "perc.ring", 0.4f },
              { "bass.accent", 1 }, { "bass.cutoff", 0.22f }, { "bass.reso", 0.68f }, { "bass.envamt", 0.62f }, { "bass.decay", 0.25f }, { "bass.glide", 0.3f },
              { "melody.model", 0 }, { "melody.decay", 0.35f }, { "melody.cutoff", 0.6f }, { "melody.timbre", 0.3f }, { "melody.dly", 0.3f }, { "melody.level", 0.74f },
              { "ring.freq", 330 }, { "ring.mix", 0.5f },
              { "bfx.every", 3 }, { "bfx.len", 1 }, { "bfx.crush.prob", 0.25f }, { "bfx.crush.target", 1 }, { "bfx.tapestop.prob", 0.15f } },
            [] (PatternBank& b) {
                b.clearAll();
                row (b, 0, Kick, "x---x---x---x-o-");
                cond (b, 0, Kick, 14, c12);
                row (b, 0, Clap, "----x--------x--");
                row (b, 0, Rim,  "x-.x-x..-x.x--x.");
                row (b, 0, HatC, "x.xxx.xxx.xxx.xR");
                row (b, 0, HatO, "--x-------x-----");
                row (b, 0, Perc, "x-x--x-", { 0, 2, 0, 0, 4, 0, 0 }, 7);
                row (b, 0, Bass, "X-xs-x--X-xs-x--", { 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 4, 3, 0, 0, 0, 0 });
                tweak (b, 0, Bass, 8, [] (Step& st) { st.octave = 1; });
                row (b, 0, Melody, "x--x--x-----x---", { 0, 0, 0, 2, 0, 0, 4, 0, 0, 0, 0, 0, 3, 0, 0, 0 });
                cond (b, 0, Melody, 12, c12);
                derive (b, 14);
            });

        // 15 ─ TIZA ──────────────────────────────────────────────────────
        v2preset ("Tiza", "Dub minimal: acordes que se van en delay y reverb al final de cada 4",
            { { "global.root", 2 }, { "global.scale", 1 }, { "global.swing", 0.55f },
              { "kick.tune", 0.33f }, { "kick.decay", 0.5f }, { "rim.rev", 0.3f },
              { "hatc.decay", 0.22f }, { "hatc.level", 0.7f }, { "hato.decay", 0.3f },
              { "bass.cutoff", 0.25f }, { "bass.sub", 0.6f }, { "bass.decay", 0.5f }, { "bass.sustain", 0.6f }, { "bass.release", 200 },
              { "chord.type", 1 }, { "chord.decay", 0.6f }, { "chord.cutoff", 0.4f }, { "chord.envamt", 0.3f }, { "chord.chorus", 0.7f },
              { "chord.sustain", 0.5f }, { "chord.release", 700 }, { "chord.strum", 14 }, { "chord.spread", 0.5f },
              { "chord.dly", 0.45f }, { "chord.rev", 0.4f }, { "chord.pump", 0.5f },
              { "dly.div", 1 }, { "dly.fb", 0.55f }, { "dly.tone", 0.35f }, { "rev.size", 0.75f }, { "rev.decay", 4.0f },
              { "bfx.every", 3 }, { "bfx.len", 3 }, { "bfx.delay.prob", 0.6f }, { "bfx.delay.target", 2 }, { "bfx.reverb.prob", 0.4f }, { "bfx.reverb.target", 2 } },
            [] (PatternBank& b) {
                b.clearAll();
                row (b, 0, Kick, "x---x---x---x---");
                row (b, 0, Rim,  "----x-------x---");
                row (b, 0, HatC, "--x---x---x---x-");
                row (b, 0, HatO, "--------------x-");
                row (b, 0, Bass, "x--x------x-----", { 0, 0, 0, -2, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0 });
                row (b, 0, Chord, "--x-------x-----", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, 0 });
                gate (b, 0, Chord, 2, 24); gate (b, 0, Chord, 10, 16);
                derive (b, 15);
            });

        // 16 ─ NEÓN ──────────────────────────────────────────────────────
        v2preset ("Neón", "Detroit: stabs en tono, bajo de octavas y campana",
            { { "global.root", 0 }, { "global.scale", 0 }, { "global.swing", 0.54f },
              { "kick.punch", 0.55f }, { "clap.level", 0.82f }, { "hatc.decay", 0.18f }, { "hatc.air", 0.55f }, { "hato.level", 0.7f },
              { "bass.osc", 2 }, { "bass.wave", 0.4f }, { "bass.cutoff", 0.38f }, { "bass.envamt", 0.5f }, { "bass.decay", 0.28f },
              { "chord.type", 0 }, { "chord.decay", 0.32f }, { "chord.cutoff", 0.55f }, { "chord.envamt", 0.55f }, { "chord.strum", 8 },
              { "chord.dly", 0.25f }, { "chord.rev", 0.2f },
              { "melody.model", 2 }, { "melody.decay", 0.55f }, { "melody.timbre", 0.42f }, { "melody.envamt", 0.5f }, { "melody.rev", 0.35f }, { "melody.level", 0.7f },
              { "bfx.every", 3 }, { "bfx.len", 2 }, { "bfx.filter.prob", 0.5f }, { "bfx.riser.prob", 0.3f } },
            [] (PatternBank& b) {
                b.clearAll();
                row (b, 0, Kick, "x---x---x---x---");
                row (b, 0, Clap, "----x-------x---");
                row (b, 0, HatC, "xxXxxxXxxxXxxxXx");
                row (b, 0, HatO, "--x---x---x---x-");
                row (b, 0, Bass, "x-xx-x-xx-x-x-xx", { 0, 0, 7, 0, 0, 0, 0, 7, 0, 0, 3, 0, 4, 0, 7, 0 });
                row (b, 0, Chord, "---x--x----x--x-", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, 3, 0 });
                row (b, 0, Melody, "x-----x---x-----", { 7, 0, 0, 0, 0, 0, 4, 0, 0, 0, 2, 0, 0, 0, 0, 0 });
                derive (b, 16);
            });

        // 17 ─ POLVO ─────────────────────────────────────────────────────
        v2preset ("Polvo", "Deep lento: pad de Orbhitz, sub largo y todo que se mueve despacio",
            { { "global.root", 7 }, { "global.scale", 0 }, { "global.swing", 0.57f }, { "global.drift", 0.35f }, { "global.driftbars", 1 },
              { "kick.decay", 0.48f }, { "clap.rev", 0.4f }, { "hatc.decay", 0.28f }, { "hatc.level", 0.68f },
              { "bass.sub", 0.7f }, { "bass.cutoff", 0.2f }, { "bass.sustain", 0.7f }, { "bass.release", 300 }, { "bass.decay", 0.6f },
              { "chord.type", 2 }, { "chord.level", 0.7f }, { "chord.sustain", 0.6f }, { "chord.release", 1200 }, { "chord.rev", 0.45f }, { "chord.attack", 0.4f },
              { "texture.level", 0.66f }, { "texture.sustain", 1.0f }, { "texture.release", 1500 }, { "texture.rev", 0.4f }, { "texture.follow", 1 },
              { "rev.size", 0.8f }, { "rev.decay", 5.0f }, { "master.texture", 0.25f },
              { "bfx.every", 4 }, { "bfx.len", 4 }, { "bfx.reverb.prob", 0.5f } },
            [] (PatternBank& b) {
                b.clearAll();
                row (b, 0, Kick, "x---x---x---x---");
                row (b, 0, Clap, "----x-------x---");
                row (b, 0, HatC, "--x---x---x---x.");
                row (b, 0, Bass, "x-------x-x-----", { 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, 0, 0, 0 });
                gate (b, 0, Bass, 0, 40);
                row (b, 0, Chord, "x---------------", { 0 });
                gate (b, 0, Chord, 0, 63);
                row (b, 0, Texture, "x---------------", { 0 });
                gate (b, 0, Texture, 0, 63);
                cond (b, 0, Texture, 0, c12);
                derive (b, 17);
            }, "PAD-SoGentle");

        // 18 ─ GARAJE 127 ────────────────────────────────────────────────
        v2preset ("Garaje 127", "2-step: bombo partido, shuffle 909 y órgano",
            { { "global.root", 4 }, { "global.scale", 1 }, { "global.swing", 0.62f }, { "global.groove", 2 },
              { "kick.punch", 0.5f }, { "clap.bursts", 0.6f }, { "clap.level", 0.82f }, { "hatc.decay", 0.16f },
              { "bass.sub", 0.6f }, { "bass.cutoff", 0.3f }, { "bass.osc", 4 }, { "bass.wave", 0.3f },
              { "melody.model", 3 }, { "melody.timbre", 0.55f }, { "melody.voices", 0 }, { "melody.decay", 0.4f }, { "melody.level", 0.7f }, { "melody.rev", 0.2f },
              { "bfx.every", 3 }, { "bfx.len", 1 }, { "bfx.stutter.prob", 0.4f }, { "bfx.stutter.rate", 1 } },
            [] (PatternBank& b) {
                b.clearAll();
                row (b, 0, Kick, "x---------x-----");
                row (b, 0, Clap, "----x-------x---");
                row (b, 0, HatC, "x-xxx-xrx-xxx-xr");
                row (b, 0, Rim,  "---x-----x----x-");
                row (b, 0, Bass, "x--x---x--x--x--", { 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 4, 0, 0, 2, 0, 0 });
                row (b, 0, Melody, "--x---x---x---x-", { 0, 0, 2, 0, 0, 0, 4, 0, 0, 0, 2, 0, 0, 0, 0, 0 });
                derive (b, 18);
            });

        // 19 ─ RAYO ──────────────────────────────────────────────────────
        v2preset ("Rayo", "Tool percusivo: euclidianos, ring y tape stop cada 8",
            { { "global.root", 9 }, { "global.scale", 2 }, { "global.swing", 0.53f },
              { "rim.tone", 0.6f }, { "rim.ring", 0.3f }, { "perc.tone", 0.55f }, { "perc.fm", 0.6f }, { "perc.ring", 0.7f }, { "perc.dly", 0.3f },
              { "ring.freq", 520 }, { "ring.mix", 0.6f }, { "ring.wave", 1 },
              { "bass.cutoff", 0.28f }, { "bass.envamt", 0.55f }, { "bass.accent", 1 },
              { "bfx.every", 4 }, { "bfx.len", 2 }, { "bfx.tapestop.prob", 0.5f }, { "bfx.riser.prob", 0.4f } },
            [] (PatternBank& b) {
                b.clearAll();
                row (b, 0, Kick, "x---x---x---x---");
                euclidPattern (b, 0, Rim, 5, 16, 2);
                euclidPattern (b, 0, Perc, 3, 8, 1);
                row (b, 0, HatC, "--x---x---x---x-");
                row (b, 0, Bass, "--x---x--x--x-x-", { 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 4, 0 });
                derive (b, 19);
            });

        // 20 ─ MADRUGADA ─────────────────────────────────────────────────
        v2preset ("Madrugada", "Hipnótico: cadena A A A B, fill cada 8 y el filtro de los acordes respirando",
            { { "global.root", 11 }, { "global.scale", 0 }, { "global.swing", 0.55f }, { "global.chain", 1 },
              { "global.fill", 3 }, { "global.fillpat", 4 },
              { "hatc.decay", 0.22f }, { "rim.decay", 0.15f },
              { "bass.cutoff", 0.3f }, { "bass.reso", 0.5f },
              { "chord.type", 17 }, { "chord.cutoff", 0.35f }, { "chord.envamt", 0.3f }, { "chord.dly", 0.3f }, { "chord.rev", 0.25f },
              { "melody.model", 1 }, { "melody.cutoff", 0.4f }, { "melody.vibrato", 0.3f }, { "melody.glide", 0.45f }, { "melody.voices", 2 },
              { "melody.level", 0.68f }, { "melody.dly", 0.35f },
              { "motion1.bars", 4 }, { "motion1.depth", 0.7f },
              { "bfx.every", 4 }, { "bfx.len", 2 }, { "bfx.filter.prob", 0.6f } },
            [] (PatternBank& b) {
                b.clearAll();
                row (b, 0, Kick, "x---x---x---x---");
                row (b, 0, Rim,  "--x---x-.-x---x-");
                row (b, 0, HatC, "x-x-x-x-x-x-x-x.");
                row (b, 0, HatO, "--x---x---x---x-");
                row (b, 0, Bass, "--x--x----x--x--", { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0 });
                row (b, 0, Chord, "x-------x-------", { 0, 0, 0, 0, 0, 0, 0, 0, -2, 0, 0, 0, 0, 0, 0, 0 });
                gate (b, 0, Chord, 0, 24); gate (b, 0, Chord, 8, 24);
                row (b, 0, Melody, "x---------s-----", { 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0 });
                gate (b, 0, Melody, 0, 40);
                derive (b, 20);
                b.setChain ({ { 0, 3 }, { 1, 1 } });
                lane (b, 0, sine);
            }, {}, {}, {}, { "chord.cutoff" });

        return v;
    }();
    return list;
}

// Estado de un preset de fábrica en OGRO 2. Los de fábrica usan azar FIJO (renders
// repetibles); con fixedRandom = false quedan exactamente como en la 1.0.
// Para usar en OGRO 2 (fixedRandom = true) además pasan a macros x2 con el mismo efecto
// (valores convertidos: la diferencia de redondeo es inaudible).
inline v1::State factoryState (const Preset& p, bool fixedRandom = true)
{
    if (p.v2)
    {
        // Presets de la 2.0: valores por defecto de OGRO 2 + los del preset, por ID
        const auto& defs = paramDefs();
        v1::State st;
        st.values.resize (defs.size());
        for (size_t i = 0; i < defs.size(); ++i) st.values[i] = defs[i].def;
        for (auto& d : st.dst) d = -1;
        for (auto& d : st.laneDst) d = -1;
        for (const auto& [id, v] : p.values2)
            if (const int i = paramIndex (id); i >= 0) st.values[(size_t) i] = v;
        for (size_t k = 0; k < p.mods.size() && k < (size_t) P::kSlots; ++k)
        {
            st.src[k] = modSourceIndex (std::get<0> (p.mods[k]));
            st.dst[k] = paramIndex (std::get<1> (p.mods[k]));
            st.values[(size_t) P::mod ((int) k, P::ModAmt)] = std::get<2> (p.mods[k]);
        }
        for (size_t l = 0; l < p.lanes.size() && l < (size_t) P::kMotion; ++l) st.laneDst[l] = p.lanes[l].empty() ? -1 : paramIndex (p.lanes[l]);
        return st;
    }
    auto st = v1::convert (p.values);
    if (fixedRandom)
    {
        st.values[P::RandomMode] = 0.0f;
        st.values[P::MacroDepth] = 1.0f;
        for (int m = 0; m < 4; ++m)
        {
            auto& v = st.values[(size_t) (P::Macro1 + m)];
            v = (float) (0.5 + ((double) v - 0.5) * 0.5);
        }
    }
    return st;
}

inline void applyRouting (ModRouting& r, const v1::State& st)
{
    for (int s = 0; s < P::kSlots; ++s) { r.src[s].store (st.src[s]); r.dst[s].store (st.dst[s]); }
    for (int l = 0; l < P::kMotion; ++l) r.laneDst[l].store (st.laneDst[l]);
}

} // namespace ogro2
