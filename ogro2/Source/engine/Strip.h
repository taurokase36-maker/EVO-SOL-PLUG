// OGRO 2 — tira analógica por pista (fase 5) y saturación sin aliasing (ADAA).
//   DRIVE      cinta / válvula / transistor / diodo (2x sobremuestreo)
//   CUERPO     resonancia en graves y medios (frecuencia elegible)
//   RUIDO      piso de ruido analógico que sigue a la envolvente de la pista
//   TRANSIENTE más o menos ataque (envolvente rápida contra lenta)
//   BRILLO     exciter (armónicos de los agudos) + aire
//   COMP       VCA (RMS, suave) o FET (pico, rápido y con color), con makeup automático
// Todo en 0 = no se procesa (la pista suena exactamente igual que sin tira).
#pragma once

#include "../dsp/Dynamics.h"
#include "Voices.h"

namespace ogro2
{
// Sobremuestreo 2x con medio-banda de 15 coeficientes (polifásico)
class Os2
{
public:
    void reset() { for (auto& v : up) v = 0.0f; for (auto& v : dn) v = 0.0f; }
    template <typename F>
    inline float process (float x, F&& shape) noexcept
    {
        // Subida: muestra par = x filtrada, impar = interpolada
        for (int i = 7; i > 0; --i) up[i] = up[i - 1];
        up[0] = x;
        const float even = up[3];
        const float odd = h[0] * (up[0] + up[7]) + h[1] * (up[1] + up[6]) + h[2] * (up[2] + up[5]) + h[3] * (up[3] + up[4]);
        const float a = shape (even), b = shape (odd);
        // Bajada
        for (int i = 15; i > 1; --i) dn[i] = dn[i - 2];
        dn[1] = a; dn[0] = b;
        // Los coeficientes de la fase impar de un medio-banda valen la mitad al bajar
        return 0.5f * (dn[7] + h[0] * (dn[0] + dn[14]) + h[1] * (dn[2] + dn[12]) + h[2] * (dn[4] + dn[10]) + h[3] * (dn[6] + dn[8]));
    }

private:
    // Fase impar (x2) de un medio-banda de 15 coeficientes con ventana: suma 0.5
    static constexpr float h[4] = { -0.0110f, 0.0458f, -0.1426f, 0.6078f };
    float up[8] {}, dn[16] {};
};

class AnalogStrip
{
public:
    enum DriveType { Tape = 0, Tube, Transistor, Diode };
    struct Settings
    {
        float drive = 0.0f; int type = Tape;
        float body = 0.0f, bodyHz = 180.0f;
        float noise = 0.0f, transient = 0.0f, bright = 0.0f;
        float comp = 0.0f; bool fet = false;
        bool operator== (const Settings& o) const
        {
            return drive == o.drive && type == o.type && body == o.body && bodyHz == o.bodyHz && noise == o.noise
                   && transient == o.transient && bright == o.bright && comp == o.comp && fet == o.fet;
        }
    };

