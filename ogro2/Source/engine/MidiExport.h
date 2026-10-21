// OGRO 2 — exporta un patrón como archivo MIDI estándar (para arrastrarlo a Ableton como clip).
// Cada pista es su nota de siempre: C1 (36) KICK ... G1 (43) ACORDES, G#1 MELODÍA, A1 TEXTURAS.
// Incluye velocidad (acentos), ratchets, nudge, gate y swing; los polirritmos se repiten hasta
// completar el largo del patrón. Las condiciones y la probabilidad no se exportan (todo suena).
#pragma once

#include "Sequencer.h"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace ogro2
{
struct MidiNote { int note = 36, start = 0, length = 1, vel = 100; };

namespace midi
{
inline void varLen (std::vector<uint8_t>& d, uint32_t v)
{
    uint8_t tmp[5];
    int n = 0;
    tmp[n++] = (uint8_t) (v & 0x7f);
    while ((v >>= 7) > 0) tmp[n++] = (uint8_t) ((v & 0x7f) | 0x80);
    while (n > 0) d.push_back (tmp[--n]);
}
inline void be32 (std::vector<uint8_t>& d, uint32_t v) { for (int s = 24; s >= 0; s -= 8) d.push_back ((uint8_t) (v >> s)); }
inline void be16 (std::vector<uint8_t>& d, uint32_t v) { d.push_back ((uint8_t) (v >> 8)); d.push_back ((uint8_t) v); }
} // namespace midi

// Notas del patrón (track = -1: todas las pistas). swing 0.5 = recto. tpq: ticks por negra.
inline std::vector<MidiNote> patternNotes (const PatternBank& bank, int pattern, int track, float swing, int tpq = 96)
{
    std::vector<MidiNote> out;
    const int stepTicks = tpq / 4;
    const int patLen = std::max (1, bank.getPatternLength (pattern));
    for (int t = 0; t < kTracks; ++t)
    {
        if (track >= 0 && t != track) continue;
        const int len = std::max (1, bank.getLength (pattern, t));
        for (int k = 0; k < patLen; ++k)
        {
            const Step st = bank.get (pattern, t, k % len);
            if (! st.on) continue;
            const bool odd = (k & 1) != 0;
            const double sw = odd ? (clampv (swing, 0.5f, 0.75f) - 0.5f) * 2.0f * stepTicks : 0.0;
            const double dur = odd ? stepTicks * (1.0 - (swing - 0.5) * 2.0) : stepTicks * (1.0 + (swing - 0.5) * 2.0);
            const double start = k * stepTicks + sw + st.nudge / 100.0 * stepTicks;
            for (int r = 0; r < st.ratchet; ++r)
            {
                MidiNote n;
                n.note = 36 + t;
                n.start = std::max (0, (int) std::lround (start + dur * r / st.ratchet));
                const double sub = dur / st.ratchet;
                n.length = std::max (1, (int) std::lround (st.gate > 0 && r == st.ratchet - 1 ? stepTicks * st.gate / 8.0 : sub - 2.0));
                n.vel = clampv ((int) std::lround (st.vel * (r > 0 ? 0.82 : 1.0)), 1, 127);
                out.push_back (n);
            }
        }
    }
    std::sort (out.begin(), out.end(), [] (const MidiNote& a, const MidiNote& b) { return a.start != b.start ? a.start < b.start : a.note < b.note; });
    return out;
}

inline std::vector<uint8_t> patternToMidi (const PatternBank& bank, int pattern, int track, float swing, int tpq = 96)
{
    using namespace midi;
    const auto notes = patternNotes (bank, pattern, track, swing, tpq);
    struct Ev { int tick; bool on; int note, vel; };
    std::vector<Ev> ev;
    for (const auto& n : notes) { ev.push_back ({ n.start, true, n.note, n.vel }); ev.push_back ({ n.start + n.length, false, n.note, 0 }); }
    std::stable_sort (ev.begin(), ev.end(), [] (const Ev& a, const Ev& b) { return a.tick != b.tick ? a.tick < b.tick : (! a.on && b.on); });

    std::vector<uint8_t> trk;
    // Nombre del clip y compás 4/4
    const std::string name = std::string ("OGRO 2 - Patron ") + (char) ('A' + clampv (pattern, 0, kPatterns - 1));
    varLen (trk, 0); trk.push_back (0xFF); trk.push_back (0x03); varLen (trk, (uint32_t) name.size()); trk.insert (trk.end(), name.begin(), name.end());
    varLen (trk, 0); trk.push_back (0xFF); trk.push_back (0x58); trk.push_back (4); trk.push_back (4); trk.push_back (2); trk.push_back (24); trk.push_back (8);
    int last = 0;
    for (const auto& e : ev)
    {
        varLen (trk, (uint32_t) (e.tick - last));
        last = e.tick;
        trk.push_back (e.on ? 0x90 : 0x80);
        trk.push_back ((uint8_t) e.note);
        trk.push_back ((uint8_t) (e.on ? e.vel : 64));
    }
    // Fin de pista al final del patrón (así el clip dura el patrón entero)
    const int endTick = std::max (last, bank.getPatternLength (pattern) * tpq / 4);
    varLen (trk, (uint32_t) (endTick - last));
    trk.push_back (0xFF); trk.push_back (0x2F); trk.push_back (0x00);

    std::vector<uint8_t> d;
    d.insert (d.end(), { 'M', 'T', 'h', 'd' });
    be32 (d, 6); be16 (d, 0); be16 (d, 1); be16 (d, (uint32_t) tpq);
    d.insert (d.end(), { 'M', 'T', 'r', 'k' });
    be32 (d, (uint32_t) trk.size());
    d.insert (d.end(), trk.begin(), trk.end());
    return d;
}

// Lector mínimo (para los tests): formato 0 sin running status, como lo escribe patternToMidi
inline std::vector<MidiNote> parseMidiNotes (const std::vector<uint8_t>& d)
{
    std::vector<MidiNote> out;
    if (d.size() < 22) return out;
    size_t p = 22;
    int tick = 0;
    std::vector<std::pair<int, MidiNote>> open;
    while (p < d.size())
    {
        uint32_t delta = 0;
        while (p < d.size()) { const uint8_t b = d[p++]; delta = (delta << 7) | (b & 0x7f); if (! (b & 0x80)) break; }
        tick += (int) delta;
        if (p >= d.size()) break;
        const uint8_t st = d[p++];
        if (st == 0xFF)
        {
            const uint8_t type = d[p++];
            uint32_t len = 0;
            while (p < d.size()) { const uint8_t b = d[p++]; len = (len << 7) | (b & 0x7f); if (! (b & 0x80)) break; }
            p += len;
            if (type == 0x2F) break;
            continue;
        }
        const int note = d[p++], vel = d[p++];
        if ((st & 0xF0) == 0x90 && vel > 0) { MidiNote n; n.note = note; n.start = tick; n.vel = vel; open.push_back ({ note, n }); }
        else
            for (size_t i = 0; i < open.size(); ++i)
                if (open[i].first == note)
                {
                    auto n = open[i].second;
                    n.length = tick - n.start;
                    out.push_back (n);
                    open.erase (open.begin() + (long) i);
                    break;
                }
    }
    return out;
}

} // namespace ogro2
