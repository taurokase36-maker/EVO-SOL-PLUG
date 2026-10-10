// EVO SOL — motor completo: une afinación, módulos inteligentes, núcleo
// analógico (EQ + FET + óptico + saturación) y ear candy estéreo.
#pragma once

#include "Assistant.h"
#include "Dynamics.h"
#include "Effects.h"
#include "PitchEngine.h"
#include "Saturation.h"
#include "Spectral.h"
#include <atomic>

namespace evo
{
struct EngineParams
{
    // Entrada / salida
    float inGainDb = 0.0f, outGainDb = 0.0f, ceilingDb = -1.0f;
    // Inteligentes
    bool riderOn = true;
    float riderTargetDb = -20.0f, riderRangeDb = 9.0f;
    float smooth = 0.35f, deEss = 0.4f;
    // Afinación + armonías
    PitchEngine::Params pitch;
    float harmLevel = 0.5f;
    // Núcleo analógico
    bool coreOn = true;
    float hpfHz = 80.0f, bodyDb = 0.0f, presenceDb = 1.5f, airDb = 3.0f;
    float comp = 0.5f, color = 0.5f, warmth = 0.35f, parallel = 1.0f;
    // Ear candy
    float doubler = 0.0f;
    float delayMix = 0.0f, delayBeats = 1.0f, delayFeedback = 0.35f, delayTone = 0.5f;
    float reverbMix = 0.15f, reverbSize = 0.5f, reverbDecay = 1.8f, reverbPreMs = 25.0f;
    float duck = 0.6f, radio = 0.0f, width = 1.0f;
    double bpm = 120.0;
};

struct EngineMeters
{
    std::atomic<float> inDb { -100.0f }, outDbL { -100.0f }, outDbR { -100.0f };
    std::atomic<float> riderDb { 0.0f }, fetGr { 0.0f }, optoGr { 0.0f }, deEssGr { 0.0f };
    std::atomic<float> smoothGr { 0.0f }, limGr { 0.0f };
    std::atomic<bool> voiced { false };
    std::atomic<float> inMidi { 0.0f }, outMidi { 0.0f };
    std::atomic<int> targetNote { -1 };
};

class VocalEngine
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        fs = sampleRate;
        maxBlockSize = std::max (1, maxBlock);
        mono.assign ((size_t) maxBlockSize, 0.0f);
        lead.assign ((size_t) maxBlockSize, 0.0f);
        h1.assign ((size_t) maxBlockSize, 0.0f);
        h2.assign ((size_t) maxBlockSize, 0.0f);

        pitch.prepare (fs, maxBlockSize);
        resonance.prepare (fs);
        rider.prepare (fs);
        assistant.prepare (fs);
        fet.prepare (fs);
        opto.prepare (fs);
        deEsser.prepare (fs);
        saturator.prepare (fs);
        clipL.reset(); clipR.reset();
        limiter.prepare (fs);
        doubler.prepare (fs);
        delay.prepare (fs);
        reverb.prepare (fs);
        radio.prepare (fs);
        for (auto* d : { &dryAlign, &harm1Align, &harm2Align }) d->prepare (resonance.getLatencySamples() + 64);
        for (auto* f : { &harmHp1, &harmHp2 }) f->set (Biquad::Type::HighPass, fs, 220.0, 0.7);

        inGain.reset (fs, 30.0f, 1.0f);
        outGain.reset (fs, 30.0f, 1.0f);
        makeup.reset (fs, 60.0f, 1.0f);
        parallelMix.reset (fs, 40.0f, 1.0f);
        harmGain.reset (fs, 40.0f, 0.5f);
        widthSm.reset (fs, 40.0f, 1.0f);

