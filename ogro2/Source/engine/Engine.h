// OGRO 2 — motor completo: secuenciador + voces + modulaciones + ring mod +
// drum bus + FX + master (EQ y compresión estilo EVO SOL).
#pragma once

#include "../dsp/Dynamics.h"
#include "../dsp/Effects.h"
#include "../dsp/Saturation.h"
#include "BarFx.h"
#include "ParamDefs.h"
#include "Sequencer.h"
#include "Voices.h"
#include <atomic>

namespace ogro2
{
// Compresor estéreo enlazado: etapa FET rápida + etapa óptica lenta (como EVO SOL)
class LinkedBusComp
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        aAtt = onePoleCoef (0.4f, fs);
        aRelFast = onePoleCoef (60.0f, fs);
        aRelSlow = onePoleCoef (260.0f, fs);
        aOptoAtt = onePoleCoef (15.0f, fs);
        aOptoRel = onePoleCoef (450.0f, fs);
        aDet = onePoleCoef (3.0f, fs);
        fet = opto = sq = 0.0f;
        dcL.prepare (fs); dcR.prepare (fs);
    }
    // amount 0..1, color 0..1, fetOnly = drum bus (más agresivo, sin óptico)
    void set (float amount, float colorAmt, bool drumBus)
    {
        const float a = clampv (amount, 0.0f, 1.0f);
        drum = drumBus;
        thrF = drumBus ? -4.0f - 26.0f * a : -6.0f - 18.0f * a;
        ratioF = drumBus ? 3.0f + 9.0f * a : 2.5f + 3.5f * a;
        thrO = -10.0f - 14.0f * a;
        ratioO = 2.0f + 2.0f * a;
        color = clampv (colorAmt, 0.0f, 1.0f);
        const float mk = a <= 0.0f ? 0.0f
                                   : 0.55f * (-thrF - 6.0f) * (1.0f - 1.0f / ratioF) + (drumBus ? 0.0f : 0.3f * (-thrO - 10.0f) * (1.0f - 1.0f / ratioO));
        makeupTarget = dbToGain (clampv (mk, 0.0f, 15.0f));
        enabled = a > 0.001f;
    }
    float reductionDb() const { return fet + opto; }
    void resetState() noexcept { fet = opto = sq = 0.0f; makeup = makeupTarget; dcL.reset(); dcR.reset(); }

    inline void process (float& l, float& r) noexcept
    {
        makeup += 0.0005f * (makeupTarget - makeup);
        if (! enabled) { fet *= 0.999f; opto *= 0.999f; return; }
        const float peak = std::max (std::fabs (l), std::fabs (r));
        const float inDb = gainToDb (peak, -120.0f);
        const float want = gainComputerDb (inDb, thrF, ratioF, 6.0f);
        if (want > fet) fet += aAtt * (want - fet);
        else fet += ((fet - want) > 5.0f ? aRelFast : aRelSlow) * (want - fet);

        if (! drum)
        {
            const float m = 0.5f * (l * l + r * r);
            sq += aDet * (m - sq);
            const float wantO = gainComputerDb (10.0f * std::log10 (sq + 1e-12f) + 3.0f, thrO, ratioO, 10.0f);
            opto += (wantO > opto ? aOptoAtt : aOptoRel) * (wantO - opto);
        }
        const float g = dbToGain (-(fet + opto)) * makeup;
        l *= g; r *= g;
        // Color FET: armónicos proporcionales a la compresión
        const float drive = color * (0.2f + clampv (fet / 15.0f, 0.0f, 1.0f) * 0.8f);
        if (drive > 0.001f)
        {
            const float k = 1.0f + 2.2f * drive;
            l = dcL.process (l + drive * (std::tanh (k * l + 0.1f * drive) / k - l));
            r = dcR.process (r + drive * (std::tanh (k * r + 0.1f * drive) / k - r));
        }
    }

private:
    double fs = 44100.0;
    bool enabled = true, drum = false;
    float thrF = -18, ratioF = 4, thrO = -20, ratioO = 3, color = 0.5f;
    float aAtt = 0.5f, aRelFast = 0.01f, aRelSlow = 0.001f, aOptoAtt = 0.01f, aOptoRel = 0.001f, aDet = 0.1f;
    float fet = 0, opto = 0, sq = 0, makeup = 1.0f, makeupTarget = 1.0f;
    DcBlocker dcL, dcR;
};

// Textura analógica: crackle de vinilo + hiss de cinta
class Texture
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        hissL.set (Biquad::Type::BandPass, fs, 5000.0, 0.4);
        hissR.set (Biquad::Type::BandPass, fs, 5000.0, 0.4);
        crackBp.set (Biquad::Type::BandPass, fs, 3200.0, 0.9);
        rumble.set (Biquad::Type::LowPass, fs, 90.0, 0.7);
    }
    void reseed (uint32_t s) { rng.seed (s); }
    void resetState() noexcept { crackAmp = 0.0f; hissL.reset(); hissR.reset(); crackBp.reset(); rumble.reset(); }
    inline void process (float amount, float& l, float& r) noexcept
    {
        if (amount <= 0.001f) return;
        const float hiss = 0.006f * amount;
        float cl = 0.0f;
        if (rng.uni() < (float) (14.0 * amount / fs) + 2e-6f) crackAmp = 0.1f + 0.5f * rng.uni();
        if (crackAmp > 1e-4f) { cl = crackBp.process (rng.next() * crackAmp); crackAmp *= 0.86f; }
        const float rb = rumble.process (rng.next()) * 0.02f * amount;
        l += hissL.process (rng.next()) * hiss + cl * amount * 0.5f + rb;
        r += hissR.process (rng.next()) * hiss + cl * amount * 0.45f + rb;
    }

private:
    double fs = 44100.0;
    Noise rng { 0xC0FFEEu };
    Biquad hissL, hissR, crackBp, rumble;
    float crackAmp = 0.0f;
};

struct EngineMeters
{
    std::atomic<float> outL { -100.0f }, outR { -100.0f }, drumGr { 0.0f }, masterGr { 0.0f }, limGr { 0.0f };
    std::atomic<int> triggers[kTracks] {};
    std::atomic<int> step[kTracks] {};
    std::atomic<int> pattern { 0 };
    std::atomic<bool> playing { false };
    std::atomic<float> lanePhase[kLanes] {};
    std::atomic<float> lfo[P::kLfos] {};
    std::atomic<double> bpm { 120.0 };
    std::atomic<uint32_t> barFx { 0u };   // efectos de fin de compás sonando (un bit por efecto)
    std::atomic<bool> fill { false };
};