    void prepare (double sampleRate, uint32_t seed)
    {
        fs = sampleRate;
        rng.seed (seed);
        for (int c = 0; c < 2; ++c) { os[c].reset(); tapeLp[c].reset(); hp[c].reset(); air[c].reset(); body[c].reset(); }
        envF = envS = env = gr = 0.0f;
        lastBody = lastBodyHz = lastBright = -1.0f;
        aFastA = onePoleCoef (1.0f, fs); aFastR = onePoleCoef (25.0f, fs);
        aSlowA = onePoleCoef (25.0f, fs); aSlowR = onePoleCoef (220.0f, fs);
        aEnv = onePoleCoef (8.0f, fs);
        tapeLp[0].set (Biquad::Type::LowPass, fs, 15000.0, 0.6);
        tapeLp[1].set (Biquad::Type::LowPass, fs, 15000.0, 0.6);
        compPeakC = (float) std::exp (-1.0 / (0.002 * fs));
        configured = false;
    }
    void reset() noexcept
    {
        for (int c = 0; c < 2; ++c) { os[c].reset(); tapeLp[c].reset(); hp[c].reset(); air[c].reset(); body[c].reset(); }
        envF = envS = env = gr = compDet = 0.0f;
        compGain = compGainTarget = transGain = transTarget = 1.0f;
        ctl = 0;
    }
    // Llamar cada bloque de control (cambia coeficientes solo si hace falta)
    void set (const Settings& s)
    {
        if (configured && s == st) return;   // lo común: nada cambió desde el bloque anterior
        configured = true;
        st = s;
        active = s.drive > 0.001f || s.body > 0.001f || s.noise > 0.001f || std::fabs (s.transient) > 0.01f || s.bright > 0.001f || s.comp > 0.001f;
        if (s.body != lastBody || s.bodyHz != lastBodyHz)
        {
            for (auto& b : body) b.set (Biquad::Type::Peak, fs, s.bodyHz, 1.1, 9.0 * s.body);
            lastBody = s.body; lastBodyHz = s.bodyHz;
        }
        if (s.bright != lastBright)
        {
            for (auto& h2 : hp) h2.set (Biquad::Type::HighPass, fs, 3200.0, 0.707);
            for (auto& a : air) a.set (Biquad::Type::HighShelf, fs, 10000.0, 0.7, 4.0 * s.bright);
            lastBright = s.bright;
        }
        driveGain = 1.0f + s.drive * s.drive * 14.0f;
        driveNorm = 1.0f / std::sqrt (driveGain);
        // Compresor: el umbral baja con la cantidad; makeup automático
        thr = -2.0f - 28.0f * s.comp;
        ratio = s.fet ? 6.0f : 3.5f;
        makeup = dbToGain (-thr * (1.0f - 1.0f / ratio) * 0.45f);
        compA4 = onePoleCoef (s.fet ? 0.4f : 8.0f, fs / 4.0);
        compR4 = onePoleCoef (s.fet ? 70.0f : 160.0f, fs / 4.0);
    }
    bool isActive() const { return active; }

