// OGRO 2 — reproductor de samples (pista TEXTURAS y capa de samples de ACORDES).
// Cada sample se edita como un sonido sintetizado: inicio, largo, reversa, pitch y envolvente
// de pitch, filtro ladder o SVF con envolvente, ADSR y drive. Interpolación Hermite en LIVE y
// sinc de 16 puntos (con antialias al subir el pitch) en ALTA. No pide memoria en el audio.
#pragma once

#include "SamplePool.h"
#include "Voices.h"

namespace ogro2
{
struct SampleParams
{
    float start = 0.0f, length = 1.0f;   // 0..1 del sample
    bool reverse = false;
    float semis = 0.0f;                  // transposición total
    float pitchEnv = 0.0f;               // semitonos al principio (cae en ~120 ms)
    float cutoffHz = 20000.0f, reso = 0.0f, envOct = 0.0f;
    bool ladder = true;
    float drive = 0.0f;
    float attackMs = 2.0f, sustain = 1.0f, releaseMs = 300.0f;
    float gain = 1.0f;
};

class SamplePlayer
{
public:
    static constexpr int kVoices = 3;
    static constexpr int kTaps = 16, kPhases = 256, kTables = 4;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        // Tablas sinc con ventana de Blackman: corte 0.9, 0.7, 0.5 y 0.35 de Nyquist
        static const float cut[kTables] = { 0.9f, 0.7f, 0.5f, 0.35f };
        for (int t = 0; t < kTables; ++t)
            for (int p = 0; p < kPhases; ++p)
            {
                const double frac = (double) p / kPhases;
                double sum = 0.0;
                for (int k = 0; k < kTaps; ++k)
                {
                    const double x = (double) (k - kTaps / 2 + 1) - frac;
                    const double sincArg = 3.14159265358979323846 * x * cut[t];
                    const double s = std::fabs (x) < 1e-9 ? 1.0 : std::sin (sincArg) / sincArg;
                    const double wpos = (double) (k + 1 - frac) / (double) kTaps;
                    const double w = 0.42 - 0.5 * std::cos (2.0 * 3.14159265358979323846 * wpos) + 0.08 * std::cos (4.0 * 3.14159265358979323846 * wpos);
                    table[t][p][k] = (float) (s * w * cut[t]);
                    sum += s * w * cut[t];
                }
                for (int k = 0; k < kTaps; ++k) table[t][p][k] = (float) (table[t][p][k] / sum);
            }
        reset();
    }
    void setHighQuality (bool h) { high = h; }
    void reset()
    {
        for (auto& v : voice) { v.active = false; v.fl.reset(); v.fr.reset(); v.ll.reset(); v.lr.reset(); }
    }
    bool isActive() const
    {
        for (const auto& v : voice) if (v.active) return true;
        return false;
    }
    void release() { for (auto& v : voice) v.gateLeft = 0.0; }

    void trigger (const SampleEntry* e, const SampleParams& p, const Hit& h)
    {
        if (e == nullptr || ! e->ready.load (std::memory_order_acquire) || e->frames < 4) return;
        // Voz libre o la más vieja (las colas de pads se superponen)
        int vi = 0;
        for (int k = 0; k < kVoices; ++k)
        {
            if (! voice[k].active) { vi = k; break; }
            if (voice[k].age < voice[vi].age) vi = k;
        }
        Voice& v = voice[vi];
        v.e = e;
        v.p = p;
        const double a = clampv ((double) p.start, 0.0, 0.98) * (e->frames - 1);
        const double len = std::max (16.0, clampv ((double) p.length, 0.01, 1.0) * (e->frames - 1));
        v.first = a;
        v.last = std::min ((double) e->frames - 2, a + len);
        v.pos = p.reverse ? v.last : v.first;
        v.amp = 0.0f; v.attack = true; v.releasing = false;
        v.fenv = 1.0f; v.penv = 1.0f;
        v.gain = velGain (h.vel) * p.gain;
        v.gateLeft = h.gateSamples > 0.0 ? h.gateSamples : 1.0e18;
        v.age = ++age;
        v.active = true;
        v.fl.reset(); v.fr.reset(); v.ll.reset(); v.lr.reset();
        setCoefs (v);
    }