// Registro de disparos con la muestra exacta (para tests y para la interfaz).
// Lo escribe solo el hilo de audio; se lee con "count" como índice de escritura.
struct TriggerLog
{
    static constexpr int kSize = 4096;
    struct Entry { int64_t sample = 0; int track = 0; };
    Entry entries[kSize] {};
    std::atomic<uint32_t> count { 0 };
    void push (int64_t sample, int track) noexcept
    {
        const uint32_t c = count.load (std::memory_order_relaxed);
        entries[c % kSize] = { sample, track };
        count.store (c + 1, std::memory_order_release);
    }
};

struct HostInfo
{
    bool valid = false, playing = false;
    double ppq = 0.0, bpm = 120.0;
    bool looping = false;               // loop de Ableton activo
    double loopStart = 0.0, loopEnd = 0.0;
};

struct MidiTrigger { int sample; int track; float vel; bool off = false; };

// Macros: -1..+1 desde el centro (al medio no cambian nada)
inline float macroAmount (float v) { return (v - 0.5f) * 2.0f; }
// MOVIMIENTO: con recorrido x2 va de 0 (quieto) a 3 veces la modulación; con x1, de 0 a 2 (como la 1.0)
inline float movementScale (float v, bool doubleDepth)
{
    if (! doubleDepth) return v * 2.0f;
    return v <= 0.5f ? v * 2.0f : 1.0f + (v - 0.5f) * 4.0f;
}

class Engine
{
public:
    static constexpr int kControl = 32;

    Engine()
    {
        const auto& d = paramDefs();
        base.resize (d.size());
        eff.resize (d.size());
        modNorm.assign (d.size(), 0.0f);
        for (size_t i = 0; i < d.size(); ++i) base[i] = eff[i] = d[i].def;
        voices[Kick] = &kick; voices[Clap] = &clap; voices[Rim] = &rim; voices[HatC] = &hatC;
        voices[HatO] = &hatO; voices[Perc] = &perc; voices[Bass] = &bass;
    }

    void prepare (double sampleRate, int maxBlock)
    {
        fs = sampleRate;
        maxBlockSize = std::max (32, maxBlock);
        for (int t = 0; t < kTracks; ++t)
            if (voices[t]) voices[t]->prepare (fs, 0x1234567u * (uint32_t) (t + 3));
        chord.prepare (fs, 0xBEEFu);
        clock.prepare (fs);
        delay.prepare (fs);
        reverb.prepare (fs);
        drumComp.prepare (fs);
        masterComp.prepare (fs);
        drumSatL.prepare (fs); drumSatR.prepare (fs);
        warmL.prepare (fs); warmR.prepare (fs);
        clipL.reset(); clipR.reset();
        limiter.prepare (fs);
        texture.prepare (fs);
        barFx.prepare (fs);
        synthAlignL.prepare (64); synthAlignR.prepare (64);
        dryL.prepare (64); dryR.prepare (64);
        djL.reset(); djR.reset();
        lastFilter = 99.0f;
        lastEq[0] = lastEq[1] = lastEq[2] = lastEq[3] = -999.0f;
        for (auto& g : mixState) g = MixState();
        pumpEnv = 0.0f; pumpRise = false;
        ringPhL = ringPhR = 0.0f;
        internalPpq = 0.0;
        controlCounter = 0;
        lastRandStep = -1;
        sampleClock = 0;
        for (auto& l : trackLocks) l.n = 0;
        updateControl (0.0, false);
    }

    int getLatencySamples() const
    {
        return AnalogSaturator::latencySamples() * 2 + OversampledClipper::latencySamples() + limiter.getLatencySamples();
    }

    // Valores físicos de todos los parámetros (desde el plugin)
    void setParams (const float* values, int count)
    {
        for (int i = 0; i < count && i < (int) base.size(); ++i) base[(size_t) i] = values[i];
    }

    // Tests: sin saltear voces en silencio (comparación muestra a muestra con la 1.0)
    void setExactLegacy (bool e) { exactLegacy = e; }

    // Llamadas seguras desde la interfaz
    void setInternalPlay (bool p) { internalPlayReq.store (p); }
    bool isInternalPlaying() const { return internalPlayReq.load(); }
    void audition (int track) { if (track >= 0 && track < kTracks) auditionMask.fetch_or (1u << track); }
    // Botón "TIRAR FX": efectos de fin de compás ya, por el largo elegido
    void fireBarFxNow() { barFxNow.store (true); }

    PatternBank bank;
    ModRouting routing;
    EngineMeters meters;
    TriggerLog triggerLog;

    void process (const HostInfo& host, const MidiTrigger* midi, int numMidi, float* outL, float* outR, int numSamples) noexcept
    {
        for (int start = 0; start < numSamples; start += maxBlockSize)
        {
            const int n = std::min (maxBlockSize, numSamples - start);
            HostInfo h = host;
            if (h.valid && h.playing)
            {
                h.ppq += (double) start * h.bpm / 60.0 / fs;
                if (h.looping && h.loopEnd > h.loopStart && h.ppq >= h.loopEnd) h.ppq -= h.loopEnd - h.loopStart;
            }
            processChunk (h, midi, numMidi, start, outL + start, outR + start, n);
        }
    }

private:
    struct MixState { float gL = 0, gR = 0, dly = 0, rev = 0; };