        lastHpf = lastBody = lastPresence = lastAir = -999.0f;
        prepared = true;
        updateFilters (params);
    }

    int getLatencySamples() const
    {
        return pitch.getLatencySamples() + resonance.getLatencySamples() + AnalogSaturator::latencySamples()
               + OversampledClipper::latencySamples() + limiter.getLatencySamples();
    }

    void setParams (const EngineParams& p)
    {
        params = p;
        pitch.setParams (p.pitch);
        rider.setEnabled (p.riderOn);
        rider.setTargetDb (p.riderTargetDb);
        rider.setRangeDb (p.riderRangeDb);
        resonance.setDepth (p.smooth);
        deEsser.setAmount (p.deEss);

        const float c = clampv (p.comp, 0.0f, 1.0f);
        const float tF = -8.0f - 20.0f * c, rF = 4.0f + 4.0f * c;
        const float tO = -12.0f - 16.0f * c, rO = 2.5f + 2.0f * c;
        fet.setThreshold (tF); fet.setRatio (rF); fet.setColor (p.color);
        opto.setThreshold (tO); opto.setRatio (rO);
        const float mk = p.coreOn ? clampv (0.5f * ((-12.0f - tF) * (1.0f - 1.0f / rF) + (-14.0f - tO) * (1.0f - 1.0f / rO)), 0.0f, 14.0f) : 0.0f;
        makeup.setTarget (dbToGain (mk));
        saturator.setDrive (p.coreOn ? p.warmth : 0.0f);
        parallelMix.setTarget (p.coreOn ? clampv (p.parallel, 0.0f, 1.0f) : 1.0f);

        doubler.setAmount (p.doubler);
        const double beatSec = 60.0 / clampv (p.bpm, 30.0, 300.0);
        delay.setTimeSeconds ((float) (beatSec * p.delayBeats));
        delay.setFeedback (p.delayFeedback);
        delay.setMix (p.delayMix);
        delay.setDuck (p.duck);
        delay.setTone (p.delayTone);
        reverb.setMix (p.reverbMix);
        reverb.setSize (p.reverbSize);
        reverb.setDecay (p.reverbDecay);
        reverb.setPreDelayMs (p.reverbPreMs);
        reverb.setDuck (p.duck * 0.8f);
        reverb.setDamping (0.45f);
        radio.setAmount (p.radio);

        inGain.setTarget (dbToGain (p.inGainDb));
        outGain.setTarget (dbToGain (p.outGainDb));
        harmGain.setTarget (clampv (p.harmLevel, 0.0f, 1.0f));
        widthSm.setTarget (clampv (p.width, 0.0f, 2.0f));
        const float ceil = dbToGain (clampv (p.ceilingDb, -12.0f, 0.0f));
        limiter.setCeilingDb (clampv (p.ceilingDb, -12.0f, 0.0f));
        clipL.setThreshold (ceil * 1.12f);
        clipR.setThreshold (ceil * 1.12f);
        if (prepared) updateFilters (p);
    }

    // --- Asistente / detección de key ---------------------------------
    void startLearning()
    {
        assistant.reset();
        pitch.resetKeyLearning();
        learning = true;
    }
    void stopLearning() { learning = false; }
    bool isLearning() const { return learning; }
    VocalAssistant::Suggestion suggestion() const { return assistant.compute (pitch.keyResult()); }
    KeyDetector::Result keyResult() const { return pitch.keyResult(); }

    EngineMeters meters;

    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples) noexcept
    {
        for (int start = 0; start < numSamples; start += maxBlockSize)
        {
            const int n = std::min (maxBlockSize, numSamples - start);
            processChunk (inL + start, inR ? inR + start : nullptr, outL + start, outR + start, n);
        }
    }