    // Procesa en el lugar (mono: r == nullptr, un solo canal)
    void process (float* l, float* r, int n) noexcept
    {
        if (! active) return;
        if (r != nullptr) processImpl<2> (l, r, n);
        else processImpl<1> (l, nullptr, n);
    }
    float reductionDb() const { return gr; }

private:
    // tanh por fracción continua (error < 1e-4): bastante más barata que std::tanh
    static inline float fastTanh (float x) noexcept
    {
        if (x > 4.97f) return 1.0f;
        if (x < -4.97f) return -1.0f;
        const float x2 = x * x;
        return x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2))) / (135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f)));
    }

    template <int C>
    void processImpl (float* l, float* r, int n) noexcept
    {
        float* ch[2] = { l, r };
        const bool doTrans = std::fabs (st.transient) > 0.01f, doBody = st.body > 0.001f, doDrive = st.drive > 0.001f;
        const bool doBright = st.bright > 0.001f, doComp = st.comp > 0.001f, doNoise = st.noise > 0.001f;
        for (int i = 0; i < n; ++i)
        {
            float x[2];
            float mono = 0.0f;
            for (int c = 0; c < C; ++c) { x[c] = ch[c][i]; mono += std::fabs (x[c]); }
            mono *= 1.0f / (float) C;
            // Transiente: envolvente rápida contra lenta (la ganancia se recalcula cada 8 muestras)
            if (doTrans)
            {
                envF += (mono > envF ? aFastA : aFastR) * (mono - envF);
                envS += (mono > envS ? aSlowA : aSlowR) * (mono - envS);
                if ((ctl & 7) == 0)
                {
                    const float ratioEnv = (envF + 1e-5f) / (envS + 1e-5f);
                    transTarget = clampv (std::exp2 (1.5f * st.transient * std::log2 (ratioEnv)), 0.25f, 4.0f);
                }
                transGain += 0.2f * (transTarget - transGain);
                for (int c = 0; c < C; ++c) x[c] *= transGain;
            }
            if (doBody) for (int c = 0; c < C; ++c) x[c] = body[c].process (x[c]);
            if (doDrive)
            {
                for (int c = 0; c < C; ++c)
                {
                    x[c] = os[c].process (x[c] * driveGain, [this] (float v) { return shape (v); }) * driveNorm;
                    if (st.type == Tape) x[c] = tapeLp[c].process (x[c]);
                }
            }
            if (doBright)
                for (int c = 0; c < C; ++c)
                {
                    const float h = hp[c].process (x[c]);
                    x[c] = air[c].process (x[c] + st.bright * 0.6f * fastTanh (h * 3.0f));
                }
            if (doComp)
            {
                float det;
                if (st.fet) { det = std::fabs (x[0]); if (C == 2) det = std::max (det, std::fabs (x[C - 1])); }
                else { float sq = 0.0f; for (int c = 0; c < C; ++c) sq += x[c] * x[c]; det = std::sqrt (sq / (float) C); }
                compDet = std::max (det, compDet * compPeakC);   // detector con caída corta
                if ((ctl & 3) == 0)
                {
                    const float want = gainComputerDb (gainToDb (compDet, -120.0f), thr, ratio, 6.0f);
                    gr += (want > gr ? compA4 : compR4) * (want - gr);
                    compGainTarget = dbToGain (-gr) * makeup;
                }
                compGain += 0.25f * (compGainTarget - compGain);
                for (int c = 0; c < C; ++c)
                {
                    x[c] *= compGain;
                    if (st.fet) x[c] = fastTanh (x[c] * 1.15f) * (1.0f / 1.15f);
                }
            }
            // Ruido: sigue a la envolvente de la pista (como el piso de una consola)
            if (doNoise)
            {
                env += aEnv * (mono - env);
                const float lvl = st.noise * 0.05f * std::sqrt (env + 1e-6f);
                x[0] += (rng.next() + rng.next() + rng.next()) * 0.33f * lvl;
                if (C == 2) x[C - 1] += rng.next() * lvl * 0.9f;
            }
            ++ctl;
            for (int c = 0; c < C; ++c) ch[c][i] = x[c];
        }
        for (int c = 0; c < C; ++c) { body[c].flush(); hp[c].flush(); air[c].flush(); tapeLp[c].flush(); }
    }

    inline float shape (float v) const noexcept
    {
        switch (st.type)
        {
            case Tube:   // asimétrica: armónicos pares
                return fastTanh (v + 0.25f * v * v * (v > 0.0f ? 1.0f : 0.6f)) * 0.95f;
            case Transistor:   // más dura: impares (rodilla algebraica, sin transcendentes)
            {
                const float v2 = v * v;
                return v / std::sqrt (std::sqrt (1.0f + v2 * v2));
            }
            case Diode:   // diodos: asimétrica y con rodilla exponencial
                return v >= 0.0f ? (1.0f - std::exp (-v)) : -(1.0f - std::exp (v * 1.4f)) * 0.75f;
            default:     // cinta: suave
                return fastTanh (v * 0.9f) * 1.05f;
        }
    }

    double fs = 44100.0;
    Settings st;
    bool active = false, configured = false;
    Os2 os[2];
    Biquad tapeLp[2], hp[2], air[2], body[2];
    float lastBody = -1.0f, lastBodyHz = -1.0f, lastBright = -1.0f;
    float driveGain = 1.0f, driveNorm = 1.0f;
    float envF = 0, envS = 0, env = 0, gr = 0;
    float aFastA = 0, aFastR = 0, aSlowA = 0, aSlowR = 0, aEnv = 0;
    float thr = -10.0f, ratio = 4.0f, makeup = 1.0f, compA4 = 0.1f, compR4 = 0.01f, compPeakC = 0.99f;
    float compDet = 0.0f, compGain = 1.0f, compGainTarget = 1.0f, transGain = 1.0f, transTarget = 1.0f;
    uint32_t ctl = 0;
    Noise rng;
};

} // namespace ogro2