    // ------------------------------------------------------------------
    void processChunk (const HostInfo& host, const MidiTrigger* midi, int numMidi, int offset,
                       float* outL, float* outR, int n) noexcept
    {
        // Transporte
        Transport tr;
        const double bpm = host.valid && host.bpm > 0.0 ? host.bpm : (double) base[P::Tempo];
        tr.bpm = bpm;
        if (host.valid && host.playing)
        {
            tr.playing = true; tr.ppq = host.ppq; internalPpq = host.ppq;
            tr.looping = host.looping; tr.loopStart = host.loopStart; tr.loopEnd = host.loopEnd;
        }
        else if (internalPlay) { tr.playing = true; tr.ppq = internalPpq; internalPpq += (double) n * bpm / 60.0 / fs; }
        if (internalPlayReq.load (std::memory_order_relaxed) != internalPlay)
        {
            internalPlay = ! internalPlay;
            internalPpq = 0.0;
        }
        playing = tr.playing;
        curPpq = tr.ppq;
        curBpm = bpm;

        fixedRandom = ! exactLegacy && base[P::RandomMode] < 0.5f;
        randomSeed = (uint32_t) std::lround (base[P::Seed]);
        clock.setRandom (fixedRandom, randomSeed);
        clock.setSwing (base[P::Swing]);
        for (int t = 0; t < kTracks; ++t)
            clock.setTrackSwing (t, base[(size_t) P::track (t, P::SwingOwn)] > 0.5f, eff[(size_t) P::track (t, P::SwingAmt)]);
        clock.setGroove ((int) base[P::Groove], eff[P::GrooveAmt], eff[P::Humanize]);
        clock.setPatternMode ((int) base[P::PatChange], base[P::PolyMode] > 0.5f, base[P::Chain] > 0.5f);
        {
            static const int fillBars[] = { 0, 2, 4, 8, 16 };
            static const int fxBars[] = { 0, 1, 2, 4, 8 };
            clock.setFill (fillBars[clampv ((int) base[P::Fill], 0, 4)], (int) base[P::FillPat]);
            clock.setBarFx (fxBars[clampv ((int) base[P::BfxEvery], 0, 4)], (int) std::lround (base[P::BfxLen]));
        }
        clock.requestPattern ((int) base[P::Pattern]);
        int numEvents = clock.process (tr, n, bank, rngState, events, kMaxEvents);
        // Azar FIJO: al arrancar el transporte o al saltar a otro lugar, todo vuelve al mismo estado
        // inicial (voces, efectos, compresores, ring): dos renders desde el mismo lugar salen iguales
        if (clock.relocated() && fixedRandom) resetForRender (tr.ppq);

        // Disparos MIDI (C1..G1 = pistas 1..8). El note-off cierra el gate de bajo y acordes.
        for (int i = 0; i < numMidi && numEvents < kMaxEvents; ++i)
        {
            const int s = midi[i].sample - offset;
            if (s < 0 || s >= n) continue;
            TriggerEvent e;
            e.sample = s; e.track = midi[i].track;
            e.release = midi[i].off;
            e.hit.vel = midi[i].vel; e.hit.accent = midi[i].vel > 0.93f;
            if (fixedRandom)
            {
                const int64_t k = (int64_t) std::floor ((curPpq + (double) s * curBpm / 60.0 / fs) * 4.0);
                e.hit.fixedSeed = true;
                e.hit.seed = hash32 (randomSeed, (uint32_t) e.track, (uint32_t) k, 0x4D494449u);
            }
            int j = numEvents++;
            while (j > 0 && events[j - 1].sample > s) { events[j] = events[j - 1]; --j; }
            events[j] = e;
        }

        // FX de fin de compás a mano
        if (barFxNow.exchange (false) && numEvents < kMaxEvents)
        {
            TriggerEvent e; e.sample = 0; e.track = -1;
            e.bfxBar = (int) (std::floor (curPpq / 4.0)) + 1000003;   // otra semilla que la del compás
            e.bfxBeats = 0.25 * std::lround (base[P::BfxLen]);
            for (int j = numEvents++; j > 0; --j) events[j] = events[j - 1];
            events[0] = e;
        }

        // Pruebas de sonido desde la interfaz
        if (const uint32_t mask = auditionMask.exchange (0u))
            for (int t = 0; t < kTracks && numEvents < kMaxEvents; ++t)
                if (mask & (1u << t))
                {
                    TriggerEvent e; e.sample = 0; e.track = t; e.hit.vel = 0.85f;
                    for (int j = numEvents++; j > 0; --j) events[j] = events[j - 1];
                    events[0] = e;
                }

        int ev = 0;
        int pos = 0;
        while (pos < n)
        {
            if (controlCounter <= 0)
            {
                const double ppqHere = curPpq + (double) pos * curBpm / 60.0 / fs;
                updateControl (ppqHere, playing);
                controlCounter = kControl;
            }
            while (ev < numEvents && events[ev].sample <= pos)
            {
                const double ppqAt = curPpq + (double) events[ev].sample * curBpm / 60.0 / fs;
                fire (events[ev++], ppqAt);
            }
            int segEnd = std::min (n, pos + controlCounter);
            if (ev < numEvents) segEnd = std::min (segEnd, std::max (pos + 1, events[ev].sample));
            const int len = segEnd - pos;
            renderSegment (outL + pos, outR + pos, len);
            controlCounter -= len;
            pos = segEnd;
        }

        // Medidores
        for (int t = 0; t < kTracks; ++t) meters.step[t].store (clock.stepOf (t), std::memory_order_relaxed);
        meters.pattern.store (clock.pattern(), std::memory_order_relaxed);
        meters.playing.store (playing, std::memory_order_relaxed);
        meters.barFx.store (barFx.activeBits(), std::memory_order_relaxed);
        meters.fill.store (clock.fillActive() && playing, std::memory_order_relaxed);
        meters.bpm.store (curBpm, std::memory_order_relaxed);
        float pl = 0, pr = 0;
        for (int i = 0; i < n; ++i) { pl = std::max (pl, std::fabs (outL[i])); pr = std::max (pr, std::fabs (outR[i])); }
        auto fall = [n, this] (std::atomic<float>& m, float db) {
            m.store (std::max (db, m.load (std::memory_order_relaxed) - 24.0f * (float) n / (float) fs), std::memory_order_relaxed);
        };
        fall (meters.outL, gainToDb (pl, -100.0f));
        fall (meters.outR, gainToDb (pr, -100.0f));
        meters.drumGr.store (drumComp.reductionDb(), std::memory_order_relaxed);
        meters.masterGr.store (masterComp.reductionDb(), std::memory_order_relaxed);
        meters.limGr.store (limiter.getReductionDb(), std::memory_order_relaxed);
        sampleClock += n;
    }

