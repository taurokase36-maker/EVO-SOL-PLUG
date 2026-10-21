// OGRO 2 (copia propia del DSP de EVO SOL) — ear candy: doblaje estéreo, delay ping-pong sincronizado,
// reverb FDN, filtro "radio" y ancho estéreo.
#pragma once

#include "Common.h"

namespace evo2
{
// --------------------------------------------------------------------------
// Doblaje: dos copias con micro-retardo y micro-afinación moduladas al azar
// (simula una segunda toma), abiertas a izquierda y derecha.
// --------------------------------------------------------------------------
class Doubler
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        dl.prepare ((int) (0.06 * fs));
        modL.prepare (fs, 0.9f, 1234u);
        modR.prepare (fs, 0.7f, 98765u);
        for (auto* f : { &hpL, &hpR }) f->set (Biquad::Type::HighPass, fs, 220.0, 0.6);
        for (auto* f : { &lpL, &lpR }) f->set (Biquad::Type::LowPass, fs, 9000.0, 0.6);
    }
    void setAmount (float a) { amount = clampv (a, 0.0f, 1.0f); }

    inline void process (float x, float& outL, float& outR) noexcept
    {
        dl.push (x);
        if (amount <= 0.0f) { outL = outR = 0.0f; return; }
        const float dL = (float) fs * (0.0115f + 0.0016f * modL.next());
        const float dR = (float) fs * (0.0185f + 0.0019f * modR.next());
        const float g = amount * 0.75f;
        outL = lpL.process (hpL.process (dl.read (dL))) * g;
        outR = lpR.process (hpR.process (dl.read (dR))) * g;
    }
    void flush() { hpL.flush(); hpR.flush(); lpL.flush(); lpR.flush(); }

private:
    double fs = 44100.0;
    DelayLine dl;
    SmoothRandom modL, modR;
    Biquad hpL, hpR, lpL, lpR;
    float amount = 0.0f;
};

// --------------------------------------------------------------------------
// Delay ping-pong sincronizado al tempo, con filtros en la realimentación
// y "ducking" (baja mientras cantás y aparece en los huecos).
// --------------------------------------------------------------------------
class PingPongDelay
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const int maxS = (int) (2.6 * fs);
        dlL.prepare (maxS); dlR.prepare (maxS);
        hp[0].set (Biquad::Type::HighPass, fs, 280.0, 0.7);
        hp[1].set (Biquad::Type::HighPass, fs, 280.0, 0.7);
        lp[0].set (Biquad::Type::LowPass, fs, 5200.0, 0.7);
        lp[1].set (Biquad::Type::LowPass, fs, 5200.0, 0.7);
        timeSm.reset (fs, 250.0f, (float) (0.375 * fs));
        aEnvA = onePoleCoef (5.0f, fs);
        aEnvR = onePoleCoef (250.0f, fs);
        env = 0.0f;
    }
    void setTimeSeconds (float s) { timeSm.setTarget (clampv (s, 0.01f, 2.5f) * (float) fs); }
    void clear() noexcept { dlL.clear(); dlR.clear(); for (auto& f : hp) f.reset(); for (auto& f : lp) f.reset(); env = 0.0f; timeSm.snapToTarget(); }
    void setFeedback (float f) { feedback = clampv (f, 0.0f, 0.92f); }
    void setDuck (float d) { duck = clampv (d, 0.0f, 1.0f); }
    void setMix (float m) { mix = clampv (m, 0.0f, 1.0f); }
    void setTone (float t) // 0 = oscuro, 1 = brillante
    {
        const double f = 2500.0 * std::pow (2.0, 2.5 * clampv (t, 0.0f, 1.0f));
        lp[0].set (Biquad::Type::LowPass, fs, f, 0.7);
        lp[1].set (Biquad::Type::LowPass, fs, f, 0.7);
    }

    inline void process (float x, float& outL, float& outR) noexcept
    {
        const float d = timeSm.next();
        const float yL = dlL.read (d);
        const float yR = dlR.read (d);
        // Entrada al canal izquierdo; los ecos rebotan L -> R -> L
        const float fbL = lp[0].process (hp[0].process (x + yR * feedback));
        const float fbR = lp[1].process (hp[1].process (yL * feedback));
        dlL.push (sanitize (fbL));
        dlR.push (sanitize (fbR));

        const float ax = std::fabs (x);
        env += (ax > env ? aEnvA : aEnvR) * (ax - env);
        const float duckGain = 1.0f - duck * clampv (env * 8.0f, 0.0f, 0.85f);
        outL = yL * mix * duckGain;
        outR = yR * mix * duckGain;
    }
    void flush() { for (auto& f : hp) f.flush(); for (auto& f : lp) f.flush(); }

private:
    double fs = 44100.0;
    DelayLine dlL, dlR;
    Biquad hp[2], lp[2];
    SmoothedValue timeSm;
    float feedback = 0.3f, duck = 0.5f, mix = 0.0f;
    float aEnvA = 0.1f, aEnvR = 0.001f, env = 0.0f;
};

