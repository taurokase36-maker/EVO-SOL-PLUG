// OGRO 2 — síntesis de las 8 voces (todo sintetizado, sin samples)
#pragma once

#include "VoiceMaps.h"

namespace ogro2
{
// ---------------------------------------------------------------------------
// Utilidades de síntesis
// ---------------------------------------------------------------------------
class Noise
{
public:
    explicit Noise (uint32_t seed = 0x9E3779B9u) : s (seed ? seed : 1u) {}
    inline float next() noexcept
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) (int32_t) s * (1.0f / 2147483648.0f);
    }
    inline float uni() noexcept { return 0.5f * (next() + 1.0f); }
    void seed (uint32_t v) { s = v ? v : 1u; }

private:
    uint32_t s;
};

inline float polyBlep (float t, float dt) noexcept
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}
inline float blepSaw (float ph, float dt) noexcept { return 2.0f * ph - 1.0f - polyBlep (ph, dt); }
inline float blepSquare (float ph, float dt) noexcept
{
    float v = ph < 0.5f ? 1.0f : -1.0f;
    v += polyBlep (ph, dt);
    float p2 = ph + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
    v -= polyBlep (p2, dt);
    return v;
}

// Coeficiente para que una envolvente exponencial caiga 60 dB en "ms"
inline float decayCoef (float ms, double fs) noexcept
{
    return (float) std::exp (-6.907755 / (std::max (0.1, (double) ms) * 0.001 * fs));
}

// Seno rápido de precisión suficiente para osciladores (fase 0..1)
inline float sin01 (float ph) noexcept { return (float) std::sin (kTwoPi * ph); }