    void resetForRender (double ppq) noexcept
    {
        // Primero los valores de este lugar del tema (los suavizadores quedan fijos en ellos)
        for (auto& l : trackLocks) l.n = 0;
        lastFilter = 99.0f; lastReso = -1.0f;
        lastDlyTone = lastRevSize = lastRevDecay = -1.0f;
        lastEq[0] = lastEq[1] = lastEq[2] = lastEq[3] = -999.0f;
        updateControl (ppq, true);
        controlCounter = kControl;
        for (int t = 0; t < kTracks; ++t) { if (voices[t]) voices[t]->resetState(); quiet[t] = 0; mixState[t] = target[t]; }
        chord.resetState();
        delay.clear(); reverb.clear();
        drumComp.resetState(); masterComp.resetState();
        drumSatL.resetState(); drumSatR.resetState(); warmL.resetState(); warmR.resetState();
        clipL.reset(); clipR.reset();
        limiter.reset();
        texture.resetState();
        barFx.reset();
        synthAlignL.clear(); synthAlignR.clear(); dryL.clear(); dryR.clear();
        djL.reset(); djR.reset();
        for (int c = 0; c < 2; ++c) { hp1[c].reset(); hp2[c].reset(); body[c].reset(); pres[c].reset(); air[c].reset(); }
        pumpEnv = 0.0f; pumpRise = false;
        // Portadora del ring: la fase sale de la posición del tema
        const double secs = ppq * 60.0 / std::max (1.0, curBpm);
        const double cyc = secs * (double) ringFreq;
        ringPhL = ringPhR = (float) (cyc - std::floor (cyc));
        ringMix = ringMixTarget;
        vol = volTarget;
        lastTextureBar = INT64_MIN;
    }

    void fire (const TriggerEvent& ev, double ppqAt) noexcept
    {
        const int t = ev.track;
        if (t < 0 && ev.bfxBar != -1) { startBarFx (ev.bfxBar, ev.bfxBeats); return; }
        if (t < 0 || t >= kTracks) return;
        quiet[t] = 0;
        if (ev.release)
        {
            if (t == Chord) chord.release();
            else if (voices[t]) voices[t]->release();
            return;
        }
        // P-locks: los valores propios del paso valen hasta el próximo golpe de la pista
        if (ev.locks || trackLocks[t].n > 0)
        {
            auto& tl = trackLocks[t];
            tl.n = 0;
            if (ev.locks)
                for (int k = 0; k < kMaxLocks; ++k)
                {
                    const Lock l = bank.getLock (ev.pattern, t, ev.stepIndex, k);
                    if (l.param < 0 || l.param >= (int) base.size()) continue;
                    tl.param[tl.n] = l.param; tl.norm[tl.n] = l.value; ++tl.n;
                }
            updateControl (ppqAt, playing);
            mixState[t] = target[t];   // el nivel y el paneo del lock valen desde el golpe, sin rampa
        }
        TriggerEvent e = ev;
        if (e.gateBeats > 0.0) e.hit.gateSamples = e.gateBeats * 60.0 / curBpm * fs;
        if (t == Chord) chord.trigger (e.hit);
        else if (voices[t])
        {
            voices[t]->trigger (e.hit);
            if (t == HatC) hatO.choke();
            if (t == Kick) { pumpRise = true; }
        }
        meters.triggers[t].fetch_add (1, std::memory_order_relaxed);
        triggerLog.push (sampleClock + e.sample, t);
    }

    void startBarFx (int bar, double beats) noexcept
    {
        const double len = beats * 60.0 / std::max (1.0, curBpm) * fs;
        const double slice = (base[P::BfxStutterRate] > 0.5f ? 0.125 : 0.25) * 60.0 / std::max (1.0, curBpm) * fs;
        const float amount = eff[P::BfxAmount];
        for (int k = 0; k < P::kBfx; ++k)
        {
            const float prob = eff[(size_t) P::bfx (k, P::BfxProb)];
            if (prob <= 0.0f) continue;
            float roll;
            if (fixedRandom) roll = hashUnit (hash32 (randomSeed, 0x42465800u + (uint32_t) k, (uint32_t) bar, 0x5EEDu));
            else roll = lfoRng.uni();
            if (roll >= prob) continue;
            const int point = clampv ((int) base[(size_t) P::bfx (k, P::BfxTarget)], 0, PointCount - 1);
            barFx.start (k, point, len, amount, slice, hash32 (randomSeed, (uint32_t) bar, (uint32_t) k, 0x52u));
        }
    }

    // ------------------------------------------------------------------
    // Modulaciones (cada 32 muestras)
    // ------------------------------------------------------------------
    float lfoValue (int shape, double phase, int64_t cycle, int l)
    {
        const float ph = (float) phase;
        switch (shape)
        {
            case 0: return std::sin ((float) kTwoPi * ph);
            case 1: return ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph;
            case 2: return 2.0f * ph - 1.0f;
            case 3: return 1.0f - 2.0f * ph;
            case 4: return ph < 0.5f ? 1.0f : -1.0f;
            case 5:
            case 6:
            {
                if (fixedRandom)
                {
                    // Azar FIJO: el valor de cada ciclo sale de (semilla, LFO, ciclo)
                    lfoHold[l] = hashBipolar (hash32 (randomSeed, 0x4C464Fu + (uint32_t) l, (uint32_t) cycle, (uint32_t) (cycle >> 32)));
                    lfoPrev[l] = hashBipolar (hash32 (randomSeed, 0x4C464Fu + (uint32_t) l, (uint32_t) (cycle - 1), (uint32_t) ((cycle - 1) >> 32)));
                    lfoCycle[l] = cycle;
                }
                else if (cycle != lfoCycle[l]) { lfoPrev[l] = lfoHold[l]; lfoHold[l] = lfoRng.next(); lfoCycle[l] = cycle; }
                if (shape == 5) return lfoHold[l];
                const float s = ph * ph * (3.0f - 2.0f * ph);
                return lfoPrev[l] + (lfoHold[l] - lfoPrev[l]) * s;
            }
            default: return 0.0f;
        }
    }

