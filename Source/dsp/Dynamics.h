// EVO SOL — dinámica: compresor FET, compresor óptico, de-esser adaptativo,
// rider vocal (auto-ganancia) y limitador con lookahead.
#pragma once

#include "Common.h"

namespace evo
{
// Curva de compresión con rodilla suave (todo en dB). Devuelve reducción >= 0.
inline float gainComputerDb (float inDb, float thresh, float ratio, float knee) noexcept
{
    const float over = inDb - thresh;
    if (knee > 0.0f && 2.0f * std::fabs (over) <= knee)
    {
        const float t = over + knee * 0.5f;
        return (1.0f - 1.0f / ratio) * t * t / (2.0f * knee);
    }
    return over > 0.0f ? over * (1.0f - 1.0f / ratio) : 0.0f;
}

// --------------------------------------------------------------------------
// Compresor estilo FET (rápido, agresivo, con color armónico al comprimir)
// --------------------------------------------------------------------------
class FetCompressor
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        aAtt = onePoleCoef (0.35f, fs);
        setRelease (90.0f);
        state = 0.0f;
        dc.prepare (fs);
    }
    void setThreshold (float db) { thresh = db; }
    void setRatio (float r) { ratio = std::max (1.0f, r); }
    void setRelease (float ms) { aRelFast = onePoleCoef (ms * 0.5f, fs); aRelSlow = onePoleCoef (ms * 3.0f, fs); }
    void setColor (float c) { color = clampv (c, 0.0f, 1.0f); }
    float getReductionDb() const noexcept { return state; }

    inline float process (float x) noexcept
    {
        const float inDb = gainToDb (std::fabs (x), -120.0f);
        const float want = gainComputerDb (inDb, thresh, ratio, 6.0f);
        // Release dependiente del programa: transitorios cortos liberan rápido
        if (want > state) state += aAtt * (want - state);
        else
        {
            const float a = (state - want) > 6.0f ? aRelFast : aRelSlow;
            state += a * (want - state);
        }
        float y = x * dbToGain (-state);
        // Color FET: saturación asimétrica suave proporcional a la compresión
        const float drive = color * (0.15f + clampv (state / 18.0f, 0.0f, 1.0f) * 0.85f);
        if (drive > 1.0e-3f)
        {
            const float k = 1.0f + 2.5f * drive;
            const float s = std::tanh (k * (y + 0.08f * drive)) - std::tanh (k * 0.08f * drive);
            y = dc.process (y + drive * (s / k - y));
        }
        return y;
    }

private:
    double fs = 44100.0;
    float thresh = -18.0f, ratio = 4.0f, color = 0.5f;
    float aAtt = 0.5f, aRelFast = 0.01f, aRelSlow = 0.001f, state = 0.0f;
    DcBlocker dc;
};

// --------------------------------------------------------------------------
// Compresor óptico (suave, nivelador, release en dos etapas con "memoria")
// --------------------------------------------------------------------------
class OptoCompressor
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        aDet = onePoleCoef (4.0f, fs);
        aAtt = onePoleCoef (12.0f, fs);
        aMem = onePoleCoef (400.0f, fs);
        aMemRel = onePoleCoef (2500.0f, fs);
        relFastMs = 70.0f; relSlowMs = 1200.0f;
        aRelFast = onePoleCoef (relFastMs, fs);
        aRelSlow = onePoleCoef (relSlowMs, fs);
        sq = state = mem = 0.0f;
    }
    void setThreshold (float db) { thresh = db; }
    void setRatio (float r) { ratio = std::max (1.0f, r); }
    float getReductionDb() const noexcept { return state; }

    inline float process (float x) noexcept
    {
        sq += aDet * (x * x - sq);
        const float inDb = 10.0f * std::log10 (sq + 1.0e-12f) + 3.0f;
        const float want = gainComputerDb (inDb, thresh, ratio, 10.0f);
        if (want > state) state += aAtt * (want - state);
        else
        {
            // Cuanto más tiempo comprimió, más lento libera (comportamiento de célula óptica)
            const float m = clampv (mem / 6.0f, 0.0f, 1.0f);
            const float a = aRelFast + (aRelSlow - aRelFast) * m;
            state += a * (want - state);
        }
        mem += (state > mem ? aMem : aMemRel) * (state - mem);
        return x * dbToGain (-state);
    }