// Filtro de estado variable TPT (Zavalishin): LP / BP / HP simultáneos
class Svf
{
public:
    void reset() { ic1 = ic2 = 0.0f; }
    void set (double fs, float hz, float q)
    {
        g = (float) std::tan (kPi * clampv ((double) hz, 10.0, fs * 0.47) / fs);
        k = 1.0f / std::max (0.3f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    inline void process (float x, float& lp, float& bp, float& hp) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
    inline float lp (float x) noexcept { float l, b, h; process (x, l, b, h); return l; }
    inline float bp (float x) noexcept { float l, b, h; process (x, l, b, h); return b; }
    inline float hp (float x) noexcept { float l, b, h; process (x, l, b, h); return h; }
    void flush() noexcept
    {
        if (! std::isfinite (ic1) || ! std::isfinite (ic2)) reset();
        if (std::fabs (ic1) < 1e-20f) ic1 = 0;
        if (std::fabs (ic2) < 1e-20f) ic2 = 0;
    }

private:
    float g = 0.1f, k = 1.4f, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
};

// Filtro escalera de 4 polos (ZDF) con saturación: el corazón del bajo analógico
class Ladder
{
public:
    void reset() { s[0] = s[1] = s[2] = s[3] = 0.0f; }
    void set (double fs, float hz, float reso)
    {
        g = (float) std::tan (kPi * clampv ((double) hz, 15.0, fs * 0.45) / fs);
        G = g / (1.0f + g);
        k = 4.0f * clampv (reso, 0.0f, 0.985f);
    }
    inline float process (float x) noexcept
    {
        const float S1 = s[0] / (1.0f + g), S2 = s[1] / (1.0f + g), S3 = s[2] / (1.0f + g), S4 = s[3] / (1.0f + g);
        const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;
        const float sigma = G3 * S1 + G2 * S2 + G * S3 + S4;
        float u = (x - k * sigma) / (1.0f + k * G4);
        u = std::tanh (u); // saturación en la entrada de la escalera
        float y = u;
        for (int i = 0; i < 4; ++i)
        {
            const float v = (y - s[i]) * G;
            const float out = v + s[i];
            s[i] = out + v;
            y = out;
        }
        return y * (1.0f + 0.45f * k); // compensa la pérdida de graves con resonancia
    }
    void flush() noexcept
    {
        for (auto& v : s)
        {
            if (! std::isfinite (v)) { reset(); return; }
            if (std::fabs (v) < 1e-20f) v = 0.0f;
        }
    }

private:
    float g = 0.1f, G = 0.09f, k = 0.0f, s[4] {};
};

// Deriva suave que depende solo del tiempo del tema (azar FIJO): mismo lugar = misma deriva
inline float positionalDrift (uint32_t seed, uint32_t gen, double seconds, float rateHz)
{
    const double x = seconds * (double) rateHz;
    const double fl = std::floor (x);
    const int64_t seg = (int64_t) fl;
    const float t = (float) (x - fl);
    const float a = hashBipolar (hash32 (seed, gen, (uint32_t) seg, (uint32_t) (seg >> 32)));
    const float b = hashBipolar (hash32 (seed, gen, (uint32_t) (seg + 1), (uint32_t) ((seg + 1) >> 32)));
    const float sm = t * t * (3.0f - 2.0f * t);
    return a + (b - a) * sm;
}

// Suaviza el reinicio de una voz que todavía sonaba (evita clicks)
struct Declick
{
    float offset = 0.0f, coef = 0.99f;
    void prepare (double fs) { coef = (float) std::exp (-1.0 / (0.0015 * fs)); }
    void start (float lastOut) { offset = lastOut; }
    inline float apply (float y) noexcept { offset *= coef; return y + offset; }
};

inline float velGain (float v) { return 0.12f + 0.88f * std::pow (clampv (v, 0.0f, 1.0f), 1.4f); }

// ---------------------------------------------------------------------------
// Base de las voces mono
// ---------------------------------------------------------------------------
class MonoVoice
{
public:
    virtual ~MonoVoice() = default;
    virtual void prepare (double sampleRate, uint32_t seed)
    {
        fs = sampleRate;
        initSeed = seed;
        rng.seed (seed);
        declick.prepare (fs);
    }
    void setParams (const float* p) { for (int i = 0; i < kSoundParams; ++i) prm[i] = p[i]; }
    virtual void trigger (const Hit& h) = 0;
    virtual void render (float* out, int n) noexcept = 0;
    virtual void choke() {}
    virtual void release() {}
    // Estado inicial (render repetible con azar FIJO). No pide memoria.
    virtual void resetState() noexcept { active = false; lastOut = 0.0f; declick.offset = 0.0f; rng.seed (initSeed); }
    // Tiempo del tema en segundos (azar FIJO: deriva y LFOs internos dependen de la posición)
    void setSongTime (double seconds, bool fixed, uint32_t s) { songSeconds = seconds; fixedRandom = fixed; randomSeed = s; }
    bool isActive() const { return active; }
    float lastOut = 0.0f;

protected:
    // Azar FIJO: el ruido y la VARIA de cada golpe salen de su semilla
    void beginHit (const Hit& h) { if (h.fixedSeed) rng.seed (h.seed); }
    double songSeconds = 0.0;
    bool fixedRandom = false;
    uint32_t randomSeed = 1, initSeed = 1;

    // VARIA: variación humana por golpe (afinación, decay y nivel)
    void humanize (float amount, float& pitchSemis, float& decayMul, float& gainMul)
    {
        pitchSemis = rng.next() * 0.35f * amount;
        decayMul = 1.0f + rng.next() * 0.18f * amount;
        gainMul = dbToGain (rng.next() * 2.0f * amount);
    }

    double fs = 44100.0;
    float prm[kSoundParams] {};
    Noise rng;
    Declick declick;
    bool active = false;
};

// ---------------------------------------------------------------------------
// KICK: seno con envolvente de pitch, click y drive
// ---------------------------------------------------------------------------
class KickVoice : public MonoVoice
{
public:
    void prepare (double sr, uint32_t seed) override
    {
        MonoVoice::prepare (sr, seed);
        clickHp.set (Biquad::Type::HighPass, fs, 1800.0, 0.7);
        color.reset();
    }
    void trigger (const Hit& h) override
    {
        beginHit (h);
        declick.start (lastOut);
        float ps, dm, gm;
        humanize (prm[7], ps, dm, gm);
        phase = 0.0f;
        amp = 1.0f;
        pitchEnv = 1.0f;
        clickEnv = 1.0f;
        gain = velGain (h.vel) * gm * (h.accent ? 1.12f : 1.0f);
        noteMul = std::pow (2.0f, (h.note + ps) / 12.0f);
        decayMul = dm * (h.accent ? 1.1f : 1.0f);
        accentDrive = h.accent ? 0.2f : 0.0f;
        active = true;
    }
    void render (float* out, int n) noexcept override
    {
        using namespace maps;
        const float base = kickTune (prm[0]) * noteMul;
        const float aC = decayCoef (kickDecay (prm[1]) * decayMul, fs);
        const float punch = kickPunch (prm[2]);
        const float pC = decayCoef (kickCurve (prm[3]) * 3.0f, fs);
        const float cC = decayCoef (6.0f, fs);
        const float click = prm[4];
        const float drive = clampv (prm[5] + accentDrive, 0.0f, 1.0f);
        const float k = 1.0f + drive * drive * 9.0f;
        const float norm = 1.0f / std::tanh (k * 0.8f) * 0.8f;
        color.setLowpass (fs, colorHz (prm[6]));
        const float invFs = (float) (1.0 / fs);
        for (int i = 0; i < n; ++i)
        {
            if (! active) { out[i] = declick.apply (0.0f); continue; }
            const float f = base * std::exp2 (punch * pitchEnv / 12.0f);
            phase += f * invFs;
            if (phase >= 1.0f) phase -= 1.0f;
            const float body = sin01 (phase) * amp;
            const float cl = (clickHp.process (rng.next()) * 0.6f + sin01 (phase * 3.0f) * 0.3f) * clickEnv * click;
            float y = std::tanh (k * (body + cl)) * norm;
            y = color.lp (y) * gain;
            amp *= aC; pitchEnv *= pC; clickEnv *= cC;
            if (amp < 1.0e-4f) active = false;
            out[i] = lastOut = declick.apply (y);
        }
        clickHp.flush();
    }

public:
    void resetState() noexcept override
    {
        MonoVoice::resetState();
        phase = amp = pitchEnv = clickEnv = 0.0f;
        gain = noteMul = decayMul = 1.0f; accentDrive = 0.0f;
        clickHp.reset(); color.reset();
    }

private:
    float phase = 0, amp = 0, pitchEnv = 0, clickEnv = 0, gain = 1, noteMul = 1, decayMul = 1, accentDrive = 0;
    Biquad clickHp;
    OnePole color;
};

// ---------------------------------------------------------------------------
// CLAP: ráfagas de ruido filtrado + cola + room corto
// ---------------------------------------------------------------------------
class ClapVoice : public MonoVoice
{
public:
    void prepare (double sr, uint32_t seed) override
    {
        MonoVoice::prepare (sr, seed);
        for (int i = 0; i < 3; ++i) room[i].prepare ((int) (0.06 * fs));
        roomLp.setLowpass (fs, 4500.0);
    }
    void trigger (const Hit& h) override
    {
        beginHit (h);
        float ps, dm, gm;
        humanize (prm[7], ps, dm, gm);
        t = 0;
        gain = velGain (h.vel) * gm * (h.accent ? 1.15f : 1.0f);
        toneMul = std::pow (2.0f, (h.note + ps) / 12.0f);
        decayMul = dm;
        active = true;
    }
    void render (float* out, int n) noexcept override
    {
        using namespace maps;
        const float tone = clapTone (prm[0]) * toneMul;
        bp.set (fs, tone, 0.7f + prm[4] * 2.6f);
        const int spread = std::max (1, (int) (clapSpread (prm[2]) * 0.001 * fs));
        const int bursts = clapBursts (prm[3]);
        const float burstTau = (float) (0.0045 * fs);
        const float tailC = decayCoef (clapDecay (prm[1]) * decayMul, fs);
        const float drive = 1.0f + prm[5] * prm[5] * 8.0f;
        const float roomMix = prm[6];
        static const float roomMs[3] = { 19.0f, 27.0f, 37.0f };
        for (int i = 0; i < n; ++i)
        {
            float env = 0.0f;
            if (active)
            {
                const int burstEnd = bursts * spread;
                if (t < burstEnd)
                {
                    const int local = t % spread;
                    env = std::exp (-(float) local / burstTau) * (t / spread == bursts - 1 ? 1.0f : 0.8f);
                    if (t == burstEnd - 1) tail = 1.0f;
                }
                else
                {
                    env = tail;
                    tail *= tailC;
                    if (tail < 1.0e-4f) active = false;
                }
                ++t;
            }
            const float x = bp.bp (rng.next()) * env * 3.0f;
            float y = std::tanh (x * drive) / std::sqrt (drive) * gain;
            // Room: 3 combs cortos amortiguados
            float r = 0.0f;
            for (int c = 0; c < 3; ++c)
            {
                const float d = room[c].read ((float) (roomMs[c] * 0.001 * fs));
                room[c].push (y + roomLp.lp (d) * 0.55f);
                r += d;
            }
            y = y + roomMix * r * 0.45f;
            out[i] = lastOut = y;
        }
        bp.flush();
    }

public:
    void resetState() noexcept override
    {
        MonoVoice::resetState();
        t = 0; tail = 0.0f;
        gain = toneMul = decayMul = 1.0f;
        bp.reset(); roomLp.reset();
        for (auto& r : room) r.clear();
    }

private:
    int t = 0;
    float tail = 0, gain = 1, toneMul = 1, decayMul = 1;
    Svf bp;
    DelayLine room[3];
    OnePole roomLp;
};

// ---------------------------------------------------------------------------
// RIM / SNARE: dos modos resonantes + click + ruido
// ---------------------------------------------------------------------------
class RimVoice : public MonoVoice
{
public:
    void trigger (const Hit& h) override
    {
        beginHit (h);
        declick.start (lastOut);
        float ps, dm, gm;
        humanize (prm[7], ps, dm, gm);
        ph1 = ph2 = 0.0f;
        amp = 1.0f; noiseEnv = 1.0f; clickEnv = 1.0f;
        gain = velGain (h.vel) * gm * (h.accent ? 1.15f : 1.0f);
        toneMul = std::pow (2.0f, (h.note + ps) / 12.0f);
        decayMul = dm;
        active = true;
    }
    void render (float* out, int n) noexcept override
    {
        using namespace maps;
        const float f1 = rimTone (prm[0]) * toneMul, f2 = f1 * 1.47f;
        const float aC = decayCoef (rimDecay (prm[1]) * decayMul, fs);
        const float nC = decayCoef (rimDecay (prm[1]) * decayMul * 1.7f, fs);
        const float cC = decayCoef (2.5f, fs);
        noiseHp.set (fs, rimBright (prm[6]), 0.8f);
        const float noiseAmt = prm[2], body = prm[3], click = prm[4];
        const float drive = 1.0f + prm[5] * prm[5] * 10.0f;
        const float invFs = (float) (1.0 / fs);
        for (int i = 0; i < n; ++i)
        {
            if (! active) { out[i] = declick.apply (0.0f); continue; }
            ph1 += f1 * invFs; if (ph1 >= 1.0f) ph1 -= 1.0f;
            ph2 += f2 * invFs; if (ph2 >= 1.0f) ph2 -= 1.0f;
            const float tonal = (sin01 (ph1) * (0.35f + 0.65f * body) + sin01 (ph2) * (1.0f - 0.6f * body)) * amp;
            const float nz = noiseHp.hp (rng.next()) * noiseEnv * noiseAmt * 1.6f;
            const float cl = rng.next() * clickEnv * click;
            float y = std::tanh ((tonal + nz + cl) * drive) / std::sqrt (drive) * gain;
            amp *= aC; noiseEnv *= nC; clickEnv *= cC;
            if (amp < 1.0e-4f && noiseEnv < 1.0e-4f) active = false;
            out[i] = lastOut = declick.apply (y);
        }
        noiseHp.flush();
    }

public:
    void resetState() noexcept override
    {
        MonoVoice::resetState();
        ph1 = ph2 = amp = noiseEnv = clickEnv = 0.0f;
        gain = toneMul = decayMul = 1.0f;
        noiseHp.reset();
    }

private:
    float ph1 = 0, ph2 = 0, amp = 0, noiseEnv = 0, clickEnv = 0, gain = 1, toneMul = 1, decayMul = 1;
    Svf noiseHp;
};

// ---------------------------------------------------------------------------
// HATS: 6 osciladores cuadrados inarmónicos + ruido, filtrados (metal crudo)
// ---------------------------------------------------------------------------
class HatVoice : public MonoVoice
{
public:
    explicit HatVoice (bool openHat) : open (openHat) {}
    void prepare (double sr, uint32_t seed) override
    {
        MonoVoice::prepare (sr, seed);
        for (int i = 0; i < 6; ++i) ph[i] = rng.uni();
        lastAir = -100.0f;
    }
    void trigger (const Hit& h) override
    {
        beginHit (h);
        float ps, dm, gm;
        humanize (prm[7], ps, dm, gm);
        // Azar FIJO: la fase de los 6 osciladores también sale de la semilla del golpe
        if (h.fixedSeed) for (int k = 0; k < 6; ++k) ph[k] = hashUnit (hash32 (h.seed, 0x484154u, (uint32_t) k));
        amp = 1.0f;
        att = 0.0f;
        chokeMul = 1.0f;
        gain = velGain (h.vel) * gm * (h.accent ? 1.2f : 1.0f);
        toneMul = std::pow (2.0f, (h.note + ps) / 12.0f);
        decayMul = dm * (h.accent ? 1.15f : 1.0f);
        active = true;
    }
    void choke() override { if (active) choking = true; }
    void render (float* out, int n) noexcept override
    {
        using namespace maps;
        static const float base[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
        const float scale = hatTone (prm[0]) * toneMul;
        const float aC = decayCoef ((open ? hatODecay (prm[1]) : hatCDecay (prm[1])) * decayMul, fs);
        const float chokeC = decayCoef (12.0f, fs);
        const float metal = prm[2];
        const float hpHz = hatFilter (prm[3]);
        bpf.set (fs, std::min (hpHz * 1.35f, (float) fs * 0.45f), hatReso (prm[4]));
        hpf.set (fs, hpHz, 0.7f);
        const float airTarget = airDb (prm[6]);
        if (std::fabs (airTarget - lastAir) > 0.05f) { air.set (Biquad::Type::HighShelf, fs, 9000.0, 0.7, airTarget); lastAir = airTarget; }
        const float drive = 1.0f + prm[5] * prm[5] * 8.0f;
        const float attC = 1.0f - (float) std::exp (-1.0 / (0.0004 * fs));
        float dt[6];
        for (int k = 0; k < 6; ++k) dt[k] = base[k] * scale / (float) fs;
        for (int i = 0; i < n; ++i)
        {
            float m = 0.0f;
            for (int k = 0; k < 6; ++k)
            {
                ph[k] += dt[k]; if (ph[k] >= 1.0f) ph[k] -= 1.0f;
                m += blepSquare (ph[k], dt[k]);
            }
            m *= (1.0f / 6.0f);
            const float src = metal * m + (1.0f - metal) * rng.next() * 0.8f;
            float env = 0.0f;
            if (active)
            {
                att += attC * (1.0f - att);
                env = amp * att * chokeMul;
                amp *= aC;
                if (choking) { chokeMul *= chokeC; if (chokeMul < 1.0e-4f) { active = false; choking = false; } }
                if (amp < 1.0e-4f) active = false;
            }
            float y = hpf.hp (bpf.bp (src) * 1.5f + src * 0.35f) * env;
            y = air.process (y);
            y = std::tanh (y * drive * 2.2f) / std::sqrt (drive) * gain * 0.2f;
            out[i] = lastOut = y;
        }
        bpf.flush(); hpf.flush(); air.flush();
    }

public:
    void resetState() noexcept override
    {
        MonoVoice::resetState();
        choking = false; amp = att = 0.0f; chokeMul = 1.0f;
        gain = toneMul = decayMul = 1.0f;
        for (auto& p : ph) p = 0.0f;
        bpf.reset(); hpf.reset(); air.reset();
        lastAir = -100.0f;
    }

private:
    bool open = false, choking = false;
    float ph[6] {}, amp = 0, att = 0, chokeMul = 1, gain = 1, toneMul = 1, decayMul = 1, lastAir = -100.0f;
    Svf bpf, hpf;
    Biquad air;
};

// ---------------------------------------------------------------------------
// PERC: FM de 2 operadores con envolvente de pitch (congas, toms, bleeps)
// ---------------------------------------------------------------------------
class PercVoice : public MonoVoice
{
public:
    void trigger (const Hit& h) override
    {
        beginHit (h);
        declick.start (lastOut);
        float ps, dm, gm;
        humanize (prm[7], ps, dm, gm);
        pc = pm = 0.0f;
        amp = 1.0f; pe = 1.0f; ie = 1.0f; ne = 1.0f;
        gain = velGain (h.vel) * gm * (h.accent ? 1.15f : 1.0f);
        toneMul = std::pow (2.0f, (h.note + ps) / 12.0f);
        decayMul = dm;
        active = true;
    }
    void render (float* out, int n) noexcept override
    {
        using namespace maps;
        const float f0 = percTone (prm[0]) * toneMul;
        const float dec = percDecay (prm[1]) * decayMul;
        const float aC = decayCoef (dec, fs), iC = decayCoef (dec * 0.45f, fs);
        const float pC = decayCoef (25.0f, fs), nC = decayCoef (5.0f, fs);
        const float index = percFm (prm[2]), ratio = percRatio (prm[3]), pitch = percPitch (prm[4]);
        const float drive = 1.0f + prm[5] * prm[5] * 8.0f;
        const float noise = prm[6];
        const float invFs = (float) (1.0 / fs);
        for (int i = 0; i < n; ++i)
        {
            if (! active) { out[i] = declick.apply (0.0f); continue; }
            const float f = f0 * std::exp2 (pitch * pe / 12.0f);
            pm += f * ratio * invFs; if (pm >= 1.0f) pm -= std::floor (pm);
            const float mod = sin01 (pm) * index * ie;
            pc += f * invFs; if (pc >= 1.0f) pc -= 1.0f;
            float c = pc + mod * 0.159155f; c -= std::floor (c);
            const float y0 = sin01 (c) * amp + rng.next() * ne * noise * 0.7f;
            const float y = std::tanh (y0 * drive) / std::sqrt (drive) * gain;
            amp *= aC; ie *= iC; pe *= pC; ne *= nC;
            if (amp < 1.0e-4f) active = false;
            out[i] = lastOut = declick.apply (y);
        }
    }

public:
    void resetState() noexcept override
    {
        MonoVoice::resetState();
        pc = pm = amp = pe = ie = ne = 0.0f;
        gain = toneMul = decayMul = 1.0f;
    }

private:
    float pc = 0, pm = 0, amp = 0, pe = 0, ie = 0, ne = 0, gain = 1, toneMul = 1, decayMul = 1;
};

// ---------------------------------------------------------------------------
// BAJO: sierra/cuadrada/sub + escalera de 4 polos, acento y glide
// ---------------------------------------------------------------------------
class BassVoice : public MonoVoice
{
public:
    void prepare (double sr, uint32_t seed) override
    {
        MonoVoice::prepare (sr, seed);
        ladder.reset();
        drift.prepare (fs, 0.3f, seed ^ 0xABCDu);
        curNote = targetNote = 36.0f;
        envA = envR = -1.0f;
        setEnv (2.0f, 0.0f, 120.0f);
    }
    void setRoot (int r) { root = r; }
    // Envolvente: ataque (ms), sustain (0..1) y release (ms). Con sustain 0 suena como en la 1.0.
    void setEnv (float attackMs, float sus, float releaseMs)
    {
        if (attackMs != envA) { envA = attackMs; atkC = 1.0f - (float) std::exp (-1.0 / ((double) attackMs * 0.001 * fs)); }
        envS = clampv (sus, 0.0f, 1.0f);
        if (releaseMs != envR) { envR = releaseMs; relC = decayCoef (releaseMs, fs); }
    }
    void release() override { if (envS > 0.0f) released = true; }
    // Fase 3: sub (0..1), forma (0 = la clásica de la 1.0), acento 303 y legato
    void setExtras (float subLevel, int oscType, bool acid303, bool legatoMode)
    {
        sub = clampv (subLevel, 0.0f, 1.0f); osc = oscType; acid = acid303; legato = legatoMode;
    }
    void trigger (const Hit& h) override
    {
        beginHit (h);
        const int oct = maps::bassOctave (prm[0]);
        const float note = 36.0f + (float) root + 12.0f * (float) oct + h.note;
        targetNote = note;
        // Legato: si la nota anterior sigue apretada, se liga sin volver a disparar
        const bool tie = (h.slide || (legato && ! released && gateLeft > 0.0)) && active;
        if (acid)
        {
            // Acento 303: más volumen, más filtro y un "barrido" que se acumula con acentos seguidos
            accentEnv = h.accent ? 1.0f : 0.0f;
            accentSweep = h.accent ? std::min (2.0f, accentSweep * 0.6f + 1.0f) : accentSweep * 0.5f;
            slideNow = h.slide && active;
        }
        if (! tie)
        {
            declick.start (lastOut);
            curNote = note;
            amp = 0.0f;
            ampTarget = 1.0f;
            fenv = 1.0f;
            sustain = 1.0f;
        }
        gateLeft = h.gateSamples > 0.0 ? h.gateSamples : 1.0e18;
        released = false;
        gain = velGain (h.vel) * (h.accent ? (acid ? 1.45f : 1.2f) : 1.0f);
        accent = h.accent ? 1.0f : 0.0f;
        if (tie) fenv = std::max (fenv, 0.6f);
        active = true;
    }
    void render (float* out, int n) noexcept override
    {
        using namespace maps;
        const float dec = synthDecay (prm[1]);
        const float aC = decayCoef (dec, fs), fC = decayCoef (dec * 0.55f, fs);
        // 303: el slide tarda siempre lo mismo (~60 ms escalado por GLIDE), si no hay slide la nota salta
        const float glideMsNow = acid ? (slideNow ? 25.0f + 0.4f * glideMs (prm[7]) : 0.3f) : glideMs (prm[7]);
        const float glideC = 1.0f - (float) std::exp (-1.0 / (glideMsNow * 0.001 * fs));
        const float accC = decayCoef (200.0f, fs);
        const float sus = envS;
        const float driftFixed = fixedRandom ? positionalDrift (randomSeed, 0xBA55u, songSeconds, 0.3f) : 0.0f;
        const float cut = bassCut (prm[2]);
        const float reso = bassReso (prm[3]);
        const float envAmt = envOct (prm[4]) * (1.0f + (acid ? 0.0f : 0.4f * accent));
        const float drive = 1.0f + prm[5] * prm[5] * 6.0f;
        const float wave = prm[6];
        const float fC303 = acid && accent > 0.5f ? decayCoef (200.0f, fs) : fC;   // con acento el filtro cae rápido
        for (int i = 0; i < n; ++i)
        {
            if (! active) { out[i] = declick.apply (0.0f); continue; }
            curNote += glideC * (targetNote - curNote);
            const float dr = fixedRandom ? driftFixed : drift.next();
            const float hz = 440.0f * std::exp2 ((curNote - 69.0f + dr * 0.06f) / 12.0f);
            const float dt = hz / (float) fs;
            ph += dt; if (ph >= 1.0f) ph -= 1.0f;
            phSub += dt * 0.5f; if (phSub >= 1.0f) phSub -= 1.0f;
            float o;
            if (osc == 0)
            {
                const float saw = blepSaw (ph, dt), sq = blepSquare (ph, dt), sq2 = blepSquare (phSub, dt * 0.5f);
                if (wave < 0.5f) o = saw + (sq - saw) * (wave * 2.0f);
                else o = sq * (1.0f - (wave - 0.5f)) + sq2 * (wave - 0.5f) * 1.6f;
            }
            else o = shape (dt);

            if ((i & 15) == 0)
            {
                float fc = cut * std::exp2 (envAmt * fenv);
                if (acid) fc *= std::exp2 (accentEnv * (1.2f + 0.8f * accentSweep));
                ladder.set (fs, std::min (fc, 18000.0f), reso);
            }
            float y = ladder.process (o * 0.9f * drive) / std::sqrt (drive);
            amp += atkC * (ampTarget - amp);
            if (sus > 0.0f)
            {
                if (! released && (gateLeft -= 1.0) <= 0.0) released = true;
                ampTarget = released ? ampTarget * relC : sus + (ampTarget - sus) * aC;
            }
            else ampTarget *= aC;
            fenv *= acid ? fC303 : fC;
            if (acid) { accentEnv *= accC; accentSweep *= 0.99999f; }
            // Sub: seno una octava abajo, limpio (después del filtro)
            if (sub > 0.0f) y += sub * 0.7f * sin01 (phSub) / std::sqrt (drive);
            y *= amp * gain;
            y = std::tanh (y * 1.6f) * 0.9f;
            if (ampTarget < 1.0e-4f && amp < 1.0e-4f) active = false;
            out[i] = lastOut = declick.apply (y);
        }
        ladder.flush();
    }

public:
    void resetState() noexcept override
    {
        MonoVoice::resetState();
        ph = phSub = amp = ampTarget = fenv = 0.0f;
        ph2 = 0.33f; ph3 = 0.71f;
        curNote = targetNote = 36.0f;
        gateLeft = 0.0; released = false;
        accentEnv = accentSweep = 0.0f; slideNow = false;
        ladder.reset();
    }

private:
    // Formas nuevas (ONDA es el timbre de cada una)
    float shape (float dt) noexcept
    {
        const float w = prm[6];
        switch (osc)
        {
            case 1: // pulso: ONDA = ancho (50 % a 5 %)
            {
                const float pw = 0.5f - 0.45f * w;
                float p2 = ph + (1.0f - pw); if (p2 >= 1.0f) p2 -= 1.0f;
                return 0.9f * (blepSaw (ph, dt) - blepSaw (p2, dt));
            }
            case 2: // supersierra: 3 sierras, ONDA = desafinación
            {
                const float d = 1.0f + 0.012f * w;
                ph2 += dt * d; if (ph2 >= 1.0f) ph2 -= 1.0f;
                ph3 += dt / d; if (ph3 >= 1.0f) ph3 -= 1.0f;
                return 0.5f * (blepSaw (ph, dt) + blepSaw (ph2, dt * d) + blepSaw (ph3, dt / d));
            }
            case 3: // triángulo, ONDA lo vuelve más brillante (pliega)
            {
                const float tri = ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph;
                return std::tanh (tri * (1.0f + 3.0f * w)) * 1.1f;
            }
            case 4: // seno FM (modulación de fase consigo mismo): ONDA = índice
                return sin01 (ph + (w * 2.5f / (float) kTwoPi) * sin01 (ph));
            default: return 0.0f;
        }
    }
    int root = 0, osc = 0;
    float sub = 0.0f, ph2 = 0.33f, ph3 = 0.71f, accentEnv = 0.0f, accentSweep = 0.0f;
    bool acid = false, legato = false, slideNow = false;
    float ph = 0, phSub = 0, curNote = 36, targetNote = 36, amp = 0, ampTarget = 0, fenv = 0, sustain = 1, gain = 1, accent = 0;
    float envA = -1.0f, envS = 0.0f, envR = -1.0f, atkC = 0.1f, relC = 0.999f;
    double gateLeft = 0.0;
    bool released = false;
    Ladder ladder;
    SmoothRandom drift;
};

// ---------------------------------------------------------------------------
// ACORDES: polifónico (hasta 5 notas x 2 sierras desafinadas), filtro + chorus
// ---------------------------------------------------------------------------
// Voces estéreo (acordes, melodía, texturas)
class StereoVoice
{
public:
    virtual ~StereoVoice() = default;
    virtual void prepare (double sampleRate, uint32_t seed) = 0;
    virtual void setParams (const float* p) = 0;
    virtual void setSongTime (double seconds, bool fixed, uint32_t s) = 0;
    virtual void trigger (const Hit& h) = 0;
    virtual void release() = 0;
    virtual void render (float* outL, float* outR, int n) noexcept = 0;
    virtual bool isActive() const = 0;
    virtual void resetState() noexcept = 0;
};

class ChordVoice : public StereoVoice
{
public:
    void prepare (double sampleRate, uint32_t seed) override
    {
        fs = sampleRate;
        initSeed = seed;
        rng.seed (seed);
        for (auto& s : slots) { s.active = false; s.fl.reset(); s.fr.reset(); }
        chorusL.prepare ((int) (0.03 * fs));
        chorusR.prepare ((int) (0.03 * fs));
        chorusLfo = 0.0f;
        envR = -1.0f;
        setEnv (0.0f, 300.0f);
        for (int i = 0; i < 10; ++i) drift[i].prepare (fs, 0.2f + 0.05f * (float) i, seed + 17u * (uint32_t) i);
    }
    void setParams (const float* p) override { for (int i = 0; i < kSoundParams; ++i) prm[i] = p[i]; }
    void setRoot (int r) { root = r; }
    void setEnv (float sus, float releaseMs)
    {
        envS = clampv (sus, 0.0f, 1.0f);
        if (releaseMs != envR) { envR = releaseMs; userRelC = decayCoef (releaseMs, fs); }
    }
    void setStrum (float ms) { strumSamples = std::max (0.0, (double) ms * 0.001 * fs); }
    // Notas del próximo acorde (las arma el motor: tonalidad, inversión, voice leading)
    void setNextNotes (const float* notes, int count)
    {
        pendingCount = clampv (count, 1, kMaxChordNotes);
        for (int k = 0; k < pendingCount; ++k) pending[k] = notes[k];
    }
    const float* lastNotes (int& count) const { count = slots[(size_t) cur].count; return slots[(size_t) cur].note; }
    void setSongTime (double seconds, bool fixed, uint32_t s) override { songSeconds = seconds; fixedRandom = fixed; randomSeed = s; }
    bool isActive() const override { return slots[0].active || slots[1].active; }
    void release() override { if (envS > 0.0f) for (auto& s : slots) s.gateLeft = 0.0; }
    void resetState() noexcept override
    {
        for (auto& s : slots) { s.active = s.attackDone = s.release = false; s.amp = s.fenv = 0.0f; s.gateLeft = 0.0; s.fl.reset(); s.fr.reset(); }
        chorusL.clear(); chorusR.clear();
        cur = 0;
        rng.seed (initSeed);
    }

    void trigger (const Hit& h) override
    {
        if (h.fixedSeed) rng.seed (h.seed);
        if (pendingCount <= 0)
        {
            // Sin notas del motor (tests directos): acorde de la pista en paralelo, como la 1.0
            const float* iv = chordIntervals ((int) std::lround (prm[0]), pendingCount);
            for (int k = 0; k < pendingCount; ++k) pending[k] = 48.0f + (float) root + h.note + iv[k];
        }
        const int count = pendingCount;
        pendingCount = 0;
        // LIGADO: si el acorde sigue sonando, cambian las notas sin volver a disparar la envolvente
        Slot& prev = slots[(size_t) cur];
        if (h.slide && prev.active && ! prev.release)
        {
            for (int k = 0; k < count; ++k)
            {
                if (k >= prev.count) { prev.ph[k][0] = rng.uni(); prev.ph[k][1] = rng.uni(); prev.wait[k] = 0.0; prev.fade[k] = 1.0f; }
                prev.note[k] = pending[k];
            }
            prev.count = count;
            prev.gateLeft = h.gateSamples > 0.0 ? h.gateSamples : 1.0e18;
            return;
        }
        cur = 1 - cur;
        Slot& s = slots[(size_t) cur];
        s.count = count;
        s.strum = strumSamples > 0.5;
        for (int k = 0; k < count; ++k)
        {
            s.note[k] = pending[k];
            s.ph[k][0] = rng.uni(); s.ph[k][1] = rng.uni();
            // Strum: cada nota entra un poco después que la anterior (de abajo hacia arriba)
            s.wait[k] = s.strum ? strumSamples * (double) k : 0.0;
            s.fade[k] = s.strum && k > 0 ? 0.0f : 1.0f;
        }
        s.gateLeft = h.gateSamples > 0.0 ? h.gateSamples : 1.0e18;
        s.amp = 0.0f;
        s.ampTarget = 1.0f;
        s.attackDone = false;
        s.fenv = 0.0f;
        s.gain = velGain (h.vel) * (h.accent ? 1.15f : 1.0f);
        s.active = true;
        // La voz anterior se apaga rápido para no embarrar
        Slot& o = slots[(size_t) (1 - cur)];
        if (o.active) o.release = true;
    }

    void render (float* outL, float* outR, int n) noexcept override
    {
        using namespace maps;
        const float dec = synthDecay (prm[1]);
        const float fadeInc = (float) (1.0 / (0.003 * fs));
        const float aC = decayCoef (dec, fs), fC = decayCoef (dec * 0.6f, fs), relC = decayCoef (90.0f, fs);
        const float atkMs = attackMs (prm[7]);
        const float atkC = 1.0f - (float) std::exp (-1.0 / (atkMs * 0.001 * fs));
        const float cut = chordCut (prm[2]);
        const float q = 0.7f + chordReso (prm[3]) * 5.0f;
        const float envAmt = chordEnvOct (prm[4]);
        const float det = detuneCents (prm[5]);
        const float chorus = prm[6];
        const float lfoInc = (float) (0.55 / fs);
        const float sus = envS;
        float driftFixed[10] {};
        if (fixedRandom)
        {
            for (int k = 0; k < 10; ++k) driftFixed[k] = positionalDrift (randomSeed, 0xC40Du + (uint32_t) k, songSeconds, 0.2f + 0.05f * (float) k);
            const double ph = songSeconds * 0.55;
            chorusLfo = (float) (ph - std::floor (ph));
        }

        for (int i = 0; i < n; ++i)
        {
            float l = 0.0f, r = 0.0f;
            for (auto& s : slots)
            {
                if (! s.active) continue;
                if (! s.attackDone)
                {
                    s.amp += atkC * (1.0f - s.amp);
                    s.fenv += atkC * (1.0f - s.fenv);
                    if (s.amp > 0.97f) s.attackDone = true;
                }
                else if (sus > 0.0f && ! s.release)
                {
                    if (s.gateLeft > 0.0) { s.gateLeft -= 1.0; s.amp = sus + (s.amp - sus) * aC; }
                    else s.amp *= userRelC;
                    s.fenv *= fC;
                }
                else
                {
                    s.amp *= s.release ? relC : aC;
                    s.fenv *= fC;
                }
                if ((i & 15) == 0)
                {
                    const float fc = std::min (cut * std::exp2 (envAmt * s.fenv), 17000.0f);
                    s.fl.set (fs, fc, q);
                    s.fr.set (fs, fc, q);
                }
                float sl = 0.0f, sr = 0.0f;
                for (int k = 0; k < s.count; ++k)
                {
                    float noteGain = 1.0f;
                    if (s.strum)
                    {
                        if (s.wait[k] > 0.0) { s.wait[k] -= 1.0; continue; }
                        if (s.fade[k] < 1.0f) s.fade[k] = std::min (1.0f, s.fade[k] + fadeInc);
                        noteGain = s.fade[k];
                    }
                    for (int o = 0; o < 2; ++o)
                    {
                        const float dr = fixedRandom ? driftFixed[(k * 2 + o) % 10] : drift[(k * 2 + o) % 10].next();
                        const float cents = (o == 0 ? -det : det) + dr * 4.0f;
                        if ((i & 31) == 0)
                            s.inc[k][o] = 440.0f * std::exp2 ((s.note[k] - 69.0f + cents / 100.0f) / 12.0f) / (float) fs;
                        float& p = s.ph[k][o];
                        p += s.inc[k][o]; if (p >= 1.0f) p -= 1.0f;
                        const float v = s.strum ? blepSaw (p, s.inc[k][o]) * noteGain : blepSaw (p, s.inc[k][o]);
                        if (o == 0) sl += v; else sr += v;
                    }
                }
                const float g = s.amp * s.gain * 0.2f;
                l += s.fl.lp (sl * g);
                r += s.fr.lp (sr * g);
                if (s.amp < 1.0e-4f && s.attackDone) { s.active = false; s.release = false; }
            }
            // Chorus estéreo (dos líneas moduladas en contrafase)
            chorusLfo += lfoInc; if (chorusLfo >= 1.0f) chorusLfo -= 1.0f;
            const float m = sin01 (chorusLfo);
            chorusL.push (l); chorusR.push (r);
            const float dL = (float) fs * (0.0055f + 0.0022f * m);
            const float dR = (float) fs * (0.0055f - 0.0022f * m);
            const float cl = chorusL.read (dL), cr = chorusR.read (dR);
            outL[i] = std::tanh ((l + chorus * (cr - 0.3f * l)) * 1.1f);
            outR[i] = std::tanh ((r + chorus * (cl - 0.3f * r)) * 1.1f);
        }
        for (auto& s : slots) { s.fl.flush(); s.fr.flush(); }
    }

private:
    struct Slot
    {
        bool active = false, attackDone = false, release = false, strum = false;
        int count = 0;
        float note[kMaxChordNotes] {}, ph[kMaxChordNotes][2] {}, inc[kMaxChordNotes][2] {}, fade[kMaxChordNotes] {};
        double wait[kMaxChordNotes] {};
        float amp = 0, ampTarget = 0, fenv = 0, gain = 1;
        double gateLeft = 0.0;
        Svf fl, fr;
    };
    double fs = 44100.0, songSeconds = 0.0;
    bool fixedRandom = false;
    uint32_t randomSeed = 1, initSeed = 1;
    float envS = 0.0f, envR = -1.0f, userRelC = 0.999f;
    float prm[kSoundParams] {};
    int root = 0, cur = 0, pendingCount = 0;
    float pending[kMaxChordNotes] {};
    double strumSamples = 0.0;
    Slot slots[2];
    Noise rng;
    DelayLine chorusL, chorusR;
    float chorusLfo = 0.0f;
    SmoothRandom drift[10];
};

// ---------------------------------------------------------------------------
// MELODÍA: 4 voces (o mono / mono legato) con cuatro modelos de síntesis limpia:
//   PLUCK   cuerda pulsada (Karplus-Strong): TIMBRE = apagado de la cuerda
//   LEAD    2 sierras desafinadas + sub, filtro con envolvente: TIMBRE = desafinación
//   CAMPANA FM de 2 operadores: TIMBRE = relación (de 1 a 7), ENV = índice
//   ORGANO  6 armónicos (drawbars): TIMBRE = brillo, ENV = click de tecla
// ---------------------------------------------------------------------------
class MelodyVoice : public StereoVoice
{
public:
    static constexpr int kVoices = 4;
    static constexpr int kKs = 4096;

    void prepare (double sampleRate, uint32_t seed) override
    {
        fs = sampleRate;
        initSeed = seed;
        rng.seed (seed);
        ks.assign ((size_t) (kVoices * kKs), 0.0f);
        envR = envA = -1.0f;
        setEnv (2.0f, 0.0f, 300.0f);
        resetState();
    }
    void setParams (const float* p) override { for (int i = 0; i < kSoundParams; ++i) prm[i] = p[i]; }
    void setSongTime (double seconds, bool fixed, uint32_t s) override { songSeconds = seconds; fixedRandom = fixed; randomSeed = s; }
    void setEnv (float attackMs, float sus, float releaseMs)
    {
        if (attackMs != envA) { envA = attackMs; atkC = 1.0f - (float) std::exp (-1.0 / ((double) attackMs * 0.001 * fs)); }
        envS = clampv (sus, 0.0f, 1.0f);
        if (releaseMs != envR) { envR = releaseMs; relC = decayCoef (releaseMs, fs); }
    }
    // 0 = poly, 1 = mono, 2 = mono legato; base = nota MIDI de la tónica en la octava elegida
    void setMode (int voicesMode, float baseNote) { mode = voicesMode; base = baseNote; }
    bool isActive() const override
    {
        for (const auto& v : voice) if (v.active) return true;
        return false;
    }
    void release() override
    {
        if (envS <= 0.0f) return;
        for (auto& v : voice) v.gateLeft = 0.0;
    }
    void resetState() noexcept override
    {
        for (auto& v : voice) v = Voice();
        std::fill (ks.begin(), ks.end(), 0.0f);
        rng.seed (initSeed);
        age = 0;
        last = 0;
    }

    void trigger (const Hit& h) override
    {
        if (h.fixedSeed) rng.seed (h.seed);
        const float note = base + h.note;
        const int model = clampv ((int) std::lround (prm[0]), 0, 3);
        // Elegir voz: mono usa siempre la 0; poly, una libre o la más vieja
        int vi = 0;
        bool tie = false;
        if (mode == 0)
        {
            if (h.slide && voice[(size_t) last].active && ! voice[(size_t) last].releasing) { vi = last; tie = true; }
            else
            {
                int oldest = 0;
                vi = -1;
                for (int k = 0; k < kVoices; ++k)
                {
                    if (! voice[(size_t) k].active) { vi = k; break; }
                    if (voice[(size_t) k].age < voice[(size_t) oldest].age) oldest = k;
                }
                if (vi < 0) vi = oldest;
            }
        }
        else
        {
            const auto& v0 = voice[0];
            tie = v0.active && ! v0.releasing && (h.slide || (mode == 2 && v0.gateLeft > 0.0));
        }
        Voice& v = voice[(size_t) vi];
        v.target = note;
        v.gateLeft = h.gateSamples > 0.0 ? h.gateSamples : 1.0e18;
        v.releasing = false;
        if (tie) return;   // ligado: cambia la altura con GLIDE, sin volver a disparar
        if (! v.active || mode == 0) v.note = note;   // en mono, el GLIDE arranca desde la nota anterior
        v.active = true;
        v.attack = true;
        v.amp = 0.0f;
        v.fenv = 1.0f;
        v.fm = 1.0f;
        v.click = 1.0f;
        v.time = 0.0f;
        v.vel = clampv (h.vel, 0.0f, 1.0f);
        v.gain = velGain (h.vel) * (h.accent ? 1.15f : 1.0f);
        v.age = ++age;
        v.pan = mode == 0 ? (vi % 2 == 0 ? -0.22f : 0.22f) * (vi < 2 ? 1.0f : 0.5f) : 0.0f;
        for (int o = 0; o < 6; ++o) v.ph[o] = model == 3 ? rng.uni() : 0.0f;
        v.filt.reset();
        last = vi;
        if (model == 0) pluck (vi, note);
    }

    void render (float* outL, float* outR, int n) noexcept override
    {
        using namespace maps;
        const int model = clampv ((int) std::lround (prm[0]), 0, 3);
        const float dec = synthDecay (prm[1]);
        const float decC = decayCoef (dec, fs), fC = decayCoef (dec * 0.45f, fs), fmC = decayCoef (dec * 0.3f, fs);
        const float clickC = decayCoef (4.0f, fs);
        const float cut = melCut (prm[2]);
        const float q = 0.7f + melReso (prm[3]) * 5.0f;
        const float envAmt = melEnvOct (prm[4]);
        const float timbre = prm[5];
        const float vib = vibratoCents (prm[6]);
        const float glideC = 1.0f - (float) std::exp (-1.0 / (glideMs (prm[7]) * 0.001 * fs));
        const float sus = envS;
        const float dt = (float) (1.0 / fs);
        // Cuerda: apagado y largo (el sonido de la cuerda dura lo que DECAY)
        const float damp = 0.05f + 0.45f * timbre;
        const float ratio = 1.0f + 6.0f * timbre;
        float harm[6];
        {
            const float t = timbre;
            harm[0] = 1.0f; harm[1] = 0.2f + 0.8f * t; harm[2] = 0.7f * t; harm[3] = 0.6f * t * t; harm[4] = 0.45f * t * t; harm[5] = 0.35f * t * t * t;
        }
        static const float harmMul[6] = { 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f };
        for (int i = 0; i < n; ++i) { outL[i] = 0.0f; outR[i] = 0.0f; }

        for (int vi = 0; vi < kVoices; ++vi)
        {
            Voice& v = voice[(size_t) vi];
            if (! v.active) continue;
            float* buf = ks.data() + (size_t) vi * kKs;
            const float gl = std::cos ((v.pan + 1.0f) * 0.25f * (float) kPi) * 1.4142f;
            const float gr = std::sin ((v.pan + 1.0f) * 0.25f * (float) kPi) * 1.4142f;
            for (int i = 0; i < n; ++i)
            {
                v.note += glideC * (v.target - v.note);
                v.time += dt;
                // Vibrato que entra de a poco
                v.vibPh += 5.2f * dt; if (v.vibPh >= 1.0f) v.vibPh -= 1.0f;
                if ((i & 7) == 0)
                {
                    const float vibAmt = vib * std::min (1.0f, std::max (0.0f, (v.time - 0.15f) * 3.0f));
                    const float pitch = v.note + vibAmt * 0.01f * sin01 (v.vibPh);
                    v.hz = 440.0f * std::exp2 ((pitch - 69.0f) / 12.0f);
                    v.loss = std::pow (0.001f, 1.0f / std::max (0.05f, dec * 0.001f * v.hz));
                }
                const float hz = v.hz;
                const float inc = hz * dt;

                // Envolvente ADSR
                if (v.attack) { v.amp += atkC * (1.05f - v.amp); if (v.amp >= 1.0f) { v.amp = 1.0f; v.attack = false; } }
                else if (sus > 0.0f && ! v.releasing)
                {
                    if ((v.gateLeft -= 1.0) <= 0.0) v.releasing = true;
                    v.amp = sus + (v.amp - sus) * decC;
                }
                else v.amp *= v.releasing && sus > 0.0f ? relC : decC;
                v.fenv *= fC;
                v.fm *= fmC;

                float y = 0.0f;
                switch (model)
                {
                    case 0: // cuerda pulsada
                    {
                        const float period = clampv ((float) fs / hz - damp, 2.0f, (float) kKs - 4.0f);
                        float rp = (float) v.w - period;
                        if (rp < 0.0f) rp += (float) kKs;
                        const int i0 = (int) rp;
                        const float fr = rp - (float) i0;
                        const float a = buf[i0 % kKs], b = buf[(i0 + 1) % kKs];
                        const float x = a + (b - a) * fr;
                        const float filtered = x * (1.0f - damp) + v.ksLast * damp;
                        v.ksLast = x;
                        buf[v.w] = filtered * v.loss;
                        v.w = (v.w + 1) % kKs;
                        y = filtered;
                        break;
                    }
                    case 1: // lead
                    {
                        const float d = 1.0f + 0.015f * timbre;
                        v.ph[0] += inc * d; if (v.ph[0] >= 1.0f) v.ph[0] -= 1.0f;
                        v.ph[1] += inc / d; if (v.ph[1] >= 1.0f) v.ph[1] -= 1.0f;
                        v.ph[2] += inc * 0.5f; if (v.ph[2] >= 1.0f) v.ph[2] -= 1.0f;
                        y = 0.4f * (blepSaw (v.ph[0], inc * d) + blepSaw (v.ph[1], inc / d)) + 0.3f * sin01 (v.ph[2]);
                        break;
                    }
                    case 2: // campana FM
                    {
                        v.ph[0] += inc; if (v.ph[0] >= 1.0f) v.ph[0] -= 1.0f;
                        v.ph[1] += inc * ratio; if (v.ph[1] >= 1.0f) v.ph[1] -= 1.0f;
                        v.ph[2] += inc * 2.76f; if (v.ph[2] >= 1.0f) v.ph[2] -= 1.0f;
                        const float index = (0.3f + 4.0f * prm[4]) * v.fm;
                        y = sin01 (v.ph[0] + index * sin01 (v.ph[1]) / (float) kTwoPi) + 0.25f * v.fm * sin01 (v.ph[2]);
                        y *= 0.7f;
                        break;
                    }
                    default: // órgano: armónicos por recurrencia de Chebyshev (2 senos por muestra en vez de 6)
                    {
                        v.ph[0] += inc; if (v.ph[0] >= 1.0f) v.ph[0] -= 1.0f;
                        const float s1 = sin01 (v.ph[0]), c2 = 2.0f * sin01 (v.ph[0] + 0.25f);
                        float sPrev = 0.0f, sCur = s1;
                        y = harm[0] * s1;
                        int h = 1;
                        for (int n = 2; n <= 8; ++n)
                        {
                            const float sNext = c2 * sCur - sPrev;
                            sPrev = sCur; sCur = sNext;
                            if (n == (int) harmMul[h])
                            {
                                if (hz * (float) n < (float) fs * 0.45f) y += harm[h] * sCur;
                                ++h;
                            }
                        }
                        y *= 0.32f;
                        y += v.click * prm[4] * 0.3f * rng.next();
                        v.click *= clickC;
                        break;
                    }
                }
                // Filtro con envolvente (la campana pasa casi abierta)
                if ((i & 15) == 0)
                {
                    const float vb = std::exp2 ((v.vel - 0.7f) * 0.8f);
                    const float fc = std::min (cut * vb * std::exp2 ((model == 2 ? 0.0f : envAmt) * v.fenv), 18000.0f);
                    v.filt.set (fs, fc, q);
                }
                y = v.filt.lp (y) * v.amp * v.gain;
                outL[i] += y * gl;
                outR[i] += y * gr;
                if (! v.attack && v.amp < 1.0e-4f) { v.active = false; v.ksLast = 0.0f; break; }
            }
            v.filt.flush();
        }
        for (int i = 0; i < n; ++i) { outL[i] = std::tanh (outL[i] * 0.9f); outR[i] = std::tanh (outR[i] * 0.9f); }
    }

private:
    struct Voice
    {
        bool active = false, attack = false, releasing = false;
        float note = 60.0f, target = 60.0f, amp = 0.0f, fenv = 0.0f, fm = 0.0f, click = 0.0f, gain = 1.0f, vel = 0.8f, pan = 0.0f;
        float ph[6] {};
        float vibPh = 0.0f, time = 0.0f, ksLast = 0.0f, hz = 440.0f, loss = 0.999f;
        double gateLeft = 0.0;
        int w = 0;
        uint32_t age = 0;
        Svf filt;
    };
    // Excita la cuerda: ráfaga de ruido filtrada (CORTE = brillo de la púa)
    void pluck (int vi, float note)
    {
        Voice& v = voice[(size_t) vi];
        float* buf = ks.data() + (size_t) vi * kKs;
        const float hz = 440.0f * std::exp2 ((note - 69.0f) / 12.0f);
        const int period = clampv ((int) ((float) fs / hz) + 2, 4, kKs - 4);
        const float bright = clampv (prm[2], 0.05f, 1.0f);
        float lp = 0.0f;
        for (int k = period; k >= 1; --k)
        {
            lp += bright * (rng.next() - lp);
            const int idx = ((v.w - k) % kKs + kKs) % kKs;
            buf[idx] += lp * 0.9f;
        }
    }

    double fs = 44100.0, songSeconds = 0.0;
    bool fixedRandom = false;
    uint32_t randomSeed = 1, initSeed = 1, age = 0;
    float prm[kSoundParams] {};
    float envA = -1.0f, envS = 0.0f, envR = -1.0f, atkC = 0.1f, relC = 0.999f, base = 69.0f;
    int mode = 0, last = 0;
    Voice voice[kVoices];
    std::vector<float> ks;
    Noise rng;
};

} // namespace ogro2