    void updateControl (double ppq, bool isPlaying) noexcept
    {
        const auto& defs = paramDefs();
        fixedRandom = ! exactLegacy && base[P::RandomMode] < 0.5f;
        randomSeed = (uint32_t) std::lround (base[P::Seed]);
        if (! isPlaying) ppq = stoppedPpq += (double) kControl * curBpm / 60.0 / fs; // los LFO siguen girando
        else stoppedPpq = ppq;

        // Fuentes
        float src[SrcCount] {};
        for (int l = 0; l < P::kLfos; ++l)
        {
            const double beats = lfoRateBeats ((int) base[(size_t) P::lfo (l, P::LfoRate)]);
            const double x = ppq / beats;
            const int64_t cycle = (int64_t) std::floor (x);
            const float v = lfoValue ((int) base[(size_t) P::lfo (l, P::LfoShape)], x - (double) cycle, cycle, l);
            src[SrcLfo1 + l] = v;
            meters.lfo[l].store (v, std::memory_order_relaxed);
        }
        const int64_t stepIdx = (int64_t) std::floor (ppq * 4.0);
        if (fixedRandom)
        {
            randStep = hashBipolar (hash32 (randomSeed, 0x52535445u, (uint32_t) stepIdx, (uint32_t) (stepIdx >> 32)));
            randPrev = hashBipolar (hash32 (randomSeed, 0x52535445u, (uint32_t) (stepIdx - 1), (uint32_t) ((stepIdx - 1) >> 32)));
            lastRandStep = stepIdx;
        }
        else if (stepIdx != lastRandStep) { randPrev = randStep; randStep = lfoRng.next(); lastRandStep = stepIdx; }
        src[SrcRandStep] = randStep;
        const float stepFrac = (float) (ppq * 4.0 - std::floor (ppq * 4.0));
        src[SrcRandSmooth] = randPrev + (randStep - randPrev) * stepFrac;
        src[SrcKickEnv] = pumpEnv;
        // Macros como fuente (con recorrido x2, el doble alrededor del centro)
        const bool doubleDepth = base[P::MacroDepth] > 0.5f;
        const float depth = doubleDepth ? 2.0f : 1.0f;
        for (int m = 0; m < 4; ++m)
        {
            const float v = base[(size_t) (P::Macro1 + m)];
            src[SrcMacro1 + m] = doubleDepth ? 0.5f + macroAmount (v) : v;
        }

        std::fill (modNorm.begin(), modNorm.end(), 0.0f);
        const float movement = movementScale (base[P::Macro2], doubleDepth); // MOVIMIENTO escala toda la modulación
        const int numParams = (int) base.size();
        for (int s = 0; s < P::kSlots; ++s)
        {
            const int si = routing.src[s].load (std::memory_order_relaxed);
            const int di = routing.dst[s].load (std::memory_order_relaxed);
            const float amt = base[(size_t) P::mod (s, P::ModAmt)];
            if (si <= SrcNone || si >= SrcCount || di < 0 || di >= numParams || amt == 0.0f) continue;
            const bool isMacro = si >= SrcMacro1;
            modNorm[(size_t) di] += src[si] * amt * (isMacro ? 1.0f : movement);
        }
        // Carriles dibujados
        for (int l = 0; l < kLanes; ++l)
        {
            const int bars = clampv ((int) base[(size_t) P::lane (l, P::LaneBars)], 1, 8);
            const double len = 4.0 * bars;
            double ph = std::fmod (ppq, len) / len;
            if (ph < 0) ph += 1.0;
            meters.lanePhase[l].store ((float) ph, std::memory_order_relaxed);
            const int di = routing.laneDst[l].load (std::memory_order_relaxed);
            if (di < 0 || di >= numParams) continue;
            const float fi = (float) ph * kLanePoints;
            const int i0 = (int) fi % kLanePoints, i1 = (i0 + 1) % kLanePoints;
            const float fr = fi - std::floor (fi);
            const float v = bank.lane (l, i0) + (bank.lane (l, i1) - bank.lane (l, i0)) * fr;
            modNorm[(size_t) di] += (v - 0.5f) * 2.0f * base[(size_t) P::lane (l, P::LaneDepth)] * movement;
        }
        // Macros fijas (con recorrido x2, el doble que en la 1.0)
        const float calor = macroAmount (base[P::Macro1]) * depth;
        const float espacio = macroAmount (base[P::Macro3]) * depth;
        const float crudeza = macroAmount (base[P::Macro4]) * depth;
        modNorm[(size_t) P::sound (Chord, 2)] += 0.22f * calor;
        modNorm[(size_t) P::sound (Bass, 2)] += 0.18f * calor;
        modNorm[(size_t) P::sound (Bass, 5)] += 0.15f * calor;
        modNorm[(size_t) P::MWarmth] += 0.35f * calor;
        modNorm[(size_t) P::sound (Kick, 5)] += 0.12f * calor;
        modNorm[(size_t) P::DlyRet] += 0.35f * espacio;
        modNorm[(size_t) P::RevRet] += 0.35f * espacio;
        modNorm[(size_t) P::RevSize] += 0.25f * espacio;
        modNorm[(size_t) P::track (Chord, P::Reverb)] += 0.2f * espacio;
        modNorm[(size_t) P::BusCrudo] += 0.4f * crudeza;
        modNorm[(size_t) P::BusComp] += 0.3f * crudeza;
        for (int t = 0; t <= Perc; ++t) modNorm[(size_t) P::sound (t, 5)] += 0.2f * crudeza;
        // MOVIMIENTO por encima del medio también mueve cosas solo (aunque el preset no tenga LFOs)
        const float extra = doubleDepth ? std::max (0.0f, base[P::Macro2] - 0.5f) * 2.0f : 0.0f;
        if (extra > 0.0f)
        {
            const double bars2 = ppq / 8.0, bars4 = ppq / 16.0;
            const float slow = (float) std::sin (kTwoPi * (bars2 - std::floor (bars2)));
            const float tri = (float) (bars4 - std::floor (bars4));
            const float tri4 = tri < 0.5f ? 4.0f * tri - 1.0f : 3.0f - 4.0f * tri;
            const float stepRnd = fixedRandom ? hashBipolar (hash32 (randomSeed, 0x4D4F56u, (uint32_t) stepIdx, (uint32_t) (stepIdx >> 32)))
                                              : randStep;
            modNorm[(size_t) P::sound (Chord, 2)] += 0.12f * extra * slow;
            modNorm[(size_t) P::sound (Bass, 4)] += 0.10f * extra * tri4;
            modNorm[(size_t) P::sound (HatC, 1)] += 0.10f * extra * stepRnd;
            modNorm[(size_t) P::sound (Perc, 0)] += 0.05f * extra * src[SrcRandSmooth];
        }

        // Valores efectivos (los de elección modulables se cuantizan)
        for (size_t i = 0; i < base.size(); ++i)
        {
            const bool canMod = defs[i].kind == Kind::Float || (defs[i].kind == Kind::Choice && defs[i].modulatable);
            if (modNorm[i] == 0.0f || ! canMod) { eff[i] = base[i]; continue; }
            eff[i] = defs[i].fromNorm (defs[i].toNorm (base[i]) + modNorm[i]);
        }
        // P-locks (encima de la modulación)
        for (int t = 0; t < kTracks; ++t)
            for (int k = 0; k < trackLocks[t].n; ++k)
            {
                const size_t i = (size_t) trackLocks[t].param[k];
                const bool canMod = defs[i].kind == Kind::Float || (defs[i].kind == Kind::Choice && defs[i].modulatable);
                eff[i] = defs[i].fromNorm (trackLocks[t].norm[k] + (canMod ? modNorm[i] : 0.0f));
            }

        // Voces
        const int root = (int) eff[P::Root];
        bass.setRoot (root);
        chord.setRoot (root);
        bass.setEnv (eff[(size_t) P::track (Bass, P::EnvA)], eff[(size_t) P::track (Bass, P::EnvS)], eff[(size_t) P::track (Bass, P::EnvR)]);
        chord.setEnv (eff[(size_t) P::track (Chord, P::EnvS)], eff[(size_t) P::track (Chord, P::EnvR)]);
        const double songSeconds = ppq * 60.0 / std::max (1.0, curBpm);
        for (int t = 0; t < kTracks; ++t)
        {
            if (t == Chord) { chord.setParams (&eff[(size_t) P::sound (t, 0)]); chord.setSongTime (songSeconds, fixedRandom, randomSeed); }
            else { voices[t]->setParams (&eff[(size_t) P::sound (t, 0)]); voices[t]->setSongTime (songSeconds, fixedRandom, randomSeed); }
        }
        // Azar FIJO: la textura de vinilo arranca cada compás desde su semilla
        const int64_t bar = (int64_t) std::floor (ppq / 4.0);
        if (fixedRandom && bar != lastTextureBar) { texture.reseed (hash32 (randomSeed, 0x54455854u, (uint32_t) bar, (uint32_t) (bar >> 32))); lastTextureBar = bar; }

        // Mezcla por pista
        bool anySolo = false;
        for (int t = 0; t < kTracks; ++t) anySolo |= eff[(size_t) P::track (t, P::Solo)] > 0.5f;
        for (int t = 0; t < kTracks; ++t)
        {
            const bool audible = eff[(size_t) P::track (t, P::Mute)] < 0.5f && (! anySolo || eff[(size_t) P::track (t, P::Solo)] > 0.5f);
            const float g = audible ? levelGain (eff[(size_t) P::track (t, P::Level)]) : 0.0f;
            const float pan = eff[(size_t) P::track (t, P::Pan)];
            const float a = pan * (float) kPi * 0.5f;
            target[t].gL = g * std::cos (a) * 1.4142f;
            target[t].gR = g * std::sin (a) * 1.4142f;
            target[t].dly = audible ? eff[(size_t) P::track (t, P::Delay)] : 0.0f;
            target[t].rev = audible ? eff[(size_t) P::track (t, P::Reverb)] : 0.0f;
            pumpAmt[t] = eff[(size_t) P::track (t, P::Pump)];
            ringAmt[t] = eff[(size_t) P::track (t, P::Ring)];
        }

        // Ring mod
        ringOn = eff[P::RingOn] > 0.5f;
        ringFreq = eff[P::RingFreq];
        ringWave = (int) eff[P::RingWave];
        ringMixTarget = ringOn ? eff[P::RingMix] : 0.0f;
        ringSpread = eff[P::RingSpread];

        // FX
        delay.setTimeSeconds ((float) (60.0 / curBpm * delayDivBeats ((int) eff[P::DlyDiv])));
        delay.setFeedback (barFx.delayBoost() > 0.0f ? std::min (0.95f, eff[P::DlyFb] + barFx.delayBoost()) : eff[P::DlyFb]);
        delay.setMix (eff[P::DlyRet]);
        delay.setDuck (0.25f);
        if (std::fabs (eff[P::DlyTone] - lastDlyTone) > 0.005f) { delay.setTone (eff[P::DlyTone]); lastDlyTone = eff[P::DlyTone]; }
        if (std::fabs (eff[P::RevSize] - lastRevSize) > 0.005f) { reverb.setSize (eff[P::RevSize]); lastRevSize = eff[P::RevSize]; }
        if (std::fabs (eff[P::RevDecay] - lastRevDecay) > 0.01f) { reverb.setDecay (eff[P::RevDecay]); lastRevDecay = eff[P::RevDecay]; }
        reverb.setMix (eff[P::RevRet]);
        reverb.setDuck (0.0f);
        reverb.setPreDelayMs (12.0f);
        pumpRelCoef = decayCoef (eff[P::PumpRel], fs);

        // Buses
        drumComp.set (eff[P::BusComp], 0.6f, true);
        drumSatL.setDrive (eff[P::BusCrudo]);
        drumSatR.setDrive (eff[P::BusCrudo]);
        masterComp.set (eff[P::MComp], eff[P::MColor], false);
        warmL.setDrive (eff[P::MWarmth]);
        warmR.setDrive (eff[P::MWarmth]);
        parallel = eff[P::MParallel];
        textureAmt = eff[P::MTexture];
        volTarget = dbToGain (eff[P::MVol]);
        const float ceil = eff[P::MCeiling];
        limiter.setCeilingDb (ceil);
        clipL.setThreshold (dbToGain (ceil) * 1.1f);
        clipR.setThreshold (dbToGain (ceil) * 1.1f);

        // Filtro DJ (bipolar)
        const float flt = eff[P::MFilter];
        const float reso = eff[P::MReso];
        if (std::fabs (flt - lastFilter) > 0.002f || std::fabs (reso - lastReso) > 0.005f)
        {
            lastFilter = flt; lastReso = reso;
            const float amt = std::fabs (flt);
            const float q = 0.707f + reso * 4.0f * std::min (1.0f, amt * 4.0f);
            if (flt < 0.0f) { djHz = 20000.0f * std::pow (130.0f / 20000.0f, amt); djMode = 0; }
            else { djHz = 15.0f * std::pow (4500.0f / 15.0f, amt); djMode = 1; }
            djL.set (fs, djHz, q); djR.set (fs, djHz, q);
        }
        // EQ estilo EVO SOL
        const float lc = eff[P::MLowCut], bd = eff[P::MBody], pr = eff[P::MPres], ar = eff[P::MAir];
        if (lc != lastEq[0]) { for (int c = 0; c < 2; ++c) { hp1[c].set (Biquad::Type::HighPass, fs, lc, 0.5412); hp2[c].set (Biquad::Type::HighPass, fs, lc, 1.3066); } lastEq[0] = lc; }
        if (bd != lastEq[1]) { for (int c = 0; c < 2; ++c) body[c].set (Biquad::Type::Peak, fs, 180.0, 0.8, bd); lastEq[1] = bd; }
        if (pr != lastEq[2]) { for (int c = 0; c < 2; ++c) pres[c].set (Biquad::Type::Peak, fs, 3500.0, 0.8, pr); lastEq[2] = pr; }
        if (ar != lastEq[3]) { for (int c = 0; c < 2; ++c) air[c].set (Biquad::Type::HighShelf, fs, 11000.0, 0.6, ar); lastEq[3] = ar; }
    }

