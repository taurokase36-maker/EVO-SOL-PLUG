// EVO SOL — motor de afinación: corrección en key/escala + PSOLA con formantes
// y dos voces de armonía diatónica que comparten el mismo análisis.
#pragma once

#include "Common.h"
#include "PitchDetector.h"
#include "Scales.h"

namespace evo
{
enum class HarmonyInterval : int { Off = 0, OctDown, FifthDown, ThirdDown, ThirdUp, FifthUp, OctUp, Num };

inline const char* const* harmonyNames()
{
    static const char* names[] = { "Apagada", "Octava abajo", "5ta abajo", "3ra abajo",
                                   "3ra arriba", "5ta arriba", "Octava arriba" };
    return names;
}

enum class VoiceRange : int { Soprano = 0, AltoTenor, Low, Num };

inline const char* const* voiceRangeNames()
{
    static const char* names[] = { "Soprano", "Alto / Tenor", "Grave" };
    return names;
}

class PitchEngine
{
public:
    static constexpr int kVoices = 3; // 0 = principal, 1-2 = armonías

    struct Params
    {
        bool correctionOn = true;
        int key = 0;                       // 0 = C ... 11 = B
        ScaleType scale = ScaleType::Chromatic;
        float retuneMs = 20.0f;            // 0 = efecto robot
        float humanize = 0.0f;             // 0..1
        float flexTune = 0.0f;             // 0..1
        float vibrato = 0.0f;              // -1..+1 (−100% = sin vibrato, +100% = doble)
        float throatSemis = 0.0f;          // −4..+4 (formantes)
        float transposeSemis = 0.0f;       // −12..+12
        float detuneCents = 0.0f;          // referencia (A4 = 440 Hz ± cents)
        float tracking = 0.5f;             // 0 = permisivo, 1 = estricto
        VoiceRange range = VoiceRange::AltoTenor;
        HarmonyInterval harmony[2] { HarmonyInterval::Off, HarmonyInterval::Off };
        bool learnKey = false;
    };

    struct Meter
    {
        bool voiced = false;
        float inputMidi = 0.0f;   // pitch detectado (MIDI continuo)
        float outputMidi = 0.0f;  // pitch corregido
        int targetNote = -1;      // nota objetivo
    };

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        fs = sampleRate;
        detector.prepare (fs, kAbsMinHz, 1300.0f);

        tMax = (int) std::ceil (fs / kAbsMinHz);
        tUnvoiced = std::max (64, (int) std::lround (0.006 * fs));
        horizon = (int) std::ceil (1.2 * tMax) + 4;
        const double phiMin = std::pow (2.0, -kMaxThroat / 12.0);
        maxOutHalf = (int) std::ceil (tMax / phiMin) + 2;
        latency = horizon + maxOutHalf + tMax / 2 + 8;

        int size = 1;
        while (size < 4 * (latency + 2 * tMax)) size <<= 1;
        inBuf.assign ((size_t) size, 0.0f);
        lpBuf.assign ((size_t) size, 0.0f);
        inMask = size - 1;
        for (auto& v : voices)
        {
            v.ola.assign ((size_t) size, 0.0f);
            v.mask = size - 1;
        }
        marks.assign (kMarkCapacity, Mark());
        refineLp.set (Biquad::Type::LowPass, fs, 1000.0, 0.707);

        xfade.reset (fs, 40.0f, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (inBuf.begin(), inBuf.end(), 0.0f);
        std::fill (lpBuf.begin(), lpBuf.end(), 0.0f);
        for (auto& v : voices)
        {
            std::fill (v.ola.begin(), v.ola.end(), 0.0f);
            v.nextSyn = 0.0;
            v.markIdx = 0;
            v.harmGain = 0.0f;
        }
        detector.reset();
        refineLp.reset();
        n = 0;
        markHead = 0;
        markCount = 0;
        lastMarkPos = 0;
        wasVoiced = false;
        curTarget = -1;
        deltaSmooth = 0.0f;
        flexW = 1.0f;
        holdSec = 0.0f;
        for (auto& r : ratio) r = 1.0f;
        harmVoiced = 0.0f;
        meter = Meter();
        appliedRange = -1;
    }

    int getLatencySamples() const noexcept { return latency; }

