// OGRO — secuenciador de 16 pasos x 8 pistas x 8 patrones, carriles de
// automatización dibujables y reloj sincronizado al host.
// Todos los datos son atómicos: la interfaz edita mientras el audio lee.
#pragma once

#include "VoiceMaps.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>

namespace ogro
{
constexpr int kSteps = 16;
constexpr int kPatterns = 8;
constexpr int kLanes = 4;
constexpr int kLanePoints = 64;
constexpr int kMaxRatchet = 4;

// Un paso empaquetado en 32 bits
struct Step
{
    bool on = false;
    int vel = 100;      // 1..127
    int note = 0;       // -24..+24 semitonos
    int prob = 100;     // 0..100 %
    int ratchet = 1;    // 1..4
    bool slide = false; // ligado (bajo / acordes)

    uint32_t pack() const
    {
        return (uint32_t) (on ? 1 : 0)
             | ((uint32_t) clampv (vel, 0, 127) << 1)
             | ((uint32_t) clampv (note + 64, 0, 127) << 8)
             | ((uint32_t) clampv (prob, 0, 100) << 15)
             | ((uint32_t) (clampv (ratchet, 1, kMaxRatchet) - 1) << 22)
             | ((uint32_t) (slide ? 1 : 0) << 25);
    }
    static Step unpack (uint32_t v)
    {
        Step s;
        s.on = (v & 1u) != 0;
        s.vel = (int) ((v >> 1) & 127u);
        s.note = (int) ((v >> 8) & 127u) - 64;
        s.prob = (int) ((v >> 15) & 127u);
        s.ratchet = (int) ((v >> 22) & 7u) + 1;
        s.slide = ((v >> 25) & 1u) != 0;
        return s;
    }
};

class PatternBank
{
public:
    PatternBank() { clearAll(); }

    void clearAll()
    {
        for (int p = 0; p < kPatterns; ++p) clearPattern (p);
        for (int l = 0; l < kLanes; ++l)
            for (int i = 0; i < kLanePoints; ++i) lanes[l][i].store (0.5f);
    }
    void clearPattern (int p)
    {
        for (int t = 0; t < kTracks; ++t)
        {
            length[p][t].store (kSteps);
            for (int s = 0; s < kSteps; ++s) steps[p][t][s].store (Step().pack());
        }
    }

    Step get (int p, int t, int s) const { return Step::unpack (steps[idx (p)][tr (t)][st (s)].load (std::memory_order_relaxed)); }
    void set (int p, int t, int s, const Step& v) { steps[idx (p)][tr (t)][st (s)].store (v.pack(), std::memory_order_relaxed); ++revision; }
    int getLength (int p, int t) const { return length[idx (p)][tr (t)].load (std::memory_order_relaxed); }
    void setLength (int p, int t, int len) { length[idx (p)][tr (t)].store (clampv (len, 1, kSteps)); ++revision; }

    float lane (int l, int i) const { return lanes[clampv (l, 0, kLanes - 1)][clampv (i, 0, kLanePoints - 1)].load (std::memory_order_relaxed); }
    void setLane (int l, int i, float v) { lanes[clampv (l, 0, kLanes - 1)][clampv (i, 0, kLanePoints - 1)].store (clampv (v, 0.0f, 1.0f)); ++revision; }

    void copyPattern (int from, int to)
    {
        for (int t = 0; t < kTracks; ++t)
        {
            setLength (to, t, getLength (from, t));
            for (int s = 0; s < kSteps; ++s) set (to, t, s, get (from, t, s));
        }
    }

    // Texto compacto para guardar el estado
    std::string serialise() const
    {
        std::string out;
        char buf[16];
        for (int p = 0; p < kPatterns; ++p)
            for (int t = 0; t < kTracks; ++t)
            {
                std::snprintf (buf, sizeof (buf), "%x:", getLength (p, t));
                out += buf;
                for (int s = 0; s < kSteps; ++s)
                {
                    std::snprintf (buf, sizeof (buf), "%x,", steps[p][t][s].load());
                    out += buf;
                }
                out += ";";
            }
        out += "|";
        for (int l = 0; l < kLanes; ++l)
            for (int i = 0; i < kLanePoints; ++i)
            {
                std::snprintf (buf, sizeof (buf), "%d,", (int) std::lround (lane (l, i) * 1000.0f));
                out += buf;
            }
        return out;
    }

    bool deserialise (const std::string& text)
    {
        const auto bar = text.find ('|');
        if (bar == std::string::npos) return false;
        size_t pos = 0;
        for (int p = 0; p < kPatterns; ++p)
            for (int t = 0; t < kTracks; ++t)
            {
                const auto colon = text.find (':', pos);
                if (colon == std::string::npos || colon > bar) return false;
                setLength (p, t, (int) std::strtol (text.substr (pos, colon - pos).c_str(), nullptr, 16));
                pos = colon + 1;
                for (int s = 0; s < kSteps; ++s)
                {
                    const auto comma = text.find (',', pos);
                    if (comma == std::string::npos) return false;
                    steps[p][t][s].store ((uint32_t) std::strtoul (text.substr (pos, comma - pos).c_str(), nullptr, 16));
                    pos = comma + 1;
                }
                pos = text.find (';', pos) + 1;
            }
        pos = bar + 1;
        for (int l = 0; l < kLanes; ++l)
            for (int i = 0; i < kLanePoints; ++i)
            {
                const auto comma = text.find (',', pos);
                if (comma == std::string::npos) return false;
                setLane (l, i, (float) std::atoi (text.substr (pos, comma - pos).c_str()) / 1000.0f);
                pos = comma + 1;
            }
        ++revision;
        return true;
    }