    // ------------------------------------------------------------------
    // Render de un segmento (sin eventos en el medio)
    // ------------------------------------------------------------------
    void renderSegment (float* outL, float* outR, int len) noexcept
    {
        // Pistas en silencio: cuando la voz terminó y su cola ya no suena, no se calcula
        int activeList[kTracks];
        int numActive = 0;
        for (int t = 0; t < kTracks; ++t)
        {
            const bool sounding = t == Chord ? chord.isActive() : voices[t]->isActive();
            if (! exactLegacy && ! sounding && quiet[t] > kQuietSkip)
            {
                mixState[t] = target[t];
                continue;
            }
            activeList[numActive++] = t;
            float peak = 0.0f;
            if (t == Chord)
            {
                chord.render (chordL, chordR, len);
                for (int i = 0; i < len; ++i) peak = std::max ({ peak, std::fabs (chordL[i]), std::fabs (chordR[i]) });
            }
            else
            {
                voices[t]->render (trackBuf[t], len);
                for (int i = 0; i < len; ++i) peak = std::max (peak, std::fabs (trackBuf[t][i]));
            }
            if (sounding || peak > 1.0e-6f) quiet[t] = 0;
            else quiet[t] += len;
        }
        const float inv = 1.0f / (float) len;
        MixState stepMix[kTracks];
        for (int t = 0; t < kTracks; ++t)
        {
            stepMix[t].gL = (target[t].gL - mixState[t].gL) * inv;
            stepMix[t].gR = (target[t].gR - mixState[t].gR) * inv;
            stepMix[t].dly = (target[t].dly - mixState[t].dly) * inv;
            stepMix[t].rev = (target[t].rev - mixState[t].rev) * inv;
        }
        const float ringMixStep = (ringMixTarget - ringMix) * inv;
        const float ringIncL = ringFreq / (float) fs, ringIncR = ringFreq * (1.0f + ringSpread * 0.015f) / (float) fs;
        const float pumpAtk = 1.0f - (float) std::exp (-1.0 / (0.004 * fs));
        const bool fxOn = barFx.any();

        for (int i = 0; i < len; ++i)
        {
            // Envolvente de pump (sidechain fantasma del kick)
            if (pumpRise) { pumpEnv += pumpAtk * (1.05f - pumpEnv); if (pumpEnv >= 1.0f) { pumpEnv = 1.0f; pumpRise = false; } }
            else pumpEnv *= pumpRelCoef;

            // Portadora del ring mod
            ringMix += ringMixStep;
            float cL = 1.0f, cR = 1.0f;
            const bool ringActive = ringMix > 1e-4f;
            if (ringActive)
            {
                ringPhL += ringIncL; if (ringPhL >= 1.0f) ringPhL -= 1.0f;
                ringPhR += ringIncR; if (ringPhR >= 1.0f) ringPhR -= 1.0f;
                float phR = ringPhR + ringSpread * 0.25f; if (phR >= 1.0f) phR -= 1.0f;
                cL = carrier (ringPhL, ringIncL);
                cR = carrier (phR, ringIncR);
            }

            float dL = 0, dR = 0, sL = 0, sR = 0, sendD = 0, sendR = 0;
            for (int a = 0; a < numActive; ++a)
            {
                const int t = activeList[a];
                auto& m = mixState[t];
                m.gL += stepMix[t].gL; m.gR += stepMix[t].gR; m.dly += stepMix[t].dly; m.rev += stepMix[t].rev;
                float xl, xr;
                if (t == Chord) { xl = chordL[i]; xr = chordR[i]; }
                else xl = xr = trackBuf[t][i];
                const float pg = 1.0f - pumpAmt[t] * pumpEnv * 0.92f;
                xl *= m.gL * pg; xr *= m.gR * pg;
                if (ringActive && ringAmt[t] > 0.0f)
                {
                    const float rm = ringMix * ringAmt[t];
                    xl += rm * (xl * cL - xl);
                    xr += rm * (xr * cR - xr);
                }
                if (fxOn)
                {
                    const int pt = PointTrack0 + t;
                    barFx.process (pt, xl, xr);
                    const float throwMono = 0.5f * (xl + xr);
                    sendD += throwMono * barFx.delaySend (pt);
                    sendR += throwMono * barFx.reverbSend (pt);
                }
                const float mono = 0.5f * (xl + xr);
                sendD += mono * m.dly;
                sendR += mono * m.rev;
                if (isDrum (t)) { dL += xl; dR += xr; } else { sL += xl; sR += xr; }
            }

            if (fxOn)
            {
                sendD += 0.5f * (dL + dR) * barFx.delaySend (PointDrums) + 0.5f * (sL + sR) * barFx.delaySend (PointSynths)
                       + 0.5f * (dL + dR + sL + sR) * barFx.delaySend (PointMaster);
                sendR += 0.5f * (dL + dR) * barFx.reverbSend (PointDrums) + 0.5f * (sL + sR) * barFx.reverbSend (PointSynths)
                       + 0.5f * (dL + dR + sL + sR) * barFx.reverbSend (PointMaster);
                barFx.process (PointDrums, dL, dR);
                barFx.process (PointSynths, sL, sR);
            }
            // Drum bus: compresión fuerte + saturación cruda
            drumComp.process (dL, dR);
            dL = drumSatL.process (dL);
            dR = drumSatR.process (dR);
            // Alineamos el bus de synths con la latencia de la saturación
            synthAlignL.push (sL); synthAlignR.push (sR);
            sL = synthAlignL.readInt (AnalogSaturator::latencySamples());
            sR = synthAlignR.readInt (AnalogSaturator::latencySamples());

            float yL, yR, rL, rR;
            delay.process (sendD, yL, yR);
            reverb.process (sendR, rL, rR);

            float L = dL + sL + yL + rL * 2.2f;
            float R = dR + sR + yR + rR * 2.2f;
            if (fxOn) { barFx.process (PointMaster, L, R); barFx.advance(); }

            // Master: filtro DJ -> EQ -> compresión -> calor -> paralelo -> textura -> volumen -> clipper -> limitador
            if (std::fabs (lastFilter) > 0.002f)
            {
                float lp, bp, hp;
                djL.process (L, lp, bp, hp); L = djMode == 0 ? lp : hp;
                djR.process (R, lp, bp, hp); R = djMode == 0 ? lp : hp;
            }
            L = air[0].process (pres[0].process (body[0].process (hp2[0].process (hp1[0].process (L)))));
            R = air[1].process (pres[1].process (body[1].process (hp2[1].process (hp1[1].process (R)))));
            float wl = L, wr = R;
            masterComp.process (wl, wr);
            wl = warmL.process (wl);
            wr = warmR.process (wr);
            dryL.push (L); dryR.push (R);
            const float al = dryL.readInt (AnalogSaturator::latencySamples()), ar = dryR.readInt (AnalogSaturator::latencySamples());
            L = al + parallel * (wl - al);
            R = ar + parallel * (wr - ar);
            texture.process (textureAmt, L, R);
            vol += 0.002f * (volTarget - vol);
            L = clipL.process (L * vol);
            R = clipR.process (R * vol);
            limiter.process (L, R);
            outL[i] = sanitize (L);
            outR[i] = sanitize (R);
        }
        for (int c = 0; c < 2; ++c) { hp1[c].flush(); hp2[c].flush(); body[c].flush(); pres[c].flush(); air[c].flush(); }
        djL.flush(); djR.flush();
        drumSatL.flush(); drumSatR.flush(); warmL.flush(); warmR.flush();
        delay.flush(); reverb.flush();
    }