    void setParams (const Params& p)
    {
        params = p;
        mask = keyMask (p.key, p.scale);
        if ((int) p.range != appliedRange)
        {
            appliedRange = (int) p.range;
            switch (p.range)
            {
                case VoiceRange::Soprano:   detector.setRange (130.0f, 1250.0f); break;
                case VoiceRange::AltoTenor: detector.setRange (75.0f, 950.0f); break;
                case VoiceRange::Low:       detector.setRange (kAbsMinHz, 600.0f); break;
                default: break;
            }
        }
        detector.setThreshold (0.24f - 0.16f * clampv (p.tracking, 0.0f, 1.0f));
        phi = (float) std::pow (2.0, clampv ((double) p.throatSemis, -kMaxThroat, kMaxThroat) / 12.0);
        const bool transparent = ! p.correctionOn && std::fabs (p.transposeSemis) < 0.01f
                                 && std::fabs (p.throatSemis) < 0.01f && std::fabs (p.vibrato) < 0.01f;
        xfade.setTarget (transparent ? 0.0f : 1.0f);
        if (! p.learnKey && learning) learning = false;
        if (p.learnKey && ! learning) { learning = true; }
    }

    void resetKeyLearning() { keyDetector.reset(); }
    KeyDetector::Result keyResult() const { return keyDetector.analyse(); }
    const Meter& getMeter() const noexcept { return meter; }

    // Pitch medio y percentil bajo (para el asistente)
    const PitchDetector::Estimate& lastEstimate() const noexcept { return detector.estimate(); }

    // in: mono; outs[0] = voz principal, outs[1..2] = armonías (pueden ser nullptr)
    void process (const float* in, float* lead, float* h1, float* h2, int numSamples) noexcept
    {
        float* outs[kVoices] = { lead, h1, h2 };
        const bool harmActive[2] = { params.harmony[0] != HarmonyInterval::Off,
                                     params.harmony[1] != HarmonyInterval::Off };

        for (int i = 0; i < numSamples; ++i)
        {
            const float x = in[i];
            inBuf[(size_t) (n & inMask)] = x;
            lpBuf[(size_t) (n & inMask)] = refineLp.process (x);

            if (detector.push (x))
                updateCorrection();

            placeMarks();

            const int64_t r = n - latency;
            for (int v = 0; v < kVoices; ++v)
            {
                if (v > 0 && ! harmActive[v - 1] && voices[(size_t) v].harmGain < 1.0e-4f)
                {
                    // Voz apagada: mantenemos su posición sincronizada sin sintetizar
                    voices[(size_t) v].nextSyn = std::max (voices[(size_t) v].nextSyn, (double) (r + 1));
                    if (outs[v]) outs[v][i] = 0.0f;
                    continue;
                }
                synthesize (v, r, v > 0 && ! harmActive[v - 1]);
                auto& voice = voices[(size_t) v];
                const size_t idx = (size_t) (r & voice.mask);
                const float y = voice.ola[idx];
                voice.ola[idx] = 0.0f;
                if (outs[v]) outs[v][i] = sanitize (y);
            }

            // Mezcla con la señal seca (alineada) cuando la afinación está "transparente"
            const float dry = r >= 0 ? inBuf[(size_t) (r & inMask)] : 0.0f;
            const float xf = xfade.next();
            if (lead) lead[i] = dry + xf * (lead[i] - dry);

            ++n;
        }
        detector.flush();
        refineLp.flush();
    }

private:
    static constexpr float kAbsMinHz = 60.0f;
    static constexpr double kMaxThroat = 4.0;
    static constexpr int kMarkCapacity = 1024;

    struct Mark
    {
        int64_t pos = 0;
        float period = 256.0f;
        float ratio[kVoices] { 1.0f, 1.0f, 1.0f };
        float harmGain = 0.0f;
        bool voiced = false;
    };

    struct Voice
    {
        std::vector<float> ola;
        int mask = 0;
        double nextSyn = 0.0;
        int64_t markIdx = 0;   // índice absoluto de marca
        float harmGain = 0.0f;
    };

