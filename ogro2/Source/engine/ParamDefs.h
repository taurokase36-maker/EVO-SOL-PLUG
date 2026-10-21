// OGRO 2 — tabla única de parámetros (la usan el motor, el plugin y la interfaz)
#pragma once

#include "VoiceMaps.h"
#include <cctype>
#include <functional>
#include <vector>

namespace ogro2
{
enum class Kind { Float, Choice, Bool };

struct ParamDef
{
    std::string id, name;
    Kind kind = Kind::Float;
    float min = 0.0f, max = 1.0f, def = 0.0f;
    bool log = false;
    std::vector<std::string> choices;
    std::function<std::string (float)> text;

    float toNorm (float v) const
    {
        if (kind == Kind::Bool) return v > 0.5f ? 1.0f : 0.0f;
        if (max <= min) return 0.0f;
        if (log) return clampv (std::log (std::max (v, min) / min) / std::log (max / min), 0.0f, 1.0f);
        return clampv ((v - min) / (max - min), 0.0f, 1.0f);
    }
    float fromNorm (float n) const
    {
        n = clampv (n, 0.0f, 1.0f);
        if (kind == Kind::Bool) return n > 0.5f ? 1.0f : 0.0f;
        if (kind == Kind::Choice) return (float) clampv ((int) std::floor (n * (float) choices.size()), 0, (int) choices.size() - 1);
        if (log) return min * std::pow (max / min, n);
        return min + (max - min) * n;
    }
};

// ---------------------------------------------------------------------------
// Índices
// ---------------------------------------------------------------------------
namespace P
{
    enum Global { Swing = 0, Pattern, Root, Tempo, Macro1, Macro2, Macro3, Macro4, GlobalCount };
    enum TrackField { S1 = 0, S2, S3, S4, S5, S6, S7, S8, Level, Pan, Delay, Reverb, Pump, Ring, Mute, Solo, TrackFieldCount };
    constexpr int TrackBase = GlobalCount;
    constexpr int track (int t, int f) { return TrackBase + t * TrackFieldCount + f; }
    constexpr int sound (int t, int p) { return track (t, p); }

    constexpr int kLfos = 4;
    enum LfoField { LfoShape = 0, LfoRate, LfoFieldCount };
    constexpr int LfoBase = TrackBase + kTracks * TrackFieldCount;
    constexpr int lfo (int l, int f) { return LfoBase + l * LfoFieldCount + f; }

    constexpr int kSlots = 12;
    enum ModField { ModSrc = 0, ModDst, ModAmt, ModFieldCount };
    constexpr int ModBase = LfoBase + kLfos * LfoFieldCount;
    constexpr int mod (int s, int f) { return ModBase + s * ModFieldCount + f; }

    enum LaneField { LaneDst = 0, LaneBars, LaneDepth, LaneFieldCount };
    constexpr int LaneBase = ModBase + kSlots * ModFieldCount;
    constexpr int lane (int l, int f) { return LaneBase + l * LaneFieldCount + f; }