    inline float carrier (float ph, float inc) const noexcept
    {
        switch (ringWave)
        {
            case 1: return ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph;
            case 2: return blepSquare (ph, inc);
            default: return std::sin ((float) kTwoPi * ph);
        }
    }

    // ------------------------------------------------------------------
    static constexpr int kMaxEvents = 512;
    static constexpr int kQuietSkip = 8192;
    double fs = 44100.0;
    int maxBlockSize = 512;
    std::vector<float> base, eff, modNorm;

    KickVoice kick; ClapVoice clap; RimVoice rim; HatVoice hatC { false }, hatO { true }; PercVoice perc; BassVoice bass;
    ChordVoice chord;
    MonoVoice* voices[kTracks] {};

    StepClock clock;
    TriggerEvent events[kMaxEvents];
    uint32_t rngState = 0x12345u;
    bool internalPlay = false, playing = false;
    std::atomic<bool> internalPlayReq { false };
    std::atomic<uint32_t> auditionMask { 0u };
    std::atomic<bool> barFxNow { false };
    struct TrackLocks { int n = 0; int param[kMaxLocks] {}; float norm[kMaxLocks] {}; };
    TrackLocks trackLocks[kTracks];
    BarFx barFx;
    double internalPpq = 0.0, curPpq = 0.0, curBpm = 120.0, stoppedPpq = 0.0;
    int controlCounter = 0;
    int64_t sampleClock = 0; // muestras procesadas desde prepare()

