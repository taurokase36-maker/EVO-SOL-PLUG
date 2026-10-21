// OGRO 2 (copia propia del DSP de EVO SOL) — utilidades DSP comunes (sin dependencias de JUCE)
#pragma once

#include <algorithm>
#include <array>
#include <initializer_list>
#include <functional>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace evo2
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g, float floorDb = -120.0f) noexcept
{
    return g > 1.0e-6f ? 20.0f * std::log10 (g) : floorDb;
}

template <typename T>
inline T clampv (T v, T lo, T hi) noexcept { return v < lo ? lo : (v > hi ? hi : v); }

inline float sanitize (float x) noexcept
{
    // Elimina NaN/Inf y denormales para que nada explote en el host
    if (! std::isfinite (x)) return 0.0f;
    return std::fabs (x) < 1.0e-20f ? 0.0f : x;
}

// Coeficiente de un filtro de un polo para una constante de tiempo en ms
inline float onePoleCoef (float ms, double fs) noexcept
{
    if (ms <= 0.0f) return 1.0f;
    return 1.0f - (float) std::exp (-1.0 / (0.001 * ms * fs));
}

// Suavizador lineal de parámetros (evita "zipper noise")
class SmoothedValue
{
public:
    void reset (double fs, float rampMs, float initial)
    {
        steps = std::max (1, (int) (fs * 0.001 * rampMs));
        current = target = initial;
        remaining = 0;
    }
    void setTarget (float t)
    {
        if (t == target) return;
        target = t;
        remaining = steps;
        inc = (target - current) / (float) steps;
    }
    inline float next() noexcept
    {
        if (remaining > 0)
        {
            current += inc;
            if (--remaining == 0) current = target;
        }
        return current;
    }
    float get() const noexcept { return current; }
    bool isSmoothing() const noexcept { return remaining > 0; }

private:
    float current = 0.0f, target = 0.0f, inc = 0.0f;
    int steps = 1, remaining = 0;
};

// Biquad RBJ (Direct Form II transpuesta)
class Biquad
{
public:
    enum class Type { LowPass, HighPass, BandPass, Peak, LowShelf, HighShelf };

    void set (Type type, double fs, double freq, double q, double gainDb = 0.0)
    {
        freq = clampv (freq, 10.0, fs * 0.49);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = kTwoPi * freq / fs;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);
        double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;

        switch (type)
        {
            case Type::LowPass:
                b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2;
                a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case Type::HighPass:
                b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2;
                a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case Type::BandPass:
                b0 = alpha; b1 = 0; b2 = -alpha;
                a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case Type::Peak:
                b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
                a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A; break;
            case Type::LowShelf:
            {
                const double sq = 2 * std::sqrt (A) * alpha;
                b0 = A * ((A + 1) - (A - 1) * cw + sq);
                b1 = 2 * A * ((A - 1) - (A + 1) * cw);
                b2 = A * ((A + 1) - (A - 1) * cw - sq);
                a0 = (A + 1) + (A - 1) * cw + sq;
                a1 = -2 * ((A - 1) + (A + 1) * cw);
                a2 = (A + 1) + (A - 1) * cw - sq; break;
            }
            case Type::HighShelf:
            {
                const double sq = 2 * std::sqrt (A) * alpha;
                b0 = A * ((A + 1) + (A - 1) * cw + sq);
                b1 = -2 * A * ((A - 1) + (A + 1) * cw);
                b2 = A * ((A + 1) + (A - 1) * cw - sq);
                a0 = (A + 1) - (A - 1) * cw + sq;
                a1 = 2 * ((A - 1) - (A + 1) * cw);
                a2 = (A + 1) - (A - 1) * cw - sq; break;
            }
        }
        cb0 = (float) (b0 / a0); cb1 = (float) (b1 / a0); cb2 = (float) (b2 / a0);
        ca1 = (float) (a1 / a0); ca2 = (float) (a2 / a0);
    }

    void reset() noexcept { z1 = z2 = 0.0f; }

    inline float process (float x) noexcept
    {
        const float y = cb0 * x + z1;
        z1 = cb1 * x - ca1 * y + z2;
        z2 = cb2 * x - ca2 * y;
        return y;
    }

    // Flush de denormales en estados internos (llamar una vez por bloque)
    void flush() noexcept
    {
        if (std::fabs (z1) < 1.0e-20f) z1 = 0.0f;
        if (std::fabs (z2) < 1.0e-20f) z2 = 0.0f;
        if (! std::isfinite (z1) || ! std::isfinite (z2)) reset();
    }