    enum Ring { RingOn = LaneBase + 4 * LaneFieldCount, RingFreq, RingWave, RingMix, RingSpread,
                DlyDiv, DlyFb, DlyTone, DlyRet, RevSize, RevDecay, RevRet, PumpRel,
                BusComp, BusCrudo,
                MFilter, MReso, MLowCut, MBody, MPres, MAir, MComp, MColor, MWarmth, MParallel, MTexture, MCeiling, MVol,
                Count };
}

// ---------------------------------------------------------------------------
// Listas
// ---------------------------------------------------------------------------
inline const std::vector<std::string>& lfoShapeNames()
{
    static const std::vector<std::string> v { "Seno", "Triangulo", "Sierra +", "Sierra -", "Cuadrada", "S&H", "Azar suave" };
    return v;
}
inline const std::vector<std::string>& lfoRateNames()
{
    static const std::vector<std::string> v { "8 compases", "4 compases", "2 compases", "1 compas", "1/2", "1/4",
                                               "1/8", "1/16", "1/4 T", "1/8 T", "1/4 .", "1/8 ." };
    return v;
}
inline double lfoRateBeats (int i)
{
    static const double b[] = { 32, 16, 8, 4, 2, 1, 0.5, 0.25, 2.0 / 3.0, 1.0 / 3.0, 1.5, 0.75 };
    return b[clampv (i, 0, 11)];
}
inline const std::vector<std::string>& delayDivNames()
{
    static const std::vector<std::string> v { "1/2", "1/4 .", "1/4", "1/8 .", "1/8", "1/8 T", "1/16" };
    return v;
}
inline double delayDivBeats (int i)
{
    static const double b[] = { 2.0, 1.5, 1.0, 0.75, 0.5, 1.0 / 3.0, 0.25 };
    return b[clampv (i, 0, 6)];
}
inline const std::vector<std::string>& laneBarNames()
{
    static const std::vector<std::string> v { "1 compas", "2 compases", "4 compases", "8 compases" };
    return v;
}
inline int laneBars (int i) { static const int b[] = { 1, 2, 4, 8 }; return b[clampv (i, 0, 3)]; }

inline const std::vector<std::string>& modSourceNames()
{
    static const std::vector<std::string> v { "Nada", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "Azar por paso", "Azar suave",
                                               "Envolvente kick", "Macro 1", "Macro 2", "Macro 3", "Macro 4" };
    return v;
}
enum ModSource { SrcNone = 0, SrcLfo1, SrcLfo2, SrcLfo3, SrcLfo4, SrcRandStep, SrcRandSmooth, SrcKickEnv,
                 SrcMacro1, SrcMacro2, SrcMacro3, SrcMacro4, SrcCount };

inline const char* macroName (int m)
{
    static const char* n[] = { "CALOR", "MOVIMIENTO", "ESPACIO", "CRUDEZA" };
    return n[clampv (m, 0, 3)];
}

// Destinos de modulación: índice de parámetro (o -1 = nada) + nombre
struct ModDest { int param; std::string name; };

inline std::string capitalise (const char* s)
{
    std::string r (s);
    for (size_t i = 1; i < r.size(); ++i) r[i] = (char) std::tolower ((unsigned char) r[i]);
    return r;
}

inline const std::vector<ModDest>& modDests()
{
    static const std::vector<ModDest> v = [] {
        std::vector<ModDest> d;
        d.push_back ({ -1, "Nada" });
        for (int t = 0; t < kTracks; ++t)
        {
            const std::string tn = capitalise (trackName (t));
            for (int p = 0; p < kSoundParams; ++p)
                d.push_back ({ P::sound (t, p), tn + " " + capitalise (soundParamInfo (t, p).name) });
            d.push_back ({ P::track (t, P::Level), tn + " Nivel" });
            d.push_back ({ P::track (t, P::Pan), tn + " Paneo" });
            d.push_back ({ P::track (t, P::Delay), tn + " Delay" });
            d.push_back ({ P::track (t, P::Reverb), tn + " Reverb" });
        }
        d.push_back ({ P::MFilter, "Filtro DJ" });
        d.push_back ({ P::RingFreq, "Ring frecuencia" });
        d.push_back ({ P::RingMix, "Ring mezcla" });
        d.push_back ({ P::DlyFb, "Delay feedback" });
        d.push_back ({ P::DlyRet, "Delay retorno" });
        d.push_back ({ P::RevRet, "Reverb retorno" });
        d.push_back ({ P::RevSize, "Reverb tamano" });
        d.push_back ({ P::BusCrudo, "Drum bus crudo" });
        d.push_back ({ P::MWarmth, "Master calor" });
        return d;
    }();
    return v;
}

inline int destIndexForParam (int param)
{
    const auto& d = modDests();
    for (int i = 0; i < (int) d.size(); ++i) if (d[(size_t) i].param == param) return i;
    return 0;
}

// ---------------------------------------------------------------------------
// Tabla de parámetros
// ---------------------------------------------------------------------------
inline const std::vector<ParamDef>& paramDefs()
{
    static const std::vector<ParamDef> defs = [] {
        std::vector<ParamDef> v ((size_t) P::Count);
        auto pct = [] (float x) { return fmtPct (x); };
        auto db = [] (float x) { return fmt ("%+.1f dB", x); };
        auto F = [&] (int i, std::string id, std::string name, float mn, float mx, float def, bool log,
                      std::function<std::string (float)> text)
        {
            auto& p = v[(size_t) i];
            p.id = std::move (id); p.name = std::move (name); p.kind = Kind::Float;
            p.min = mn; p.max = mx; p.def = def; p.log = log; p.text = std::move (text);
        };
        auto C = [&] (int i, std::string id, std::string name, std::vector<std::string> choices, int def)
        {
            auto& p = v[(size_t) i];
            p.id = std::move (id); p.name = std::move (name); p.kind = Kind::Choice;
            p.min = 0; p.max = (float) choices.size() - 1; p.def = (float) def; p.choices = std::move (choices);
        };
        auto B = [&] (int i, std::string id, std::string name, bool def)
        {
            auto& p = v[(size_t) i];
            p.id = std::move (id); p.name = std::move (name); p.kind = Kind::Bool;
            p.min = 0; p.max = 1; p.def = def ? 1.0f : 0.0f;
        };

        // Global
        F (P::Swing, "swing", "Swing", 0.5f, 0.75f, 0.54f, false, [] (float x) { return fmt ("%.0f %%", x * 100.0); });
        C (P::Pattern, "pattern", "Patron", { "A", "B", "C", "D", "E", "F", "G", "H" }, 0);
        C (P::Root, "root", "Tonalidad", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 9);
        F (P::Tempo, "tempo", "Tempo interno", 60.0f, 180.0f, 124.0f, false, [] (float x) { return fmt ("%.1f BPM", x); });
        for (int m = 0; m < 4; ++m)
            F (P::Macro1 + m, "macro" + std::to_string (m + 1), capitalise (macroName (m)), 0.0f, 1.0f, 0.5f, false, pct);

        // Pistas
        for (int t = 0; t < kTracks; ++t)
        {
            const std::string pre = "t" + std::to_string (t + 1) + "_";
            const std::string tn = capitalise (trackName (t));
            for (int s = 0; s < kSoundParams; ++s)
            {
                const auto info = soundParamInfo (t, s);
                F (P::sound (t, s), pre + "p" + std::to_string (s + 1), tn + " " + capitalise (info.name), 0.0f, 1.0f, info.def, false,
                   [t, s] (float x) { return describeSound (t, s, x); });
            }
            F (P::track (t, P::Level), pre + "lvl", tn + " Nivel", 0.0f, 1.0f, 0.889f, false,
               [] (float x) { return x <= 0.001f ? std::string ("-inf") : fmt ("%+.1f dB", levelDb (x)); });
            F (P::track (t, P::Pan), pre + "pan", tn + " Paneo", 0.0f, 1.0f, 0.5f, false,
               [] (float x) { const int p = (int) std::lround ((x - 0.5f) * 200.0f); return p == 0 ? std::string ("C") : (p < 0 ? fmt ("%.0f L", -p) : fmt ("%.0f R", p)); });
            F (P::track (t, P::Delay), pre + "dly", tn + " Delay", 0.0f, 1.0f, 0.0f, false, pct);
            F (P::track (t, P::Reverb), pre + "rev", tn + " Reverb", 0.0f, 1.0f, 0.0f, false, pct);
            F (P::track (t, P::Pump), pre + "pump", tn + " Pump", 0.0f, 1.0f, t >= Bass ? 0.35f : 0.0f, false, pct);
            B (P::track (t, P::Ring), pre + "ring", tn + " Ring mod", false);
            B (P::track (t, P::Mute), pre + "mute", tn + " Mute", false);
            B (P::track (t, P::Solo), pre + "solo", tn + " Solo", false);
        }

        // LFOs
        for (int l = 0; l < P::kLfos; ++l)
        {
            const std::string pre = "lfo" + std::to_string (l + 1) + "_";
            C (P::lfo (l, P::LfoShape), pre + "shape", "LFO " + std::to_string (l + 1) + " Forma", lfoShapeNames(), l % 3);
            C (P::lfo (l, P::LfoRate), pre + "rate", "LFO " + std::to_string (l + 1) + " Velocidad", lfoRateNames(), 3 + l);
        }

        // Matriz
        std::vector<std::string> destNames;
        for (const auto& d : modDests()) destNames.push_back (d.name);
        for (int s = 0; s < P::kSlots; ++s)
        {
            const std::string pre = "mod" + std::to_string (s + 1) + "_";
            const std::string nm = "Mod " + std::to_string (s + 1);
            C (P::mod (s, P::ModSrc), pre + "src", nm + " Fuente", modSourceNames(), 0);
            C (P::mod (s, P::ModDst), pre + "dst", nm + " Destino", destNames, 0);
            F (P::mod (s, P::ModAmt), pre + "amt", nm + " Cantidad", -1.0f, 1.0f, 0.0f, false,
               [] (float x) { return fmt ("%+.0f %%", x * 100.0); });
        }

        // Carriles dibujables
        for (int l = 0; l < 4; ++l)
        {
            const std::string pre = "lane" + std::to_string (l + 1) + "_";
            const std::string nm = "Motion " + std::to_string (l + 1);
            C (P::lane (l, P::LaneDst), pre + "dst", nm + " Destino", destNames, 0);
            C (P::lane (l, P::LaneBars), pre + "bars", nm + " Largo", laneBarNames(), 1);
            F (P::lane (l, P::LaneDepth), pre + "depth", nm + " Profundidad", 0.0f, 1.0f, 0.5f, false, pct);
        }

        // Ring mod
        B (P::RingOn, "ring_on", "Ring mod", true);
        F (P::RingFreq, "ring_freq", "Ring frecuencia", 20.0f, 5000.0f, 440.0f, true, [] (float x) { return fmtHz (x); });
        C (P::RingWave, "ring_wave", "Ring forma", { "Seno", "Triangulo", "Cuadrada" }, 0);
        F (P::RingMix, "ring_mix", "Ring mezcla", 0.0f, 1.0f, 0.5f, false, pct);
        F (P::RingSpread, "ring_spread", "Ring estereo", 0.0f, 1.0f, 0.2f, false, pct);

        // FX
        C (P::DlyDiv, "dly_div", "Delay tiempo", delayDivNames(), 3);
        F (P::DlyFb, "dly_fb", "Delay feedback", 0.0f, 0.9f, 0.4f, false, pct);
        F (P::DlyTone, "dly_tone", "Delay tono", 0.0f, 1.0f, 0.45f, false, pct);
        F (P::DlyRet, "dly_ret", "Delay retorno", 0.0f, 1.0f, 0.6f, false, pct);
        F (P::RevSize, "rev_size", "Reverb tamano", 0.0f, 1.0f, 0.55f, false, pct);
        F (P::RevDecay, "rev_decay", "Reverb decay", 0.3f, 8.0f, 2.2f, true, [] (float x) { return fmt ("%.1f s", x); });
        F (P::RevRet, "rev_ret", "Reverb retorno", 0.0f, 1.0f, 0.6f, false, pct);
        F (P::PumpRel, "pump_rel", "Pump tiempo", 40.0f, 600.0f, 180.0f, true, [] (float x) { return fmtMs (x); });

        // Drum bus
        F (P::BusComp, "bus_comp", "Drum bus compresion", 0.0f, 1.0f, 0.6f, false, pct);
        F (P::BusCrudo, "bus_crudo", "Drum bus crudo", 0.0f, 1.0f, 0.35f, false, pct);

        // Master
        F (P::MFilter, "m_filter", "Filtro DJ", -1.0f, 1.0f, 0.0f, false,
           [] (float x) { return std::fabs (x) < 0.02f ? std::string ("Abierto") : (x < 0 ? fmt ("LP %.0f %%", -x * 100.0) : fmt ("HP %.0f %%", x * 100.0)); });
        F (P::MReso, "m_reso", "Filtro resonancia", 0.0f, 1.0f, 0.25f, false, pct);
        F (P::MLowCut, "m_lowcut", "Low-cut", 20.0f, 200.0f, 25.0f, true, [] (float x) { return fmtHz (x); });
        F (P::MBody, "m_body", "Cuerpo", -6.0f, 6.0f, 0.0f, false, db);
        F (P::MPres, "m_pres", "Presencia", -6.0f, 6.0f, 0.5f, false, db);
        F (P::MAir, "m_air", "Aire", 0.0f, 10.0f, 1.5f, false, db);
        F (P::MComp, "m_comp", "Compresion", 0.0f, 1.0f, 0.4f, false, pct);
        F (P::MColor, "m_color", "Color", 0.0f, 1.0f, 0.5f, false, pct);
        F (P::MWarmth, "m_warmth", "Calor", 0.0f, 1.0f, 0.3f, false, pct);
        F (P::MParallel, "m_parallel", "Mezcla", 0.0f, 1.0f, 1.0f, false, pct);
        F (P::MTexture, "m_texture", "Textura", 0.0f, 1.0f, 0.0f, false, pct);
        F (P::MCeiling, "m_ceiling", "Techo", -6.0f, 0.0f, -0.3f, false, db);
        F (P::MVol, "m_vol", "Volumen", -24.0f, 6.0f, 0.0f, false, db);

        for (auto& p : v)
            if (! p.text)
                p.text = [kind = p.kind, choices = p.choices] (float x) {
                    if (kind == Kind::Choice) return choices[(size_t) clampv ((int) std::lround (x), 0, (int) choices.size() - 1)];
                    if (kind == Kind::Bool) return std::string (x > 0.5f ? "ON" : "OFF");
                    return fmt ("%.2f", x);
                };
        return v;
    }();
    return defs;
}

inline int paramIndex (const std::string& id)
{
    const auto& d = paramDefs();
    for (int i = 0; i < (int) d.size(); ++i) if (d[(size_t) i].id == id) return i;
    return -1;
}

} // namespace ogro2