    float trackBuf[kTracks][kControl] {};
    float chordL[kControl] {}, chordR[kControl] {};
    MixState mixState[kTracks], target[kTracks];
    float pumpAmt[kTracks] {};
    float ringAmt[kTracks] {};
    int quiet[kTracks] {};          // muestras seguidas en silencio (para saltear la voz)
    bool exactLegacy = false, fixedRandom = true;
    uint32_t randomSeed = 1;
    int64_t lastTextureBar = INT64_MIN;

    // Modulación
    Noise lfoRng { 0xABCDEFu };
    int64_t lfoCycle[P::kLfos] { -1, -1, -1, -1 };
    float lfoHold[P::kLfos] {}, lfoPrev[P::kLfos] {};
    int64_t lastRandStep = -1;
    float randStep = 0, randPrev = 0;
    float pumpEnv = 0, pumpRelCoef = 0.999f;
    bool pumpRise = false;

    // Ring
    bool ringOn = true;
    float ringFreq = 440, ringMix = 0, ringMixTarget = 0, ringSpread = 0.2f, ringPhL = 0, ringPhR = 0;
    int ringWave = 0;

    // FX y buses
    PingPongDelay delay;
    FdnReverb reverb;
    float lastDlyTone = -1, lastRevSize = -1, lastRevDecay = -1;
    LinkedBusComp drumComp, masterComp;
    AnalogSaturator drumSatL, drumSatR, warmL, warmR;
    OversampledClipper clipL, clipR;
    LookaheadLimiter limiter;
    Texture texture;
    DelayLine synthAlignL, synthAlignR, dryL, dryR;
    Svf djL, djR;
    int djMode = 0;
    float djHz = 20000.0f, lastFilter = 99.0f, lastReso = -1.0f;
    Biquad hp1[2], hp2[2], body[2], pres[2], air[2];
    float lastEq[4] {};
    float parallel = 1.0f, textureAmt = 0.0f, vol = 1.0f, volTarget = 1.0f;
};

} // namespace ogro2