    // Suma a outL / outR
    void render (float* outL, float* outR, int n) noexcept
    {
        for (auto& v : voice)
        {
            if (! v.active) continue;
            const SampleEntry& e = *v.e;
            const int ch = e.channels;
            const double baseInc = e.sampleRate / fs;
            const float penvC = decayCoef (120.0f, fs), fenvC = decayCoef (300.0f, fs);
            const float driveGain = 1.0f + v.p.drive * 8.0f, driveNorm = 1.0f / (1.0f + v.p.drive * 2.5f);
            for (int i = 0; i < n; ++i)
            {
                if ((i & 15) == 0)
                {
                    v.inc = baseInc * std::exp2 ((v.p.semis + v.p.pitchEnv * v.penv) / 12.0);
                    const float fc = std::min (v.p.cutoffHz * std::exp2 (v.p.envOct * v.fenv), 0.45f * (float) fs);
                    // Con el filtro abierto del todo no se filtra (el sample suena limpio)
                    v.filterOn = fc < 0.42f * (float) fs || v.p.reso > 0.05f;
                    if (v.p.ladder) { v.ll.set (fs, fc, v.p.reso * 0.95f); v.lr.set (fs, fc, v.p.reso * 0.95f); }
                    else { v.fl.set (fs, fc, 0.7f + v.p.reso * 6.0f); v.fr.set (fs, fc, 0.7f + v.p.reso * 6.0f); }
                }
                float l, r;
                read (e, ch, v.pos, v.inc, l, r);
                v.pos += v.p.reverse ? -v.inc : v.inc;
                // Envolvente
                if (v.attack) { v.amp += v.atkC * (1.02f - v.amp); if (v.amp >= 1.0f) { v.amp = 1.0f; v.attack = false; } }
                else if (v.p.sustain > 0.0f && ! v.releasing)
                {
                    if ((v.gateLeft -= 1.0) <= 0.0) v.releasing = true;
                    v.amp += 0.001f * (v.p.sustain - v.amp);
                }
                else v.amp *= v.relC;
                v.penv *= penvC;
                v.fenv *= fenvC;
                // Fundido al final del tramo (sin click)
                const double left = v.p.reverse ? v.pos - v.first : v.last - v.pos;
                const float edge = (float) std::min (1.0, left / (0.002 * e.sampleRate));
                if (left <= 0.0 || (! v.attack && v.amp < 1.0e-4f)) { v.active = false; break; }
                if (v.filterOn)
                {
                    if (v.p.ladder) { l = v.ll.process (l); r = v.lr.process (r); }
                    else { l = v.fl.lp (l); r = v.fr.lp (r); }
                }
                if (v.p.drive > 0.001f) { l = std::tanh (l * driveGain) * driveNorm * 1.6f; r = std::tanh (r * driveGain) * driveNorm * 1.6f; }
                const float g = v.amp * v.gain * edge;
                outL[i] += l * g;
                outR[i] += r * g;
            }
            v.fl.flush(); v.fr.flush(); v.ll.flush(); v.lr.flush();
        }
    }

private:
    struct Voice
    {
        const SampleEntry* e = nullptr;
        SampleParams p;
        bool active = false, attack = false, releasing = false, filterOn = false;
        double pos = 0.0, inc = 1.0, first = 0.0, last = 0.0, gateLeft = 0.0;
        float amp = 0.0f, fenv = 0.0f, penv = 0.0f, gain = 1.0f, atkC = 0.1f, relC = 0.999f;
        uint32_t age = 0;
        Svf fl, fr;
        Ladder ll, lr;
    };
    void setCoefs (Voice& v)
    {
        v.atkC = 1.0f - (float) std::exp (-1.0 / (std::max (0.5f, v.p.attackMs) * 0.001 * fs));
        v.relC = decayCoef (std::max (5.0f, v.p.releaseMs), fs);
    }
    inline float sampleAt (const SampleEntry& e, int ch, int frame, int c) const noexcept
    {
        frame = clampv (frame, 0, e.frames - 1);
        return (float) e.pcm[(size_t) (frame * ch + (ch == 2 ? c : 0))] * (1.0f / 32768.0f);
    }
    inline void read (const SampleEntry& e, int ch, double pos, double inc, float& l, float& r) const noexcept
    {
        const int i0 = (int) std::floor (pos);
        const float fr = (float) (pos - (double) i0);
        if (! high)
        {
            // Hermite de 4 puntos
            for (int c = 0; c < 2; ++c)
            {
                const float xm1 = sampleAt (e, ch, i0 - 1, c), x0 = sampleAt (e, ch, i0, c), x1 = sampleAt (e, ch, i0 + 1, c), x2 = sampleAt (e, ch, i0 + 2, c);
                const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
                (c == 0 ? l : r) = ((c3 * fr + c2) * fr + c1) * fr + x0;
            }
            return;
        }
        // Sinc de 16 puntos; al subir el pitch usa un corte más bajo (sin aliasing)
        const int t = inc <= 1.1 ? 0 : (inc <= 1.45 ? 1 : (inc <= 2.0 ? 2 : 3));
        const float* k = table[t][clampv ((int) (fr * kPhases), 0, kPhases - 1)];
        float sl = 0.0f, sr = 0.0f;
        for (int j = 0; j < kTaps; ++j)
        {
            const int idx = i0 - kTaps / 2 + 1 + j;
            sl += k[j] * sampleAt (e, ch, idx, 0);
            sr += k[j] * sampleAt (e, ch, idx, 1);
        }
        l = sl; r = sr;
    }

