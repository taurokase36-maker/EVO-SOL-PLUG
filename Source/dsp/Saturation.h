// EVO SOL — saturación analógica (válvula + cinta) y clipper suave,
// procesados con sobremuestreo 4x para evitar aliasing.
#pragma once

#include "Common.h"

namespace evo
{
// Sobremuestreo 4x con FIR de fase lineal (ventana Kaiser).
// Latencia total (subida + bajada) = 32 muestras a la frecuencia original.
class Oversampler4x
{
public:
    static constexpr int kFactor = 4;
    static constexpr int kTaps = 129;
    static constexpr int kPhaseTaps = (kTaps + kFactor - 1) / kFactor; // 33

    Oversampler4x() { design(); reset(); }

    static constexpr int latencySamples() { return (kTaps - 1) / kFactor; } // 32

    void reset()
    {
        std::fill (std::begin (upHist), std::end (upHist), 0.0f);
        std::fill (std::begin (downHist), std::end (downHist), 0.0f);
        upPos = downPos = 0;
    }

    // Sube una muestra a 4 muestras
    inline void up (float x, float* out4) noexcept
    {
        upPos = (upPos + kPhaseTaps - 1) % kPhaseTaps;
        upHist[upPos] = x;
        upHist[upPos + kPhaseTaps] = x; // espejo para lectura sin módulo
        for (int p = 0; p < kFactor; ++p)
        {
            const float* h = poly[p];
            const float* xs = upHist + upPos;
            float acc = 0.0f;
            for (int j = 0; j < kPhaseTaps; ++j) acc += h[j] * xs[j];
            out4[p] = acc;
        }
    }

    // Baja 4 muestras a una
    inline float down (const float* in4) noexcept
    {
        // La salida se toma en la fase 0 para que el retardo total sea entero (32 muestras)
        float acc = 0.0f;
        for (int p = 0; p < kFactor; ++p)
        {
            downPos = (downPos + kTaps - 1) % kTaps;
            downHist[downPos] = in4[p];
            downHist[downPos + kTaps] = in4[p];
            if (p == 0)
            {
                const float* xs = downHist + downPos;
                for (int j = 0; j < kTaps; ++j) acc += coeffs[j] * xs[j];
            }
        }
        return acc;
    }

private:
    static double besselI0 (double x)
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 30; ++k)
        {
            term *= (x / (2.0 * k)) * (x / (2.0 * k));
            sum += term;
        }
        return sum;
    }

    void design()
    {
        const double fc = 0.115; // normalizado a la tasa 4x (≈ 0.46 de la tasa original)
        const double beta = 7.8;
        const double mid = (kTaps - 1) * 0.5;
        double sum = 0.0;
        for (int i = 0; i < kTaps; ++i)
        {
            const double t = i - mid;
            const double sinc = t == 0.0 ? 2.0 * fc : std::sin (kTwoPi * fc * t) / (kPi * t);
            const double r = t / mid;
            const double w = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / besselI0 (beta);
            coeffs[i] = (float) (sinc * w);
            sum += coeffs[i];
        }
        for (int i = 0; i < kTaps; ++i) coeffs[i] = (float) (coeffs[i] / sum);
        // Polifase para la subida (ganancia x4 para compensar los ceros insertados)
        for (int p = 0; p < kFactor; ++p)
            for (int j = 0; j < kPhaseTaps; ++j)
            {
                const int idx = p + j * kFactor;
                poly[p][j] = idx < kTaps ? coeffs[idx] * (float) kFactor : 0.0f;
            }
    }

    float coeffs[kTaps] {};
    float poly[kFactor][kPhaseTaps] {};
    float upHist[2 * kPhaseTaps] {};
    float downHist[2 * kTaps] {};
    int upPos = 0, downPos = 0;
};

// Curvas de saturación
inline float tubeCurve (float x, float bias) noexcept
{
    // Asimétrica: agrega 2º armónico (calidez de válvula)
    return std::tanh (x + bias) - std::tanh (bias);
}
inline float tapeCurve (float x) noexcept
{
    // Simétrica y suave: 3er armónico, compresión de picos tipo cinta
    return x / std::sqrt (1.0f + x * x * 0.9f);
}
inline float softClip (float u) noexcept
{
    const float t = 0.72f;
    const float a = std::fabs (u);
    if (a <= t) return u;
    const float s = t + (1.0f - t) * std::tanh ((a - t) / (1.0f - t));
    return u < 0.0f ? -s : s;
}

// Saturador analógico mono (válvula + cinta) sobremuestreado
class AnalogSaturator
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        os.reset();
        dc.prepare (fs);
        // Pre/de-énfasis tipo cinta: satura un poco más los agudos
        preEmph.set (Biquad::Type::HighShelf, fs * 4.0, 6000.0, 0.7, 3.0);
        deEmph.set (Biquad::Type::HighShelf, fs * 4.0, 6000.0, 0.7, -3.0);
        drive.reset (fs, 30.0f, 0.0f);
    }
    static constexpr int latencySamples() { return Oversampler4x::latencySamples(); }

    void setDrive (float d) { drive.setTarget (clampv (d, 0.0f, 1.0f)); }

    inline float process (float x) noexcept
    {
        float up4[4];
        os.up (x, up4);
        const float d = drive.next();
        const float pre = 1.0f + 5.0f * d;
        const float wet = std::min (1.0f, d * 1.6f);
        const float comp = 1.0f / std::pow (pre, 0.75f);
        for (float& s : up4)
        {
            const float e = preEmph.process (s) * pre;
            float y = 0.6f * tubeCurve (e, 0.25f) + 0.4f * tapeCurve (e);
            y = deEmph.process (y) * comp;
            s = s + wet * (y - s);
        }
        return dc.process (os.down (up4));
    }

    void flush() { preEmph.flush(); deEmph.flush(); }

private:
    double fs = 44100.0;
    Oversampler4x os;
    Biquad preEmph, deEmph;
    DcBlocker dc;
    SmoothedValue drive;
};

// Clipper suave sobremuestreado (antes del limitador final)
class OversampledClipper
{
public:
    void reset() { os.reset(); }
    static constexpr int latencySamples() { return Oversampler4x::latencySamples(); }
    void setThreshold (float lin) { thr = std::max (0.05f, lin); }

    inline float process (float x) noexcept
    {
        float up4[4];
        os.up (x, up4);
        const float inv = 1.0f / thr;
        for (float& s : up4) s = thr * softClip (s * inv);
        return os.down (up4);
    }

private:
    Oversampler4x os;
    float thr = 1.0f;
};

} // namespace evo
