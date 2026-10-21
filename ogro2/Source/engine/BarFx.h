// OGRO 2 — efectos de fin de compás: stutter, barrido de filtro, tiros de reverb y delay,
// tape stop, bitcrush y riser de ruido. Se disparan en los últimos pasos de cada N compases
// (o a mano) y cada uno actúa en un lugar: master, bus de batería, bajo y acordes o una pista.
// Los buffers se reservan en prepare(): en el audio no se pide memoria.
#pragma once

#include "Voices.h"
#include <vector>

namespace ogro2
{
enum BfxKind { BfxStutter = 0, BfxFilter, BfxReverb, BfxDelay, BfxTapeStop, BfxCrush, BfxRiser, BfxKindCount };
// Lugares donde puede actuar (el índice coincide con la lista de destinos)
enum BfxPoint { PointMaster = 0, PointDrums, PointSynths, PointTrack0, PointCount = PointTrack0 + kTracks };

class BarFx
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        // Stutter: una semicorchea a 30 BPM; tape stop: una negra a 30 BPM
        stutterL.assign ((size_t) (fs * 0.55) + 8, 0.0f); stutterR = stutterL;
        tapeL.assign ((size_t) (fs * 2.1) + 8, 0.0f); tapeR = tapeL;
        fade = std::max (16, (int) (fs * 0.0015));
        reset();
    }

    void reset() noexcept
    {
        for (auto& f : fx) f = Fx();
        for (auto& m : pointMask) m = 0u;
        filtL.reset(); filtR.reset(); riserL.reset(); riserR.reset();
        delayBoostV = 0.0f;
    }

    // Arranca un efecto en "point" por "lengthSamples" (slice: largo del stutter)
    void start (int kind, int point, double lengthSamples, float amount, double sliceSamples, uint32_t seed) noexcept
    {
        if (kind < 0 || kind >= BfxKindCount) return;
        auto& f = fx[kind];
        if (f.on) pointMask[f.point] &= ~(1u << kind);
        f.on = true;
        f.point = clampv (point, 0, PointCount - 1);
        f.len = std::max (1.0, lengthSamples);
        f.elapsed = 0;
        f.amount = clampv (amount, 0.0f, 1.0f);
        f.slice = clampv ((int) sliceSamples, 32, (int) stutterL.size() - 4);
        f.hold = 0; f.holdL = f.holdR = 0.0f;
        f.readPos = 0.0;
        f.ctl = 0;
        if (kind == BfxFilter) { filtL.reset(); filtR.reset(); }
        if (kind == BfxRiser) { riserL.reset(); riserR.reset(); noiseL.seed (seed | 1u); noiseR.seed ((seed * 2654435761u) | 1u); }
        pointMask[f.point] |= 1u << kind;
    }

    bool any() const noexcept { return (activeBits() | (delayBoostV > 1e-4f ? 1u : 0u)) != 0u; }
    uint32_t activeBits() const noexcept
    {
        uint32_t b = 0;
        for (int k = 0; k < BfxKindCount; ++k) b |= fx[k].on ? (1u << k) : 0u;
        return b;
    }
    bool pointActive (int point) const noexcept { return pointMask[point] != 0u; }
    float delayBoost() const noexcept { return delayBoostV; }

    // Cuánto se manda al reverb / delay desde ese lugar (tiros)
    float reverbSend (int point) const noexcept { return sendOf (BfxReverb, point); }
    float delaySend (int point) const noexcept { return sendOf (BfxDelay, point); }

    // Procesa la señal de un lugar con los efectos que actúan ahí
    inline void process (int point, float& l, float& r) noexcept
    {
        const uint32_t m = pointMask[point];
        if (m == 0u) return;
        if (m & (1u << BfxStutter)) stutter (l, r);
        if (m & (1u << BfxTapeStop)) tapeStop (l, r);
        if (m & (1u << BfxFilter)) filter (l, r);
        if (m & (1u << BfxCrush)) crush (l, r);
        if (m & (1u << BfxRiser)) riser (l, r);
    }

    // Una muestra más (después de procesar todos los lugares)
    inline void advance() noexcept
    {
        for (int k = 0; k < BfxKindCount; ++k)
        {
            auto& f = fx[k];
            if (! f.on) continue;
            if (k == BfxDelay) delayBoostV = std::max (delayBoostV, 0.32f * f.amount * (float) progress (f));
            if (++f.elapsed >= (int64_t) f.len) { f.on = false; pointMask[f.point] &= ~(1u << k); }
        }
        if (! fx[BfxDelay].on && delayBoostV > 0.0f) delayBoostV = delayBoostV > 1e-4f ? delayBoostV * boostDecay() : 0.0f;
    }

