// EVO SOL — detector de pitch YIN sobre señal decimada (bajo costo de CPU)
#pragma once

#include "Common.h"

namespace evo
{
class PitchDetector
{
public:
    struct Estimate
    {
        bool voiced = false;
        float periodSamples = 0.0f; // período a la frecuencia de muestreo original
        float hz = 0.0f;
        float clarity = 0.0f;       // 0..1 (1 = muy periódico)
        float rmsDb = -120.0f;
    };

    void prepare (double sampleRate, float absoluteMinHz = 55.0f, float absoluteMaxHz = 1300.0f)
    {
        fs = sampleRate;
        decim = std::max (1, (int) std::lround (fs / 22050.0));
        fsd = fs / decim;
        absMinHz = absoluteMinHz;
        absMaxHz = absoluteMaxHz;

        const int tauMaxAbs = (int) std::ceil (fsd / absMinHz) + 2;
        const int wMax = std::max (tauMaxAbs, (int) std::lround (0.02 * fsd));
        int size = 1;
        while (size < wMax + tauMaxAbs + 8) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        diff.assign ((size_t) tauMaxAbs + 4, 0.0f);
        cmnd.assign ((size_t) tauMaxAbs + 4, 0.0f);
        lin.assign ((size_t) (wMax + tauMaxAbs + 8), 0.0f);

        hop = std::max (16, (int) std::lround (0.0055 * fsd)); // ~5.5 ms
        setRange (80.0f, 1000.0f);
        reset();
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        lp1.reset(); lp2.reset();
        w = 0; decimCounter = 0; hopCounter = 0;
        est = Estimate();
        hist[0] = hist[1] = hist[2] = 0.0f;
        histCount = 0;
    }

    void setRange (float minHz, float maxHz)
    {
        minHz = clampv (minHz, absMinHz, absMaxHz * 0.5f);
        maxHz = clampv (maxHz, minHz * 2.0f, absMaxHz);
        tauMin = std::max (2, (int) std::floor (fsd / maxHz));
        tauMax = std::min ((int) diff.size() - 3, (int) std::ceil (fsd / minHz));
        win = std::max (tauMax, (int) std::lround (0.02 * fsd));
        const double lpHz = std::min ((double) maxHz * 1.6, fsd * 0.45);
        // Butterworth de 4º orden = 2 biquads (Q 0.541 y 1.307)
        lp1.set (Biquad::Type::LowPass, fs, lpHz, 0.5412);
        lp2.set (Biquad::Type::LowPass, fs, lpHz, 1.3066);
    }

    void setThreshold (float t) { threshold = clampv (t, 0.03f, 0.45f); }
    void setGateDb (float db) { gateDb = db; }

    // Devuelve true cuando hay una estimación nueva
    inline bool push (float x) noexcept
    {
        const float y = lp2.process (lp1.process (x));
        if (++decimCounter < decim) return false;
        decimCounter = 0;
        buf[(size_t) w] = y;
        w = (w + 1) & mask;
        if (++hopCounter < hop) return false;
        hopCounter = 0;
        analyse();
        return true;
    }

    const Estimate& estimate() const noexcept { return est; }
    int decimation() const noexcept { return decim; }
    float hopSeconds() const noexcept { return (float) (hop / fsd); }
    // Retardo aproximado (muestras originales) entre la entrada y el centro del análisis
    int analysisDelaySamples() const noexcept { return (int) ((win + tauMax) * 0.5 * decim); }

    void flush() { lp1.flush(); lp2.flush(); }

private:
    void analyse() noexcept
    {
        const int total = win + tauMax;
        // Copia lineal del final del buffer circular
        for (int i = 0; i < total; ++i)
            lin[(size_t) i] = buf[(size_t) ((w - total + i) & mask)];

        // Energía de la ventana
        double e = 0.0;
        for (int i = 0; i < win; ++i) e += (double) lin[(size_t) i] * lin[(size_t) i];
        const float rms = (float) std::sqrt (e / win);
        est.rmsDb = gainToDb (rms);

        if (est.rmsDb < gateDb)
        {
            setUnvoiced();
            return;
        }

        // Función diferencia d(tau)
        const float* x = lin.data();
        diff[0] = 0.0f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float s = 0.0f;
            const float* xt = x + tau;
            for (int j = 0; j < win; ++j)
            {
                const float d = x[j] - xt[j];
                s += d * d;
            }
            diff[(size_t) tau] = s;
        }

        // Diferencia normalizada acumulativa (CMND)
        cmnd[0] = 1.0f;
        double running = 0.0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            running += diff[(size_t) tau];
            cmnd[(size_t) tau] = running > 0.0 ? (float) (diff[(size_t) tau] * tau / running) : 1.0f;
        }

        // Primer mínimo bajo el umbral (evita errores de octava)
        int tauEst = -1;
        for (int tau = tauMin; tau <= tauMax; ++tau)
        {
            if (cmnd[(size_t) tau] < threshold)
            {
                while (tau + 1 <= tauMax && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau]) ++tau;
                tauEst = tau;
                break;
            }
        }

        float minVal = 1.0f;
        if (tauEst < 0)
        {
            // Mínimo global como respaldo
            for (int tau = tauMin; tau <= tauMax; ++tau)
                if (cmnd[(size_t) tau] < minVal) { minVal = cmnd[(size_t) tau]; tauEst = tau; }
            const float exitThresh = est.voiced ? threshold + 0.12f : threshold + 0.04f;
            if (minVal > exitThresh || tauEst < 0) { setUnvoiced(); return; }
        }
        else
        {
            minVal = cmnd[(size_t) tauEst];
        }

        // Interpolación parabólica para precisión sub-muestra
        float refined = (float) tauEst;
        if (tauEst > 1 && tauEst < tauMax)
        {
            const float a = cmnd[(size_t) tauEst - 1], b = cmnd[(size_t) tauEst], c = cmnd[(size_t) tauEst + 1];
            const float den = a - 2.0f * b + c;
            if (std::fabs (den) > 1.0e-9f)
                refined += clampv (0.5f * (a - c) / den, -0.5f, 0.5f);
        }

        float period = refined * (float) decim;

        // Mediana de 3 para eliminar outliers aislados
        hist[histCount % 3] = period;
        ++histCount;
        if (histCount >= 3)
        {
            float a = hist[0], b = hist[1], c = hist[2];
            const float med = std::max (std::min (a, b), std::min (std::max (a, b), c));
            // Si el valor nuevo salta más de 1/3 de octava respecto de la mediana, usamos la mediana
            if (std::fabs (std::log2 (period / med)) > 0.33f) period = med;
        }

        est.voiced = true;
        est.periodSamples = period;
        est.hz = (float) (fs / period);
        est.clarity = clampv (1.0f - minVal, 0.0f, 1.0f);
    }

    void setUnvoiced() noexcept
    {
        est.voiced = false;
        est.clarity = 0.0f;
        histCount = 0;
    }

    double fs = 44100.0, fsd = 22050.0;
    int decim = 2;
    float absMinHz = 55.0f, absMaxHz = 1300.0f;
    std::vector<float> buf, diff, cmnd, lin;
    int mask = 0, w = 0, decimCounter = 0, hop = 128, hopCounter = 0;
    int tauMin = 2, tauMax = 100, win = 400;
    float threshold = 0.15f, gateDb = -50.0f;
    Biquad lp1, lp2;
    Estimate est;
    float hist[3] {};
    int histCount = 0;
};

} // namespace evo