    // --------------------------------------------------------------------
    void updateCorrection() noexcept
    {
        const auto& e = detector.estimate();
        const float hopSec = detector.hopSeconds();
        const float transpose = clampv (params.transposeSemis, -12.0f, 12.0f);

        if (! e.voiced || e.hz <= 0.0f)
        {
            wasVoiced = false;
            curT = (float) tUnvoiced;
            curVoiced = false;
            const float tr = std::pow (2.0f, transpose / 12.0f);
            for (auto& r : ratio) r = tr;
            meter.voiced = false;
            holdSec = 0.0f;
            return;
        }

        const float ref = 440.0f * std::pow (2.0f, params.detuneCents / 1200.0f);
        const float m = 69.0f + 12.0f * std::log2 (e.hz / ref);

        if (! wasVoiced)
        {
            mSlow = m;
            deltaSmooth = 0.0f;
            holdSec = 0.0f;
            curTarget = -1;
            lastM = m;
        }
        wasVoiced = true;
        curVoiced = true;
        curT = clampv (e.periodSamples, 16.0f, (float) tMax);

        // Separación vibrato / pitch lento (pasa-bajos ~3 Hz a la tasa de análisis)
        const float aSlow = 1.0f - std::exp (-kTwoPi * 3.0f * hopSec);
        mSlow += aSlow * (m - mSlow);
        const float vib = m - mSlow;
        const float mP = mSlow + vib * (1.0f + clampv (params.vibrato, -1.0f, 1.0f));

        // Nota objetivo con histéresis
        const int cand = nearestAllowed (mask, mP);
        if (curTarget < 0 || ! isAllowed (mask, curTarget))
            curTarget = cand;
        else if (cand != curTarget && std::fabs (mP - (float) curTarget) > std::fabs (mP - (float) cand) + 0.12f)
        {
            curTarget = cand;
            holdSec = 0.0f;
        }

        // Duración de la nota sostenida (para Humanize)
        if (std::fabs (m - lastM) < 0.35f) holdSec += hopSec; else holdSec = 0.0f;
        lastM = m;

        float delta = params.correctionOn ? (float) curTarget - mP : 0.0f;

        const float humanRamp = clampv ((holdSec - 0.12f) / 0.25f, 0.0f, 1.0f);
        const float tauMs = params.retuneMs + clampv (params.humanize, 0.0f, 1.0f) * 160.0f * humanRamp;
        const float a = tauMs <= 0.5f ? 1.0f : 1.0f - std::exp (-hopSec / (tauMs * 0.001f));
        deltaSmooth += a * (delta - deltaSmooth);

        // Flex-Tune: solo corrige cuando el cantante se acerca a la nota
        const float z = 0.06f + (1.0f - clampv (params.flexTune, 0.0f, 1.0f)) * 1.6f;
        const float ad = std::fabs (delta);
        const float wRaw = ad <= z ? 1.0f : std::max (0.0f, 1.0f - (ad - z) / 0.3f);
        flexW += 0.35f * (wRaw - flexW);

        const float outPitch = mP + deltaSmooth * flexW;
        ratio[0] = std::pow (2.0f, (outPitch - m + transpose) / 12.0f);

        for (int h = 0; h < 2; ++h)
        {
            const int off = harmonyOffset (params.harmony[h], curTarget);
            ratio[(size_t) h + 1] = std::pow (2.0f, (outPitch + (float) off - m + transpose) / 12.0f);
        }

        if (learning)
            keyDetector.addFrame (m, e.clarity, hopSec);

        meter.voiced = true;
        meter.inputMidi = m;
        meter.outputMidi = outPitch + transpose;
        meter.targetNote = curTarget;
    }

    int harmonyOffset (HarmonyInterval iv, int target) const noexcept
    {
        auto pick = [&] (std::initializer_list<int> options) {
            for (int o : options)
                if (isAllowed (mask, target + o)) return o;
            return *options.begin();
        };
        switch (iv)
        {
            case HarmonyInterval::OctDown:   return -12;
            case HarmonyInterval::FifthDown: return pick ({ -7, -6, -8 });
            case HarmonyInterval::ThirdDown: return pick ({ -3, -4 });
            case HarmonyInterval::ThirdUp:   return pick ({ 4, 3 });
            case HarmonyInterval::FifthUp:   return pick ({ 7, 6, 8 });
            case HarmonyInterval::OctUp:     return 12;
            default: return 0;
        }
    }