    std::atomic<uint32_t> revision { 0 };

private:
    static int idx (int p) { return clampv (p, 0, kPatterns - 1); }
    static int tr (int t) { return clampv (t, 0, kTracks - 1); }
    static int st (int s) { return clampv (s, 0, kSteps - 1); }

    std::atomic<uint32_t> steps[kPatterns][kTracks][kSteps];
    std::atomic<int> length[kPatterns][kTracks];
    std::atomic<float> lanes[kLanes][kLanePoints];
};

// Información de transporte por bloque
struct Transport
{
    bool playing = false;
    double ppq = 0.0;   // posición en negras al inicio del bloque
    double bpm = 120.0;
};

struct TriggerEvent
{
    int sample = 0;
    int track = 0;
    Hit hit;
};

// Calcula los disparos que caen dentro de un bloque
class StepClock
{
public:
    void prepare (double sampleRate) { fs = sampleRate; reset(); }
    void reset() { lastEnd = -1.0; curPattern = -1; for (auto& s : lastStep) s = -1; }

    void setSwing (float s) { swing = clampv (s, 0.5f, 0.75f); }
    void requestPattern (int p) { wantedPattern = clampv (p, 0, kPatterns - 1); }
    int pattern() const { return curPattern < 0 ? wantedPattern : curPattern; }
    int stepOf (int track) const { return lastStep[clampv (track, 0, kTracks - 1)]; }

    // Llena "events" (ordenados) y devuelve cuántos hay
    int process (const Transport& tr, int numSamples, const PatternBank& bank, uint32_t& rngState,
                 TriggerEvent* events, int maxEvents)
    {
        if (! tr.playing || tr.bpm <= 0.0)
        {
            lastEnd = -1.0;
            return 0;
        }
        const double ppqPerSample = tr.bpm / 60.0 / fs;
        double start = tr.ppq;
        if (lastEnd >= 0.0 && std::fabs (start - lastEnd) < ppqPerSample * 2.0) start = lastEnd; // continuidad exacta
        const double end = start + ppqPerSample * numSamples;
        lastEnd = end;
        if (curPattern < 0) curPattern = wantedPattern;

        int count = 0;
        const double stepLen = 0.25; // semicorchea
        // Recorremos semicorcheas que podrían caer en el bloque (el swing las corre hasta media semicorchea)
        const int64_t first = (int64_t) std::floor (start / stepLen) - 1;
        const int64_t last = (int64_t) std::floor (end / stepLen);
        for (int64_t k = first; k <= last; ++k)
        {
            if (k < 0) continue;
            const bool odd = (k & 1) != 0;
            const double swingOffset = odd ? (swing - 0.5) * 2.0 * stepLen : 0.0;
            const double t0 = (double) k * stepLen + swingOffset;
            // Cambio de patrón en el inicio de compás
            if (k % 16 == 0 && t0 >= start && t0 < end && wantedPattern != curPattern) curPattern = wantedPattern;

            for (int t = 0; t < kTracks; ++t)
            {
                const int len = bank.getLength (curPattern, t);
                const int s = (int) (k % len);
                const Step st = bank.get (curPattern, t, s);
                const double dur = odd ? stepLen * (1.0 - (swing - 0.5) * 2.0) : stepLen * (1.0 + (swing - 0.5) * 2.0);
                if (t0 >= start && t0 < end) lastStep[t] = s;
                if (! st.on) continue;
                for (int r = 0; r < st.ratchet; ++r)
                {
                    const double tr0 = t0 + dur * (double) r / (double) st.ratchet;
                    if (tr0 < start || tr0 >= end) continue;
                    if (r == 0 && st.prob < 100)
                    {
                        rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
                        if ((int) (rngState % 100u) >= st.prob) break;
                    }
                    if (count >= maxEvents) break;
                    TriggerEvent& e = events[count++];
                    e.sample = clampv ((int) ((tr0 - start) / ppqPerSample), 0, numSamples - 1);
                    e.track = t;
                    e.hit.vel = (float) st.vel / 127.0f * (r > 0 ? 0.82f : 1.0f);
                    e.hit.note = (float) st.note;
                    e.hit.slide = st.slide && r == 0;
                    e.hit.accent = st.vel >= 118;
                }
            }
        }
        // Orden por muestra (inserción: pocos eventos)
        for (int i = 1; i < count; ++i)
        {
            TriggerEvent v = events[i];
            int j = i - 1;
            while (j >= 0 && events[j].sample > v.sample) { events[j + 1] = events[j]; --j; }
            events[j + 1] = v;
        }
        return count;
    }

private:
    double fs = 44100.0, lastEnd = -1.0;
    float swing = 0.5f;
    int wantedPattern = 0, curPattern = -1;
    int lastStep[kTracks] { -1, -1, -1, -1, -1, -1, -1, -1 };
};

} // namespace ogro
