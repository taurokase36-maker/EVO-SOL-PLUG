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
        rng.seed (seed);
        declick.prepare (fs);
    }
    void setParams (const float* p) { for (int i = 0; i < kSoundParams; ++i) prm[i] = p[i]; }
    virtual void trigger (const Hit& h) = 0;
    virtual void render (float* out, int n) noexcept = 0;
    virtual void choke() {}
    bool isActive() const { return active; }
    float lastOut = 0.0f;

protected:
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
        float ps, dm, gm;
        humanize (prm[7], ps, dm, gm);
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
    }
    void setRoot (int r) { root = r; }
    void trigger (const Hit& h) override
    {
        const int oct = maps::bassOctave (prm[0]);
        const float note = 36.0f + (float) root + 12.0f * (float) oct + h.note;
        targetNote = note;
        if (! (h.slide && active))
        {
            declick.start (lastOut);
            curNote = note;
            amp = 0.0f;
            ampTarget = 1.0f;
            fenv = 1.0f;
            sustain = 1.0f;
        }
        gain = velGain (h.vel) * (h.accent ? 1.2f : 1.0f);
        accent = h.accent ? 1.0f : 0.0f;
        if (h.slide && active) fenv = std::max (fenv, 0.6f);
        active = true;
    }
    void render (float* out, int n) noexcept override
    {
        using namespace maps;
        const float dec = synthDecay (prm[1]);
        const float aC = decayCoef (dec, fs), fC = decayCoef (dec * 0.55f, fs);
        const float glideC = 1.0f - (float) std::exp (-1.0 / (glideMs (prm[7]) * 0.001 * fs));
        const float cut = bassCut (prm[2]);
        const float reso = bassReso (prm[3]);
        const float envAmt = envOct (prm[4]) * (1.0f + 0.4f * accent);
        const float drive = 1.0f + prm[5] * prm[5] * 6.0f;
        const float wave = prm[6];
        const float atkC = 1.0f - (float) std::exp (-1.0 / (0.002 * fs));
        for (int i = 0; i < n; ++i)
        {
            if (! active) { out[i] = declick.apply (0.0f); continue; }
            curNote += glideC * (targetNote - curNote);
            const float hz = 440.0f * std::exp2 ((curNote - 69.0f + drift.next() * 0.06f) / 12.0f);
            const float dt = hz / (float) fs;
            ph += dt; if (ph >= 1.0f) ph -= 1.0f;
            phSub += dt * 0.5f; if (phSub >= 1.0f) phSub -= 1.0f;
            const float saw = blepSaw (ph, dt), sq = blepSquare (ph, dt), sub = blepSquare (phSub, dt * 0.5f);
            float osc;
            if (wave < 0.5f) osc = saw + (sq - saw) * (wave * 2.0f);
            else osc = sq * (1.0f - (wave - 0.5f)) + sub * (wave - 0.5f) * 1.6f;

            if ((i & 15) == 0)
                ladder.set (fs, std::min (cut * std::exp2 (envAmt * fenv), 18000.0f), reso);
            float y = ladder.process (osc * 0.9f * drive) / std::sqrt (drive);
            amp += atkC * (ampTarget - amp);
            ampTarget *= aC;
            fenv *= fC;
            y *= amp * gain;
            y = std::tanh (y * 1.6f) * 0.9f;
            if (ampTarget < 1.0e-4f && amp < 1.0e-4f) active = false;
            out[i] = lastOut = declick.apply (y);
        }
        ladder.flush();
    }

private:
    int root = 0;
    float ph = 0, phSub = 0, curNote = 36, targetNote = 36, amp = 0, ampTarget = 0, fenv = 0, sustain = 1, gain = 1, accent = 0;
    Ladder ladder;
    SmoothRandom drift;
};

// ---------------------------------------------------------------------------
// ACORDES: polifónico (hasta 5 notas x 2 sierras desafinadas), filtro + chorus
// ---------------------------------------------------------------------------
class ChordVoice
{
public:
    void prepare (double sampleRate, uint32_t seed)
    {
        fs = sampleRate;
        rng.seed (seed);
        for (auto& s : slots) { s.active = false; s.fl.reset(); s.fr.reset(); }
        chorusL.prepare ((int) (0.03 * fs));
        chorusR.prepare ((int) (0.03 * fs));
        chorusLfo = 0.0f;
        for (int i = 0; i < 10; ++i) drift[i].prepare (fs, 0.2f + 0.05f * (float) i, seed + 17u * (uint32_t) i);
    }
    void setParams (const float* p) { for (int i = 0; i < kSoundParams; ++i) prm[i] = p[i]; }
    void setRoot (int r) { root = r; }
    bool isActive() const { return slots[0].active || slots[1].active; }

    void trigger (const Hit& h)
    {
        cur = 1 - cur;
        Slot& s = slots[(size_t) cur];
        int count = 0;
        const float* iv = chordIntervals (maps::chordType (prm[0]), count);
        s.count = count;
        const float rootNote = 48.0f + (float) root + h.note;
        for (int k = 0; k < count; ++k)
        {
            s.note[k] = rootNote + iv[k];
            s.ph[k][0] = rng.uni(); s.ph[k][1] = rng.uni();
        }
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

    void render (float* outL, float* outR, int n) noexcept
    {
        using namespace maps;
        const float dec = synthDecay (prm[1]);
        const float aC = decayCoef (dec, fs), fC = decayCoef (dec * 0.6f, fs), relC = decayCoef (90.0f, fs);
        const float atkMs = attackMs (prm[7]);
        const float atkC = 1.0f - (float) std::exp (-1.0 / (atkMs * 0.001 * fs));
        const float cut = chordCut (prm[2]);
        const float q = 0.7f + chordReso (prm[3]) * 5.0f;
        const float envAmt = chordEnvOct (prm[4]);
        const float det = detuneCents (prm[5]);
        const float chorus = prm[6];
        const float lfoInc = (float) (0.55 / fs);

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
                    for (int o = 0; o < 2; ++o)
                    {
                        const float cents = (o == 0 ? -det : det) + drift[(k * 2 + o) % 10].next() * 4.0f;
                        if ((i & 31) == 0)
                            s.inc[k][o] = 440.0f * std::exp2 ((s.note[k] - 69.0f + cents / 100.0f) / 12.0f) / (float) fs;
                        float& p = s.ph[k][o];
                        p += s.inc[k][o]; if (p >= 1.0f) p -= 1.0f;
                        const float v = blepSaw (p, s.inc[k][o]);
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
        bool active = false, attackDone = false, release = false;
        int count = 0;
        float note[5] {}, ph[5][2] {}, inc[5][2] {};
        float amp = 0, ampTarget = 0, fenv = 0, gain = 1;
        Svf fl, fr;
    };
    double fs = 44100.0;
    float prm[kSoundParams] {};
    int root = 0, cur = 0;
    Slot slots[2];
    Noise rng;
    DelayLine chorusL, chorusR;
    float chorusLfo = 0.0f;
    SmoothRandom drift[10];
};

} // namespace ogro2