private:
    double fs = 44100.0;
    float thresh = -20.0f, ratio = 3.0f;
    float aDet = 0.1f, aAtt = 0.1f, aMem = 0.001f, aMemRel = 0.0001f;
    float relFastMs = 70.0f, relSlowMs = 1200.0f, aRelFast = 0.01f, aRelSlow = 0.001f;
    float sq = 0.0f, state = 0.0f, mem = 0.0f;
};

// --------------------------------------------------------------------------
// De-esser adaptativo: compara la energía de sibilancia con la de toda la voz
// y aprende el brillo "normal" del cantante, así funciona igual a cualquier nivel.
// --------------------------------------------------------------------------
class AdaptiveDeEsser
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        split.set (Biquad::Type::HighPass, fs, 5200.0, 0.707);
        aFastA = onePoleCoef (0.5f, fs);
        aFastR = onePoleCoef (35.0f, fs);
        aAvg = onePoleCoef (3000.0f, fs);
        aGain = onePoleCoef (2.0f, fs);
        aGainRel = onePoleCoef (40.0f, fs);
        envHf = envFull = 0.0f;
        avgRatio = 0.12f;
        gain = 1.0f;
    }
    void setAmount (float a) { amount = clampv (a, 0.0f, 1.0f); }
    float getReductionDb() const noexcept { return -gainToDb (gain, -40.0f); }
    float learnedBrightness() const noexcept { return avgRatio; }

    inline float process (float x) noexcept
    {
        const float hf = split.process (x);
        const float ah = std::fabs (hf), af = std::fabs (x);
        envHf += (ah > envHf ? aFastA : aFastR) * (ah - envHf);
        envFull += (af > envFull ? aFastA : aFastR) * (af - envFull);

        float targetGain = 1.0f;
        if (envFull > 1.0e-4f)
        {
            const float ratio = envHf / envFull;
            // Aprende la relación "normal" solo en material no sibilante
            if (ratio < avgRatio * 2.0f) avgRatio += aAvg * (ratio - avgRatio);
            avgRatio = clampv (avgRatio, 0.03f, 0.6f);
            const float threshRatio = avgRatio * (3.2f - 2.0f * amount);
            if (amount > 0.0f && ratio > threshRatio)
            {
                const float overDb = gainToDb (ratio / threshRatio);
                const float redDb = std::min (14.0f * amount, overDb * (0.4f + 0.6f * amount));
                targetGain = dbToGain (-redDb);
            }
        }
        gain += (targetGain < gain ? aGain : aGainRel) * (targetGain - gain);
        // Solo atenúa la banda alta: low + hf*g = x + hf*(g-1)
        return x + hf * (gain - 1.0f);
    }

    void flush() { split.flush(); }

private:
    double fs = 44100.0;
    Biquad split;
    float amount = 0.5f;
    float aFastA = 0.1f, aFastR = 0.01f, aAvg = 0.0001f, aGain = 0.1f, aGainRel = 0.01f;
    float envHf = 0.0f, envFull = 0.0f, avgRatio = 0.12f, gain = 1.0f;
};

// --------------------------------------------------------------------------
// Rider vocal: lleva la voz a un nivel objetivo antes de la cadena (como un
// ingeniero moviendo el fader), sin subir ruido en los silencios.
// --------------------------------------------------------------------------
class VocalRider
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        aRms = onePoleCoef (60.0f, fs);
        aUp = onePoleCoef (250.0f, fs);
        aDown = onePoleCoef (90.0f, fs);
        ms = 0.0f;
        gainDb = 0.0f;
    }
    void setTargetDb (float t) { target = t; }
    void setRangeDb (float r) { range = std::max (0.0f, r); }
    void setEnabled (bool e) { enabled = e; }
    float getGainDb() const noexcept { return gainDb; }

    inline float process (float x) noexcept
    {
        ms += aRms * (x * x - ms);
        const float levelDb = 10.0f * std::log10 (ms + 1.0e-12f);
        float want = gainDb;
        if (! enabled) want = 0.0f;
        else if (levelDb > -52.0f) // compuerta: no sube ruido ni respiraciones bajas
            want = clampv (target - levelDb, -range, range);
        gainDb += (want > gainDb ? aUp : aDown) * (want - gainDb);
        return x * dbToGain (gainDb);
    }