private:
    void updateFilters (const EngineParams& p)
    {
        if (p.hpfHz != lastHpf)
        {
            hpf1.set (Biquad::Type::HighPass, fs, p.hpfHz, 0.5412);
            hpf2.set (Biquad::Type::HighPass, fs, p.hpfHz, 1.3066);
            lastHpf = p.hpfHz;
        }
        if (p.bodyDb != lastBody) { body.set (Biquad::Type::Peak, fs, 240.0, 0.9, p.bodyDb); lastBody = p.bodyDb; }
        if (p.presenceDb != lastPresence) { presence.set (Biquad::Type::Peak, fs, 3600.0, 0.8, p.presenceDb); lastPresence = p.presenceDb; }
        if (p.airDb != lastAir) { air.set (Biquad::Type::HighShelf, fs, 11000.0, 0.6, p.airDb); lastAir = p.airDb; }
    }

    void processChunk (const float* inL, const float* inR, float* outL, float* outR, int n) noexcept
    {
        float inPeak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float x = inR ? 0.5f * (inL[i] + inR[i]) : inL[i];
            const float g = x * inGain.next();
            inPeak = std::max (inPeak, std::fabs (g));
            mono[(size_t) i] = g;
        }

        if (learning)
        {
            const auto& m = pitch.getMeter();
            for (int i = 0; i < n; ++i) assistant.push (mono[(size_t) i], m.voiced, m.inputMidi);
        }

        for (int i = 0; i < n; ++i)
            mono[(size_t) i] = hpf2.process (hpf1.process (rider.process (mono[(size_t) i])));

        pitch.process (mono.data(), lead.data(), h1.data(), h2.data(), n);

        const int resLat = resonance.getLatencySamples();
        const int satLat = AnalogSaturator::latencySamples();
        float peakL = 0.0f, peakR = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            // 1) Limpieza inteligente de resonancias
            float l = resonance.process (lead[(size_t) i]);

            // 2) Núcleo: EQ de tono -> FET -> óptico -> aire -> de-esser -> saturación
            l = presence.process (body.process (l));
            const float pre = l;
            if (params.coreOn)
            {
                l = fet.process (l);
                l = opto.process (l);
            }
            l *= makeup.next();
            l = air.process (l);
            l = deEsser.process (l);
            l = saturator.process (l);
            dryAlign.push (pre);
            const float preAligned = dryAlign.readInt (satLat);
            l = preAligned + parallelMix.next() * (l - preAligned);
            l = radio.process (l);

            // 3) Armonías alineadas con la voz principal
            harm1Align.push (h1[(size_t) i]);
            harm2Align.push (h2[(size_t) i]);
            const float hg = harmGain.next();
            const float a1 = harmHp1.process (harm1Align.readInt (resLat + satLat)) * hg * makeup.get();
            const float a2 = harmHp2.process (harm2Align.readInt (resLat + satLat)) * hg * makeup.get();

            // 4) Ear candy estéreo
            float dL, dR, yL, yR, rL, rR;
            doubler.process (l, dL, dR);
            const float send = l + 0.7f * (a1 + a2);
            delay.process (send, yL, yR);
            reverb.process (send, rL, rR);

            float L = l + 0.85f * a1 + 0.35f * a2 + dL + yL + rL;
            float R = l + 0.35f * a1 + 0.85f * a2 + dR + yR + rR;

            // 5) Ancho estéreo (M/S), salida, clipper y limitador
            const float w = widthSm.next();
            const float mid = 0.5f * (L + R), side = 0.5f * (L - R) * w;
            const float og = outGain.next();
            L = clipL.process ((mid + side) * og);
            R = clipR.process ((mid - side) * og);
            limiter.process (L, R);

            outL[i] = sanitize (L);
            outR[i] = sanitize (R);
            peakL = std::max (peakL, std::fabs (outL[i]));
            peakR = std::max (peakR, std::fabs (outR[i]));
        }

        for (auto* b : { &hpf1, &hpf2, &body, &presence, &air, &harmHp1, &harmHp2 }) b->flush();
        deEsser.flush(); saturator.flush(); doubler.flush(); delay.flush(); reverb.flush(); radio.flush();

        // Medidores (balística simple: caída de ~20 dB/s)
        auto fall = [] (std::atomic<float>& m, float newDb, float dropDb) {
            const float old = m.load (std::memory_order_relaxed);
            m.store (std::max (newDb, old - dropDb), std::memory_order_relaxed);
        };
        const float drop = 20.0f * (float) n / (float) fs;
        fall (meters.inDb, gainToDb (inPeak, -100.0f), drop);
        fall (meters.outDbL, gainToDb (peakL, -100.0f), drop);
        fall (meters.outDbR, gainToDb (peakR, -100.0f), drop);
        meters.riderDb.store (rider.getGainDb(), std::memory_order_relaxed);
        meters.fetGr.store (params.coreOn ? fet.getReductionDb() : 0.0f, std::memory_order_relaxed);
        meters.optoGr.store (params.coreOn ? opto.getReductionDb() : 0.0f, std::memory_order_relaxed);
        meters.deEssGr.store (deEsser.getReductionDb(), std::memory_order_relaxed);
        meters.smoothGr.store (resonance.getReductionDb(), std::memory_order_relaxed);
        meters.limGr.store (limiter.getReductionDb(), std::memory_order_relaxed);
        const auto& pm = pitch.getMeter();
        meters.voiced.store (pm.voiced, std::memory_order_relaxed);
        meters.inMidi.store (pm.inputMidi, std::memory_order_relaxed);
        meters.outMidi.store (pm.outputMidi, std::memory_order_relaxed);
        meters.targetNote.store (pm.targetNote, std::memory_order_relaxed);
    }

    double fs = 44100.0;
    int maxBlockSize = 512;
    bool prepared = false, learning = false;
    EngineParams params;

    std::vector<float> mono, lead, h1, h2;
    PitchEngine pitch;
    ResonanceSuppressor resonance;
    VocalRider rider;
    VocalAssistant assistant;
    Biquad hpf1, hpf2, body, presence, air, harmHp1, harmHp2;
    FetCompressor fet;
    OptoCompressor opto;
    AdaptiveDeEsser deEsser;
    AnalogSaturator saturator;
    OversampledClipper clipL, clipR;
    LookaheadLimiter limiter;
    Doubler doubler;
    PingPongDelay delay;
    FdnReverb reverb;
    RadioFilter radio;
    DelayLine dryAlign, harm1Align, harm2Align;
    SmoothedValue inGain, outGain, makeup, parallelMix, harmGain, widthSm;
    float lastHpf = -999.0f, lastBody = -999.0f, lastPresence = -999.0f, lastAir = -999.0f;
};

} // namespace evo