private:
    struct Fx
    {
        bool on = false;
        int point = 0;
        double len = 1.0;
        int64_t elapsed = 0;
        float amount = 0.0f;
        int slice = 1024;
        int hold = 0;
        float holdL = 0.0f, holdR = 0.0f;
        double readPos = 0.0;
        int ctl = 0;
    };

    static double progress (const Fx& f) noexcept { return clampv ((double) f.elapsed / f.len, 0.0, 1.0); }
    // Fundido de entrada y salida (sin clicks al cortar en el uno del compás)
    float edge (const Fx& f) const noexcept
    {
        const double left = f.len - (double) f.elapsed;
        return (float) clampv (std::min ((double) f.elapsed, left) / (double) fade, 0.0, 1.0);
    }
    float sendOf (int kind, int point) const noexcept
    {
        const auto& f = fx[kind];
        if (! f.on || f.point != point) return 0.0f;
        const float ramp = (float) std::min (1.0, progress (f) * 4.0);
        return f.amount * ramp;
    }
    float boostDecay() const noexcept { return (float) std::exp (-1.0 / (1.2 * fs)); }

    inline void stutter (float& l, float& r) noexcept
    {
        auto& f = fx[BfxStutter];
        const float e = edge (f) * f.amount;
        if (f.elapsed < f.slice)
        {
            stutterL[(size_t) f.elapsed] = l; stutterR[(size_t) f.elapsed] = r;
            return;   // la primera tajada pasa como está mientras se graba
        }
        const int idx = (int) ((f.elapsed - f.slice) % f.slice);
        // Ventana corta en cada repetición: corta limpio, sin click
        const float w = clampv (std::min ((float) idx, (float) (f.slice - idx)) / 48.0f, 0.0f, 1.0f);
        const float sl = stutterL[(size_t) idx] * w, sr = stutterR[(size_t) idx] * w;
        l += e * (sl - l);
        r += e * (sr - r);
    }

    inline void tapeStop (float& l, float& r) noexcept
    {
        auto& f = fx[BfxTapeStop];
        const size_t n = tapeL.size();
        const size_t w = (size_t) f.elapsed % n;
        tapeL[w] = l; tapeR[w] = r;
        const double p = progress (f);
        const double rate = std::max (0.0, 1.0 - (double) f.amount * std::pow (p, 0.7));
        f.readPos = std::min (f.readPos + rate, (double) f.elapsed);
        const size_t i0 = (size_t) f.readPos % n, i1 = (i0 + 1) % n;
        const float fr = (float) (f.readPos - std::floor (f.readPos));
        const float g = 0.35f + 0.65f * (float) rate;
        const float tl = (tapeL[i0] + (tapeL[i1] - tapeL[i0]) * fr) * g;
        const float tr = (tapeR[i0] + (tapeR[i1] - tapeR[i0]) * fr) * g;
        const float e = edge (f);
        l += e * (tl - l);
        r += e * (tr - r);
    }

    inline void filter (float& l, float& r) noexcept
    {
        auto& f = fx[BfxFilter];
        if (f.ctl-- <= 0)
        {
            f.ctl = 16;
            const double p = progress (f);
            const float hz = 20000.0f * (float) std::pow (260.0 / 20000.0, (double) f.amount * std::pow (p, 1.4));
            const float q = 0.8f + 2.4f * f.amount * (float) p;
            filtL.set (fs, hz, q); filtR.set (fs, hz, q);
        }
        const float e = edge (f);
        const float fl = filtL.lp (l), fr = filtR.lp (r);
        l += e * (fl - l);
        r += e * (fr - r);
    }

    inline void crush (float& l, float& r) noexcept
    {
        auto& f = fx[BfxCrush];
        const float p = (float) progress (f);
        const float depth = f.amount * std::min (1.0f, 0.25f + p);
        const int factor = 1 + (int) (depth * 22.0f);
        if (f.hold-- <= 0) { f.hold = factor - 1; f.holdL = l; f.holdR = r; }
        const float q = std::pow (2.0f, 15.0f - depth * 11.0f);
        const float cl = std::round (f.holdL * q) / q, cr = std::round (f.holdR * q) / q;
        const float e = edge (f);
        l += e * (cl - l);
        r += e * (cr - r);
    }

    inline void riser (float& l, float& r) noexcept
    {
        auto& f = fx[BfxRiser];
        const float p = (float) progress (f);
        if (f.ctl-- <= 0)
        {
            f.ctl = 16;
            const float hz = 500.0f * std::pow (18.0f, p);
            riserL.set (fs, hz, 1.6f); riserR.set (fs, hz * 1.03f, 1.6f);
        }
        const float g = f.amount * 0.16f * p * p * edge (f);
        l += riserL.bp (noiseL.next()) * g;
        r += riserR.bp (noiseR.next()) * g;
    }

    double fs = 44100.0;
    int fade = 64;
    Fx fx[BfxKindCount];
    uint32_t pointMask[PointCount] {};
    std::vector<float> stutterL, stutterR, tapeL, tapeR;
    Svf filtL, filtR, riserL, riserR;
    Noise noiseL { 0x52495345u }, noiseR { 0x52495346u };
    float delayBoostV = 0.0f;
};

} // namespace ogro2