private:
    float cb0 = 1, cb1 = 0, cb2 = 0, ca1 = 0, ca2 = 0;
    float z1 = 0, z2 = 0;
};

// Filtro de un polo pasa-bajos / pasa-altos (barato, para colorear)
class OnePole
{
public:
    void setLowpass (double fs, double fc)
    {
        a = (float) std::exp (-kTwoPi * clampv (fc, 1.0, fs * 0.49) / fs);
    }
    void reset() noexcept { z = 0.0f; }
    inline float lp (float x) noexcept { z = x + a * (z - x); return z; }
    inline float hp (float x) noexcept { return x - lp (x); }

private:
    float a = 0.0f, z = 0.0f;
};

// Bloqueador de DC
class DcBlocker
{
public:
    void prepare (double fs) { r = (float) std::exp (-kTwoPi * 12.0 / fs); x1 = y1 = 0.0f; }
    inline float process (float x) noexcept
    {
        const float y = x - x1 + r * y1;
        x1 = x; y1 = std::fabs (y) < 1.0e-20f ? 0.0f : y;
        return y;
    }

private:
    float r = 0.999f, x1 = 0.0f, y1 = 0.0f;
};

// Línea de retardo con lectura fraccional (Hermite de 4 puntos)
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        int size = 1;
        while (size < maxSamples + 4) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void push (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }

    // Lee "d" muestras hacia atrás desde la última escrita (d >= 1)
    inline float read (float d) const noexcept
    {
        const float pos = (float) w - 1.0f - d;
        int i = (int) std::floor (pos);
        const float f = pos - (float) i;
        const float xm1 = buf[(size_t) ((i - 1) & mask)];
        const float x0 = buf[(size_t) (i & mask)];
        const float x1 = buf[(size_t) ((i + 1) & mask)];
        const float x2 = buf[(size_t) ((i + 2) & mask)];
        const float c = (x1 - xm1) * 0.5f;
        const float v = x0 - x1;
        const float wv = c + v;
        const float a = wv + v + (x2 - x0) * 0.5f;
        const float b = wv + a;
        return ((a * f - b) * f + c) * f + x0;
    }
    inline float readInt (int d) const noexcept { return buf[(size_t) ((w - 1 - d) & mask)]; }

private:
    std::vector<float> buf;
    int mask = 0, w = 0;
};

// Ruido suave (random walk filtrado) para modulaciones orgánicas
class SmoothRandom
{
public:
    void prepare (double fs, float rateHz, uint32_t seed)
    {
        state = seed ? seed : 1u;
        period = std::max (1, (int) (fs / std::max (0.01f, rateHz)));
        counter = 0;
        from = to = 0.0f;
        to = nextRand();
    }
    inline float next() noexcept
    {
        if (++counter >= period) { counter = 0; from = to; to = nextRand(); }
        const float t = (float) counter / (float) period;
        const float s = t * t * (3.0f - 2.0f * t); // smoothstep
        return from + (to - from) * s;
    }

private:
    float nextRand() noexcept
    {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return (float) (state & 0xFFFFFF) / (float) 0x7FFFFF - 1.0f; // [-1, 1]
    }
    uint32_t state = 1;
    int period = 1, counter = 0;
    float from = 0, to = 0;
};

// Interpolación cúbica Hermite sobre un buffer circular (potencia de 2)
inline float hermiteRead (const float* buf, int mask, double pos) noexcept
{
    const double fl = std::floor (pos);
    const int i = (int) (int64_t) fl;
    const float f = (float) (pos - fl);
    const float xm1 = buf[(i - 1) & mask];
    const float x0 = buf[i & mask];
    const float x1 = buf[(i + 1) & mask];
    const float x2 = buf[(i + 2) & mask];
    const float c = (x1 - xm1) * 0.5f;
    const float v = x0 - x1;
    const float wv = c + v;
    const float a = wv + v + (x2 - x0) * 0.5f;
    const float b = wv + a;
    return ((a * f - b) * f + c) * f + x0;
}

} // namespace evo2