// --------------------------------------------------------------------------
// Reverb FDN de 8 líneas (matriz de Householder), con pre-delay, filtro de
// entrada, amortiguación y modulación suave para evitar el sonido metálico.
// --------------------------------------------------------------------------
class FdnReverb
{
public:
    static constexpr int N = 8;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (int i = 0; i < N; ++i) lines[i].prepare ((int) (0.25 * fs));
        pre.prepare ((int) (0.2 * fs));
        for (int i = 0; i < 4; ++i) diff[i].prepare ((int) (0.02 * fs));
        inHp.set (Biquad::Type::HighPass, fs, 320.0, 0.7);
        inLp.set (Biquad::Type::LowPass, fs, 9000.0, 0.7);
        for (int i = 0; i < N; ++i) { damp[i].setLowpass (fs, 6000.0); damp[i].reset(); }
        modA.prepare (fs, 0.6f, 4242u);
        modB.prepare (fs, 0.45f, 777u);
        aEnvA = onePoleCoef (5.0f, fs);
        aEnvR = onePoleCoef (300.0f, fs);
        update();
    }
    void setSize (float s) { size = clampv (s, 0.0f, 1.0f); update(); }
    void clear() noexcept
    {
        for (auto& l : lines) l.clear();
        pre.clear();
        for (auto& d : diff) d.clear();
        for (auto& d : damp) d.reset();
        inHp.reset(); inLp.reset();
        modA.prepare (fs, 0.6f, 4242u);
        modB.prepare (fs, 0.45f, 777u);
        env = 0.0f;
    }
    void setDecay (float seconds) { rt60 = clampv (seconds, 0.2f, 8.0f); update(); }
    void setPreDelayMs (float ms) { preDelay = clampv (ms, 0.0f, 180.0f) * 0.001f * (float) fs; }
    void setDamping (float d)
    {
        const double f = 12000.0 * std::pow (0.2, clampv (d, 0.0f, 1.0f));
        for (auto& dmp : damp) dmp.setLowpass (fs, f);
    }
    void setMix (float m) { mix = clampv (m, 0.0f, 1.0f); }
    void setDuck (float d) { duck = clampv (d, 0.0f, 1.0f); }

    inline void process (float x, float& outL, float& outR) noexcept
    {
        pre.push (x);
        float in = preDelay > 1.0f ? pre.read (preDelay) : x;
        in = inLp.process (inHp.process (in));

        // Difusión de entrada (allpass en serie)
        for (int i = 0; i < 4; ++i)
        {
            const float g = 0.62f;
            const float delayed = diff[i].readInt (diffLen[i] - 1);
            const float v = in + g * delayed;
            diff[i].push (v);
            in = delayed - g * v;
        }

        float s[N];
        const float ma = modA.next() * 0.0011f * (float) fs;
        const float mb = modB.next() * 0.0009f * (float) fs;
        for (int i = 0; i < N; ++i)
        {
            const float dd = len[i] + (i == 1 ? ma : (i == 5 ? mb : 0.0f));
            s[i] = damp[i].lp (lines[i].read (dd)) * gains[i];
        }
        // Householder: y = s - (2/N) * sum(s)
        float sum = 0.0f;
        for (int i = 0; i < N; ++i) sum += s[i];
        const float k = 2.0f / N * sum;
        for (int i = 0; i < N; ++i)
            lines[i].push (sanitize (s[i] - k + (i % 2 == 0 ? in : -in) * 0.5f));

        const float l = s[0] - s[2] + s[4] - s[6] + 0.5f * (s[1] + s[7]);
        const float r = s[1] - s[3] + s[5] - s[7] + 0.5f * (s[2] + s[6]);

        const float ax = std::fabs (x);
        env += (ax > env ? aEnvA : aEnvR) * (ax - env);
        const float duckGain = 1.0f - duck * clampv (env * 6.0f, 0.0f, 0.8f);
        outL = l * mix * 0.35f * duckGain;
        outR = r * mix * 0.35f * duckGain;
    }
    void flush() { inHp.flush(); inLp.flush(); }

private:
    void update()
    {
        static const float baseMs[N] = { 31.7f, 37.1f, 41.3f, 46.9f, 53.3f, 59.9f, 67.7f, 73.1f };
        static const float diffMs[4] = { 4.7f, 3.6f, 2.9f, 1.9f };
        const float scale = 0.45f + 1.25f * size;
        for (int i = 0; i < N; ++i)
        {
            len[i] = baseMs[i] * scale * 0.001f * (float) fs;
            gains[i] = std::pow (10.0f, -3.0f * len[i] / (rt60 * (float) fs));
        }
        for (int i = 0; i < 4; ++i) diffLen[i] = std::max (2, (int) (diffMs[i] * 0.001f * (float) fs));
    }

    double fs = 44100.0;
    DelayLine lines[N], pre, diff[4];
    OnePole damp[N];
    Biquad inHp, inLp;
    SmoothRandom modA, modB;
    float len[N] {}, gains[N] {};
    int diffLen[4] { 200, 150, 120, 80 };
    float size = 0.5f, rt60 = 1.8f, preDelay = 0.0f, mix = 0.0f, duck = 0.4f;
    float aEnvA = 0.1f, aEnvR = 0.001f, env = 0.0f;
};

// --------------------------------------------------------------------------
// Radio / lo-fi: banda telefónica con saturación
// --------------------------------------------------------------------------
class RadioFilter
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        hp1.set (Biquad::Type::HighPass, fs, 550.0, 0.9);
        hp2.set (Biquad::Type::HighPass, fs, 550.0, 0.6);
        lp1.set (Biquad::Type::LowPass, fs, 3400.0, 1.1);
        lp2.set (Biquad::Type::LowPass, fs, 3400.0, 0.6);
        amt.reset (fs, 30.0f, 0.0f);
    }
    void setAmount (float a) { amt.setTarget (clampv (a, 0.0f, 1.0f)); }

    inline float process (float x) noexcept
    {
        const float a = amt.next();
        const float band = lp2.process (lp1.process (hp2.process (hp1.process (x))));
        if (a <= 1.0e-4f) return x;
        const float wet = std::tanh (band * 3.0f) * 0.55f;
        return x + a * (wet - x);
    }
    void flush() { hp1.flush(); hp2.flush(); lp1.flush(); lp2.flush(); }

private:
    double fs = 44100.0;
    Biquad hp1, hp2, lp1, lp2;
    SmoothedValue amt;
};

} // namespace evo2