    // --------------------------------------------------------------------
    void placeMarks() noexcept
    {
        const int64_t limit = n - horizon;
        while (true)
        {
            const float T = curVoiced ? curT : (float) tUnvoiced;
            int64_t cand = lastMarkPos + (int64_t) std::lround (T);
            if (cand > limit) break;

            if (curVoiced)
            {
                // Afinamos la marca al pico glótico más cercano (pasa-bajos ~1 kHz)
                const int search = std::max (2, (int) (T * 0.15f));
                float best = -1.0e9f;
                int64_t bestPos = cand;
                for (int d = -search; d <= search; ++d)
                {
                    const float bias = 1.0f - 0.35f * (float) std::abs (d) / (float) search;
                    const float v = lpBuf[(size_t) ((cand + d) & inMask)] * bias;
                    if (v > best) { best = v; bestPos = cand + d; }
                }
                cand = std::max (lastMarkPos + 16, bestPos);
            }

            Mark& mk = marks[(size_t) (markHead % kMarkCapacity)];
            mk.pos = cand;
            mk.period = T;
            mk.voiced = curVoiced;
            for (int v = 0; v < kVoices; ++v)
                mk.ratio[v] = clampv (ratio[(size_t) v], 0.4f, 2.5f);
            harmVoiced += 0.5f * ((curVoiced ? 1.0f : 0.0f) - harmVoiced);
            mk.harmGain = harmVoiced;

            ++markHead;
            markCount = std::min<int64_t> (markCount + 1, kMarkCapacity);
            lastMarkPos = cand;
        }
    }

    const Mark& markAt (int64_t idx) const noexcept { return marks[(size_t) (idx % kMarkCapacity)]; }

    void synthesize (int v, int64_t r, bool fadingOut) noexcept
    {
        auto& voice = voices[(size_t) v];
        if (markHead == 0) return;
        const int64_t oldest = markHead - markCount;

        while (voice.nextSyn <= (double) (r + 1 + maxOutHalf))
        {
            const double s = voice.nextSyn;

            // Marca de análisis más cercana a la posición de síntesis
            int64_t idx = std::max (voice.markIdx, oldest);
            idx = std::min (idx, markHead - 1);
            while (idx + 1 < markHead
                   && std::fabs ((double) markAt (idx + 1).pos - s) <= std::fabs ((double) markAt (idx).pos - s))
                ++idx;
            voice.markIdx = idx;
            const Mark& mk = markAt (idx);

            float rt = mk.ratio[v];
            const float T = mk.period;
            float g = 1.0f;
            if (v > 0)
            {
                const float targetG = fadingOut ? 0.0f : mk.harmGain;
                voice.harmGain += 0.45f * (targetG - voice.harmGain);
                g = voice.harmGain;
            }

            const float ph = v == 0 ? phi : phi * (v == 1 ? 1.025f : 0.975f);
            g *= std::pow (ph, 0.5f); // compensa la energía al mover formantes
            const double tg = std::min ((double) maxOutHalf - 1.0, (double) T / ph);
            const int64_t k0 = (int64_t) std::ceil (s - tg);
            const int64_t k1 = (int64_t) std::floor (s + tg);
            const double invTg = 1.0 / tg;

            if (g > 1.0e-4f)
            {
                for (int64_t k = std::max (k0, r + 1); k <= k1; ++k)
                {
                    const double d = (double) k - s;
                    const float wnd = 0.5f + 0.5f * (float) std::cos (kPi * d * invTg);
                    const double inPos = (double) mk.pos + d * ph;
                    voice.ola[(size_t) (k & voice.mask)] += g * wnd * hermiteRead (inBuf.data(), inMask, inPos);
                }
            }

            rt = clampv (rt, 0.4f, 2.5f);
            voice.nextSyn += std::max (16.0, (double) T / rt);
        }
    }

    // --------------------------------------------------------------------
    double fs = 44100.0;
    PitchDetector detector;
    KeyDetector keyDetector;
    Params params;
    std::array<bool, 12> mask {};
    Meter meter;

    std::vector<float> inBuf, lpBuf;
    int inMask = 0;
    Biquad refineLp;
    std::array<Voice, kVoices> voices;
    std::vector<Mark> marks;
    int64_t markHead = 0, markCount = 0, lastMarkPos = 0, n = 0;

    int tMax = 800, tUnvoiced = 256, horizon = 900, maxOutHalf = 1000, latency = 2048;
    float phi = 1.0f;
    SmoothedValue xfade;

    // Estado de corrección
    bool wasVoiced = false, curVoiced = false, learning = false;
    float curT = 256.0f;
    float mSlow = 60.0f, lastM = 60.0f;
    int curTarget = -1;
    float deltaSmooth = 0.0f, flexW = 1.0f, holdSec = 0.0f;
    std::array<float, kVoices> ratio { 1.0f, 1.0f, 1.0f };
    float harmVoiced = 0.0f;
    int appliedRange = -1;
};

} // namespace evo