private:
    double fs = 44100.0;
    float aRms = 0.01f, aUp = 0.001f, aDown = 0.001f, ms = 0.0f, gainDb = 0.0f;
    float target = -20.0f, range = 10.0f;
    bool enabled = true;
};

// --------------------------------------------------------------------------
// Limitador estéreo con lookahead (no deja pasar picos sobre el techo)
// --------------------------------------------------------------------------
class LookaheadLimiter
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        look = std::max (1, (int) std::lround (0.0015 * fs));
        dl[0].prepare (look + 4); dl[1].prepare (look + 4);
        gHist.assign ((size_t) look + 1, 1.0f);
        maBuf.assign ((size_t) look, 1.0);
        maSum = (double) look;
        maPos = histPos = 0;
        minDeque.assign ((size_t) (look + 2) * 2, 0);
        dqHead = dqTail = 0;
        counter = 0;
        rel = onePoleCoef (80.0f, fs);
        gOut = 1.0f;
    }
    int getLatencySamples() const noexcept { return look; }
    void setCeilingDb (float db) { ceiling = dbToGain (db); }
    void setReleaseMs (float ms) { rel = onePoleCoef (ms, fs); }
    float getReductionDb() const noexcept { return -gainToDb (gOut, -40.0f); }

    inline void process (float& l, float& r) noexcept
    {
        const float peak = std::max (std::fabs (l), std::fabs (r));
        const float gReq = peak > ceiling ? ceiling / peak : 1.0f;

        // Mínimo deslizante sobre la ventana de lookahead
        const int64_t idx = counter++;
        gHist[(size_t) (idx % (int64_t) gHist.size())] = gReq;
        pushMin (idx, gReq);
        while (dqHead != dqTail && dequeFront() <= idx - (int64_t) gHist.size()) popFront();
        const float gMin = gHist[(size_t) (dequeFront() % (int64_t) gHist.size())];

        // Promedio móvil: rampa de ataque suave que llega justo a tiempo al pico
        maSum += (double) gMin - maBuf[(size_t) maPos];
        maBuf[(size_t) maPos] = (double) gMin;
        maPos = (maPos + 1) % look;
        const float gMa = (float) (maSum / look);

        gOut = gMa < gOut ? gMa : gOut + rel * (gMa - gOut);

        dl[0].push (l); dl[1].push (r);
        const float dlL = dl[0].readInt (look);
        const float dlR = dl[1].readInt (look);
        l = clampv (dlL * gOut, -ceiling, ceiling);
        r = clampv (dlR * gOut, -ceiling, ceiling);
    }

private:
    // Deque monotónica implementada sobre un vector circular
    void pushMin (int64_t idx, float v)
    {
        while (dqHead != dqTail)
        {
            const int64_t back = minDeque[(size_t) ((dqTail - 1 + (int) minDeque.size()) % (int) minDeque.size())];
            if (gHist[(size_t) (back % (int64_t) gHist.size())] >= v) dqTail = (dqTail - 1 + (int) minDeque.size()) % (int) minDeque.size();
            else break;
        }
        minDeque[(size_t) dqTail] = idx;
        dqTail = (dqTail + 1) % (int) minDeque.size();
    }
    int64_t dequeFront() const { return minDeque[(size_t) dqHead]; }
    void popFront() { if (dqHead != dqTail) dqHead = (dqHead + 1) % (int) minDeque.size(); }

    double fs = 44100.0;
    int look = 64;
    DelayLine dl[2];
    std::vector<float> gHist;
    std::vector<double> maBuf;
    double maSum = 0.0;
    int maPos = 0, histPos = 0;
    std::vector<int64_t> minDeque;
    int dqHead = 0, dqTail = 0;
    int64_t counter = 0;
    float ceiling = 0.89f, rel = 0.001f, gOut = 1.0f;
};

} // namespace evo
