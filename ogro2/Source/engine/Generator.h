// OGRO 2 — generador de patrones: INVENTAR (con sentido musical según el género),
// MUTAR (cambia un porcentaje de lo que hay sin perder el esqueleto) y ritmos euclidianos.
#pragma once

#include "Sequencer.h"

namespace ogro2
{
enum Genre { GenreMinimal = 0, GenreMicro, GenreDeep, GenreDetroit, GenreGarage };

namespace gen
{
struct Rng
{
    uint32_t r;
    explicit Rng (uint32_t seed) : r (seed ? seed : 1u) { for (int i = 0; i < 3; ++i) next(); }
    uint32_t next() { r ^= r << 13; r ^= r >> 17; r ^= r << 5; return r; }
    bool chance (int pct) { return (int) (next() % 100u) < pct; }
    int range (int lo, int hi) { return lo + (int) (next() % (uint32_t) (hi - lo + 1)); }
    template <size_t N> int pick (const int (&v)[N]) { return v[next() % N]; }
};

// Pentatónica menor + 9na: suena bien sobre cualquier acorde menor de deep / minimal.
// Con MODO TONAL devuelve grados de la escala (tónica, 3ra, 4ta, 5ta, 7ma, 8va, 9na, 10ma).
inline int scaleNote (Rng& r, int maxDegree = 8, bool tonal = false)
{
    static const int scale[] = { 0, 3, 5, 7, 10, 12, 14, 15 };
    static const int degrees[] = { 0, 2, 3, 4, 6, 7, 8, 9 };
    const uint32_t i = r.next() % (uint32_t) clampv (maxDegree, 1, 8);
    return tonal ? degrees[i] : scale[i];
}

inline Step make (bool on, int vel, int prob = 100)
{
    Step s;
    s.on = on; s.vel = clampv (vel, 1, 127); s.prob = prob;
    return s;
}
} // namespace gen

// Un patrón para una pista. Respeta el largo de la pista (salvo PERC, que puede ir en polirritmo).
inline void inventPattern (PatternBank& bank, int pattern, int track, uint32_t seed, int genre = GenreMinimal, bool tonal = false)
{
    using namespace gen;
    const int oct = tonal ? 7 : 12;   // una octava en grados o en semitonos
    Rng r (seed * 2654435761u + (uint32_t) track * 97u + (uint32_t) genre * 7919u);
    int len = bank.getLength (pattern, track);
    if (len < 4) len = 16;
    const bool micro = genre == GenreMicro, deep = genre == GenreDeep, detroit = genre == GenreDetroit, garage = genre == GenreGarage;

    // Polirritmos de la percusión: el gesto de la minimal
    if (track == Perc && (genre == GenreMinimal || micro) && r.chance (65))
    {
        static const int lens[] = { 3, 5, 6, 7, 9, 10, 12 };
        len = r.pick (lens);
    }
    bank.setLength (pattern, track, len);

    for (int s = 0; s < kSteps; ++s)
    {
        Step st;
        const int q = s % 16;
        const bool beat = q % 4 == 0, off = q % 4 == 2, odd = (q & 1) != 0;
        switch (track)
        {
            case Kick:
                if (garage)
                {
                    st = make (q == 0 || q == 10 || (q == 7 && r.chance (40)) || (q == 13 && r.chance (25)), q == 0 ? 120 : 100);
                }
                else
                {
                    st = make (beat || (q == 14 && r.chance (micro ? 30 : 15)) || (q == 7 && r.chance (micro ? 22 : 8)), q == 0 ? 120 : (beat ? 106 : 70));
                    if (micro && q == 12 && r.chance (15)) st.on = false;   // el bombo que falta
                }
                break;
            case Clap:
                if (genre == GenreMinimal)
                    st = make ((q == 4 || q == 12) && r.chance (70), 100);
                else if (micro)
                {
                    st = make (q == 4 || (q == 13 && r.chance (45)) || (q == 11 && r.chance (20)), q == 4 ? 104 : 70);
                    if (q == 12 && r.chance (35)) { st = make (true, 96); st.ratchet = 2; }
                }
                else
                    st = make (q == 4 || q == 12 || (garage && q == 15 && r.chance (30)), q == 4 || q == 12 ? 106 : 64);
                break;
            case Rim:
            {
                const int pct = micro ? 45 : (deep ? 12 : 28);
                st = make ((odd && r.chance (pct)) || (q == 3 && r.chance (60)) || (q == 10 && r.chance (50)) || (q == 6 && r.chance (30)),
                           r.range (50, 108), r.chance (35) ? r.range (55, 85) : 100);
                if (micro && st.on) st.nudge = r.range (-12, 12);
                if (st.on && r.chance (12)) st.cond = CondRatioBase;   // 1:2
                break;
            }
            case HatC:
                if (detroit || garage)
                {
                    st = make (true, off ? 104 : (odd ? r.range (52, 78) : 74), odd && r.chance (40) ? 80 : 100);
                    if (garage && q == 15 && r.chance (45)) st.ratchet = 2;
                }
                else if (deep)
                    st = make (off || (odd && r.chance (30)), off ? 98 : r.range (48, 70));
                else
                {
                    st = make (! beat || r.chance (25), off ? 100 : (odd ? r.range (50, 84) : 66), (micro && odd) ? 75 : 100);
                    if (q == 15 && r.chance (micro ? 40 : 25)) st.ratchet = micro ? 3 : 2;
                }
                break;
            case HatO:
                st = make (off && r.chance (deep ? 95 : 82), 90);
                if (micro && st.on && r.chance (25)) st.cond = CondRatioBase + 0;
                break;
            case Perc:
            {
                const int pct = q % 3 == 0 ? 48 : 20;
                st = make (r.chance (deep ? pct - 8 : pct), r.range (60, 118));
                st.note = scaleNote (r, 8, tonal) - (r.chance (30) ? oct : 0);
                if (micro && st.on && r.chance (20)) st.cond = r.chance (50) ? CondRatioBase + 6 : CondRatioBase + 0;
                if (st.on && r.chance (15)) st.prob = 70;
                break;
            }
            case Bass:
            {
                if (deep)
                {
                    st = make (q == 0 ? r.chance (60) : (q == 3 || q == 8 || q == 11) ? r.chance (70) : r.chance (6), 100);
                    st.gate = st.on ? r.range (10, 20) : 0;
                }
                else if (detroit)
                {
                    st = make (odd ? r.chance (35) : (! beat && r.chance (75)), r.chance (25) ? 122 : 98);
                    if (st.on && r.chance (30)) st.octave = 1;
                }
                else if (garage)
                {
                    st = make ((q == 0 || q == 3 || q == 7 || q == 10 || q == 14) && r.chance (70), 104);
                    st.gate = st.on ? r.range (6, 14) : 0;
                }
                else
                {
                    st = make ((! beat && r.chance (micro ? 50 : 55)) || (q == 0 && r.chance (30)), r.chance (25) ? 122 : 96);
                    if (micro && st.on) { st.gate = r.range (3, 6); if (r.chance (20)) st.octave = 1; }
                }
                st.note = r.chance (55) ? 0 : (r.chance (50) ? scaleNote (r, 5, tonal) : (r.chance (50) ? oct : (tonal ? -1 : -2)));
                st.slide = st.on && r.chance (deep ? 25 : 15);
                break;
            }
            case Chord:
            {
                if (deep)
                {
                    st = make ((q == 0 || q == 10) ? r.chance (70) : (q == 2 || q == 6) ? r.chance (25) : false, r.range (90, 112));
                    st.gate = st.on ? r.range (16, 40) : 0;
                }
                else if (detroit)
                    st = make ((q == 3 || q == 6 || q == 11 || q == 14) && r.chance (60), r.range (96, 120));
                else
                {
                    st = make ((q == 2 || q == 6 || q == 10 || q == 13) ? r.chance (micro ? 35 : 50) : r.chance (6), r.range (92, 120));
                    if (st.on && r.chance (micro ? 35 : 15)) st.cond = r.chance (50) ? CondRatioBase + 0 : CondRatioBase + 5;
                }
                // Con MODO TONAL: i, VI, iv, III (los acordes salen en tono con el modo DIATÓNICO)
                st.note = r.chance (70) ? 0 : (tonal ? (r.chance (50) ? -2 : (r.chance (50) ? 3 : 2)) : (r.chance (50) ? -2 : (r.chance (50) ? 5 : 3)));
                break;
            }
            case Melody:
            {
                // Motivo corto que se repite en la segunda mitad con una variación
                const int half = len >= 8 ? len / 2 : len;
                const int q2 = s % half;
                const bool rhythm = (q2 == 0 && r.chance (70)) || (q2 == 3 && r.chance (55)) || (q2 == 6 && r.chance (45))
                                    || (q2 % 2 == 1 && r.chance (deep ? 12 : 18));
                st = make (rhythm, r.range (80, 115));
                if (deep) st.gate = st.on ? r.range (8, 24) : 0;
                st.note = scaleNote (r, 6, tonal);
                if (s >= half && s - half < kSteps)
                {
                    // segunda mitad: misma figura, a veces otra nota
                    const Step first = bank.get (pattern, track, s - half);
                    if (first.on && r.chance (75)) { st = first; if (r.chance (35)) st.note = scaleNote (r, 6, tonal); }
                }
                st.slide = st.on && r.chance (10);
                break;
            }
            case Texture:
                st = make ((q == 0 && r.chance (deep ? 60 : 35)) || (q == 8 && r.chance (20)), r.range (70, 105));
                st.prob = st.on && r.chance (40) ? 60 : 100;
                if (st.on && r.chance (30)) st.cond = CondRatioBase + 6;   // 2:4
                break;
            default: break;
        }
        if (s >= len) st = Step();
        bank.set (pattern, track, s, st);
        bank.clearLocks (pattern, track, s);
    }
    // Lo esencial siempre aparece: bombo, bajo y acordes arrancan con algo
    bool any = false;
    for (int s = 0; s < len; ++s) any |= bank.get (pattern, track, s).on;
    if (! any && (track == Kick || track == Bass || track == Chord || track == HatC || track == Melody))
        bank.set (pattern, track, track == Bass ? 2 : 0, make (true, 100));
}

inline void inventAll (PatternBank& bank, int pattern, uint32_t seed, int genre, bool tonal = false)
{
    for (int t = 0; t < kTracks; ++t) inventPattern (bank, pattern, t, seed + (uint32_t) t * 131u, genre, tonal);
}

// MUTAR: cambia más o menos cosas según "amount" (0..1). Los tiempos del bombo no se tocan.
inline void mutatePattern (PatternBank& bank, int pattern, int track, float amount, uint32_t seed, bool tonal = false)
{
    gen::Rng r (seed * 40503u + (uint32_t) track * 31u + 17u);
    const int a = (int) std::lround (clampv (amount, 0.0f, 1.0f) * 100.0f);
    if (a <= 0) return;
    const int len = bank.getLength (pattern, track);
    const bool melodic = track == Bass || track == Chord || track == Perc || track == Melody;
    const int oct = tonal ? 7 : 12;
    for (int s = 0; s < len; ++s)
    {
        Step st = bank.get (pattern, track, s);
        const bool anchor = track == Kick && s % 4 == 0;
        if (! anchor && r.chance (a * 30 / 100))
        {
            st.on = ! st.on;
            if (st.on && st.vel < 30) st.vel = 90;
        }
        if (st.on && r.chance (a * 45 / 100)) st.vel = clampv (st.vel + r.range (-22, 22), 30, 127);
        if (st.on && melodic && r.chance (a * 30 / 100))
            st.note = track == Chord ? (r.chance (60) ? 0 : (r.chance (50) ? -2 : (tonal ? 3 : 5))) : gen::scaleNote (r, 8, tonal) - (r.chance (25) ? oct : 0);
        if (st.on && r.chance (a * 10 / 100)) st.ratchet = st.ratchet > 1 ? 1 : 2;
        if (st.on && ! anchor && r.chance (a * 12 / 100)) st.prob = st.prob < 100 ? 100 : 70;
        if (st.on && melodic && track != Perc && r.chance (a * 12 / 100)) st.slide = ! st.slide;
        bank.set (pattern, track, s, st);
    }
}

// Euclidiano: "pulses" golpes repartidos lo más parejo posible en "steps" pasos, rotado.
// Los pasos que ya tenían datos (velocidad, nota) los conservan.
inline void euclidPattern (PatternBank& bank, int pattern, int track, int pulses, int steps, int rotation)
{
    steps = clampv (steps, 1, kSteps);
    pulses = clampv (pulses, 0, steps);
    bank.setLength (pattern, track, steps);
    for (int i = 0; i < steps; ++i)
    {
        const int src = ((i - rotation) % steps + steps) % steps;
        const bool on = pulses > 0 && ((src * pulses) % steps) < pulses;
        Step st = bank.get (pattern, track, i);
        if (on && ! st.on) { st.vel = src == 0 ? 112 : 96; }
        st.on = on;
        bank.set (pattern, track, i, st);
    }
}

// Patrón euclidiano como texto ("x..x..x.") para mostrarlo
inline std::string euclidPreview (int pulses, int steps, int rotation)
{
    std::string s;
    steps = clampv (steps, 1, kSteps);
    pulses = clampv (pulses, 0, steps);
    for (int i = 0; i < steps; ++i)
    {
        const int src = ((i - rotation) % steps + steps) % steps;
        s += pulses > 0 && ((src * pulses) % steps) < pulses ? 'x' : '.';
    }
    return s;
}

} // namespace ogro2