    double fs = 44100.0;
    bool high = false;
    uint32_t age = 0;
    Voice voice[kVoices];
    float table[kTables][kPhases][kTaps] {};
};

// ---------------------------------------------------------------------------
// TEXTURAS: one-shots y pads de Orbhitz (HIT, PAD, SFX y también CHD)
// Perillas: INICIO · LARGO · PITCH · CORTE · RESO · ENV · DRIVE · P.ENV
// ---------------------------------------------------------------------------
class TextureVoice : public StereoVoice
{
public:
    void prepare (double sampleRate, uint32_t) override { fs = sampleRate; player.prepare (sampleRate); }
    void setParams (const float* p) override { for (int i = 0; i < kSoundParams; ++i) prm[i] = p[i]; }
    void setSongTime (double, bool, uint32_t) override {}
    void setEnv (float attackMs, float sus, float releaseMs) { atk = attackMs; susLevel = sus; rel = releaseMs; }
    void setExtras (bool rev, bool ladderFilter, bool highQuality) { reverse = rev; ladder = ladderFilter; player.setHighQuality (highQuality); }
    // El motor elige el sample y la transposición antes de cada golpe
    void setNext (const SampleEntry* e, float semis) { next = e; nextSemis = semis; }
    void trigger (const Hit& h) override
    {
        SampleParams sp;
        sp.start = prm[0];
        sp.length = std::max (0.01f, prm[1]);
        sp.reverse = reverse;
        sp.ladder = ladder;
        sp.semis = nextSemis;
        sp.cutoffHz = expMap (prm[3], 60.0f, 20000.0f);
        sp.reso = prm[4];
        sp.envOct = prm[5] * 5.0f;
        sp.drive = prm[6];
        sp.pitchEnv = maps::texPitchEnv (prm[7]);
        sp.attackMs = atk;
        sp.sustain = susLevel;
        sp.releaseMs = rel;
        player.trigger (next, sp, h);
    }
    void release() override { player.release(); }
    void render (float* outL, float* outR, int n) noexcept override
    {
        for (int i = 0; i < n; ++i) outL[i] = outR[i] = 0.0f;
        player.render (outL, outR, n);
    }
    bool isActive() const override { return player.isActive(); }
    void resetState() noexcept override { player.reset(); }

private:
    double fs = 44100.0;
    float prm[kSoundParams] {};
    float atk = 2.0f, susLevel = 1.0f, rel = 300.0f, nextSemis = 0.0f;
    bool reverse = false, ladder = true;
    const SampleEntry* next = nullptr;
    SamplePlayer player;
};

} // namespace ogro2
