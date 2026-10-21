// OGRO 2 — tabla única de parámetros (la usan el motor, el plugin y la interfaz).
//
// REGLAS PARA QUE LAS SESIONES SIGAN ABRIENDO EN TODAS LAS 2.x
//  - El ID de texto de un parámetro ("bass.cutoff") no cambia nunca. Ableton guarda
//    la automatización y el estado por ese ID (JUCE lo convierte en un número fijo).
//  - El rango y el mapa físico de un parámetro existente no cambian.
//  - Las listas que pueden crecer nacen con lugares reservados ("—"): agregar una
//    opción no corre el valor normalizado que guarda la automatización.
//  - Los parámetros nuevos se agregan con "version" mayor que la última publicada.
//  - Los índices P::... son internos y pueden cambiar entre versiones (no se guardan).
#pragma once

#include "VoiceMaps.h"
#include <atomic>
#include <cctype>
#include <functional>
#include <limits>
#include <vector>

namespace ogro2
{
enum class Kind { Float, Choice, Bool, Int };

struct ParamDef
{
    std::string id, name;
    Kind kind = Kind::Float;
    float min = 0.0f, max = 1.0f, def = 0.0f;
    bool log = false;
    std::vector<std::string> choices; // con los lugares reservados incluidos
    int validChoices = 0;             // opciones reales (el resto son reservas)
    std::function<std::string (float)> text;
    int version = 1;                  // pista de versión de JUCE (2.0.0 = 1)
    bool modulatable = false;         // destino de MOD, MOTION y p-locks
    int track = -1;                   // pista dueña (-1 = global)
    bool exists = true;               // los campos de pista que no aplican no se crean
    // Valor que reproduce cómo sonaba una sesión guardada antes de que existiera este
    // parámetro (NaN = el valor por defecto). Lo usan la migración del estado y el importador.
    float compat = std::numeric_limits<float>::quiet_NaN();
    bool hasCompat() const { return compat == compat; }

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
        if (kind == Kind::Choice || kind == Kind::Int)
        {
            const int lo = (int) min, hi = kind == Kind::Choice ? std::max (0, validChoices - 1) : (int) max;
            return (float) clampv ((int) std::lround (min + n * (max - min)), lo, hi);
        }
        if (log) return min * std::pow (max / min, n);
        return min + (max - min) * n;
    }
};

// ---------------------------------------------------------------------------
// Índices internos
// ---------------------------------------------------------------------------
namespace P
{
    enum Global { Swing = 0, Pattern, Root, Tempo, Macro1, Macro2, Macro3, Macro4, Seed, RandomMode, MacroDepth,
                  PatChange, PolyMode, Chain, Groove, GrooveAmt, Humanize, Fill, FillPat, Genre, Mutate,
                  BfxEvery, BfxLen, BfxAmount, BfxStutterRate, Scale, Tonal, GlobalCount };
    enum TrackField { S1 = 0, S2, S3, S4, S5, S6, S7, S8, Level, Pan, Delay, Reverb, Pump, Ring, Mute, Solo,
                      EnvA, EnvS, EnvR, SwingOwn, SwingAmt, TrackFieldCount };
    constexpr int TrackBase = GlobalCount;
    constexpr int track (int t, int f) { return TrackBase + t * TrackFieldCount + f; }
    constexpr int sound (int t, int p) { return track (t, p); }

    constexpr int kLfos = 4;
    enum LfoField { LfoShape = 0, LfoRate, LfoFieldCount };
    constexpr int LfoBase = TrackBase + kTracks * TrackFieldCount;
    constexpr int lfo (int l, int f) { return LfoBase + l * LfoFieldCount + f; }

    constexpr int kSlots = 12;
    enum ModField { ModAmt = 0, ModFieldCount };
    constexpr int ModBase = LfoBase + kLfos * LfoFieldCount;
    constexpr int mod (int s, int f) { return ModBase + s * ModFieldCount + f; }

    constexpr int kMotion = 4;
    enum LaneField { LaneBars = 0, LaneDepth, LaneFieldCount };
    constexpr int LaneBase = ModBase + kSlots * ModFieldCount;
    constexpr int lane (int l, int f) { return LaneBase + l * LaneFieldCount + f; }

    // Efectos de fin de compás: probabilidad y destino de cada uno
    constexpr int kBfx = 7;
    enum BfxField { BfxProb = 0, BfxTarget, BfxFieldCount };
    constexpr int BfxBase = LaneBase + kMotion * LaneFieldCount;
    constexpr int bfx (int e, int f) { return BfxBase + e * BfxFieldCount + f; }

    enum Rest { RingOn = BfxBase + kBfx * BfxFieldCount, RingFreq, RingWave, RingMix, RingSpread,
                DlyDiv, DlyFb, DlyTone, DlyRet, RevSize, RevDecay, RevRet, PumpRel,
                BusComp, BusCrudo,
                MFilter, MReso, MLowCut, MBody, MPres, MAir, MComp, MColor, MWarmth, MParallel, MTexture, MCeiling, MVol,
                // Fase 3: bajo, acordes y melodía
                BassSub, BassOsc, BassAccent, BassLegato,
                ChordMode, ChordInv, ChordSpread, ChordStrum, ChordLead,
                MelVoices, MelOctave,
                Count };
}

// Prefijos de ID de cada pista (no cambian)
inline const char* trackId (int t)
{
    static const char* ids[] = { "kick", "clap", "rim", "hatc", "hato", "perc", "bass", "chord", "melody", "texture" };
    return ids[clampv (t, 0, kTracks - 1)];
}

// Sufijo de ID de cada perilla de sonido (no cambian)
inline const char* soundId (int t, int p)
{
    static const char* ids[kTracks][kSoundParams] = {
        { "tune", "decay", "punch", "curve", "click", "drive", "color", "varia" },
        { "tone", "decay", "spread", "bursts", "grain", "drive", "room", "varia" },
        { "tone", "decay", "noise", "body", "click", "drive", "bright", "varia" },
        { "tone", "decay", "metal", "filter", "reso", "drive", "air", "varia" },
        { "tone", "decay", "metal", "filter", "reso", "drive", "air", "varia" },
        { "tone", "decay", "fm", "ratio", "pitch", "drive", "noise", "varia" },
        { "octave", "decay", "cutoff", "reso", "envamt", "drive", "wave", "glide" },
        { "type", "decay", "cutoff", "reso", "envamt", "detune", "chorus", "attack" },
        { "model", "decay", "cutoff", "reso", "envamt", "timbre", "vibrato", "glide" },
        { "start", "length", "pitch", "cutoff", "reso", "envamt", "drive", "pitchenv" },
    };
    return ids[clampv (t, 0, kTracks - 1)][clampv (p, 0, kSoundParams - 1)];
}

inline std::string capitaliseName (const char* s)
{
    std::string r (s);
    for (size_t i = 1; i < r.size(); ++i) r[i] = (char) std::tolower ((unsigned char) r[i]);
    return r;
}

// ---------------------------------------------------------------------------
// Listas (con reservas para crecer sin romper la automatización)
// ---------------------------------------------------------------------------
inline std::vector<std::string> withReserve (std::vector<std::string> v, size_t total)
{
    while (v.size() < total) v.push_back ("—");
    return v;
}

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

// Fuentes de modulación: ID estable + nombre
struct ModSourceInfo { const char* id; const char* name; };
enum ModSource { SrcNone = 0, SrcLfo1, SrcLfo2, SrcLfo3, SrcLfo4, SrcRandStep, SrcRandSmooth, SrcKickEnv,
                 SrcMacro1, SrcMacro2, SrcMacro3, SrcMacro4, SrcCount };
inline const ModSourceInfo& modSource (int i)
{
    static const ModSourceInfo s[SrcCount] = {
        { "none", "Nada" }, { "lfo1", "LFO 1" }, { "lfo2", "LFO 2" }, { "lfo3", "LFO 3" }, { "lfo4", "LFO 4" },
        { "rand_step", "Azar por paso" }, { "rand_smooth", "Azar suave" }, { "kick_env", "Envolvente kick" },
        { "macro1", "Calor" }, { "macro2", "Movimiento" }, { "macro3", "Espacio" }, { "macro4", "Crudeza" },
    };
    return s[clampv (i, 0, SrcCount - 1)];
}
inline int modSourceIndex (const std::string& id)
{
    for (int i = 0; i < SrcCount; ++i) if (id == modSource (i).id) return i;
    return SrcNone;
}

// Plantillas de groove (la lista solo crece al final)
inline const std::vector<std::string>& grooveNames()
{
    static const std::vector<std::string> v { "Recto", "MPC 60", "Shuffle 909", "Suelto (Villalobos)", "Empuje (garage)" };
    return v;
}
// Géneros de INVENTAR
inline const std::vector<std::string>& genreNames()
{
    static const std::vector<std::string> v { "Minimal house", "Micro-house / rominimal", "Deep", "Detroit", "Garage" };
    return v;
}
// Efectos de fin de compás
inline const char* bfxId (int e)
{
    static const char* ids[] = { "stutter", "filter", "reverb", "delay", "tapestop", "crush", "riser" };
    return ids[clampv (e, 0, 6)];
}
inline const char* bfxName (int e)
{
    static const char* n[] = { "Stutter", "Barrido de filtro", "Tiro de reverb", "Tiro de delay", "Tape stop", "Bitcrush", "Riser de ruido" };
    return n[clampv (e, 0, 6)];
}
// Destinos de los efectos de fin de compás
inline std::vector<std::string> bfxTargetNames()
{
    std::vector<std::string> v { "Master", "Bateria", "Melodicos" };
    for (int t = 0; t < kTracks; ++t) v.push_back (capitaliseName (trackName (t)));
    return v;
}

inline const char* macroName (int m)
{
    static const char* n[] = { "CALOR", "MOVIMIENTO", "ESPACIO", "CRUDEZA" };
    return n[clampv (m, 0, 3)];
}
inline const char* macroId (int m)
{
    static const char* n[] = { "macro.calor", "macro.movimiento", "macro.espacio", "macro.crudeza" };
    return n[clampv (m, 0, 3)];
}

inline std::string capitalise (const char* s)
{
    std::string r (s);
    for (size_t i = 1; i < r.size(); ++i) r[i] = (char) std::tolower ((unsigned char) r[i]);
    return r;
}

inline std::string fmtSemis (float x) { return fmt ("%+.0f st", x); }

// ---------------------------------------------------------------------------
// Tabla de parámetros
// ---------------------------------------------------------------------------
inline const std::vector<ParamDef>& paramDefs()
{
    static const std::vector<ParamDef> defs = [] {
        std::vector<ParamDef> v ((size_t) P::Count);
        for (auto& p : v) p.exists = false;
        auto pct = [] (float x) { return fmtPct (x); };
        auto db = [] (float x) { return fmt ("%+.1f dB", x); };
        auto F = [&] (int i, std::string id, std::string name, float mn, float mx, float def, bool log,
                      std::function<std::string (float)> text, bool modulatable = false) -> ParamDef&
        {
            auto& p = v[(size_t) i];
            p.id = std::move (id); p.name = std::move (name); p.kind = Kind::Float; p.exists = true;
            p.min = mn; p.max = mx; p.def = def; p.log = log; p.text = std::move (text); p.modulatable = modulatable;
            return p;
        };
        auto C = [&] (int i, std::string id, std::string name, std::vector<std::string> choices, int def, size_t reserve = 0) -> ParamDef&
        {
            auto& p = v[(size_t) i];
            p.id = std::move (id); p.name = std::move (name); p.kind = Kind::Choice; p.exists = true;
            p.validChoices = (int) choices.size();
            p.choices = withReserve (std::move (choices), std::max (reserve, (size_t) p.validChoices));
            p.min = 0; p.max = (float) p.choices.size() - 1; p.def = (float) def;
            return p;
        };
        auto I = [&] (int i, std::string id, std::string name, int mn, int mx, int def, std::function<std::string (float)> text = {}) -> ParamDef&
        {
            auto& p = v[(size_t) i];
            p.id = std::move (id); p.name = std::move (name); p.kind = Kind::Int; p.exists = true;
            p.min = (float) mn; p.max = (float) mx; p.def = (float) def; p.text = std::move (text);
            return p;
        };
        auto B = [&] (int i, std::string id, std::string name, bool def) -> ParamDef&
        {
            auto& p = v[(size_t) i];
            p.id = std::move (id); p.name = std::move (name); p.kind = Kind::Bool; p.exists = true;
            p.min = 0; p.max = 1; p.def = def ? 1.0f : 0.0f;
            return p;
        };

        // Global
        F (P::Swing, "global.swing", "Swing", 0.5f, 0.75f, 0.54f, false, [] (float x) { return fmt ("%.0f %%", x * 100.0); });
        C (P::Pattern, "global.pattern", "Patron", { "A", "B", "C", "D", "E", "F", "G", "H" }, 0);
        C (P::Root, "global.root", "Tonalidad", { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 9);
        F (P::Tempo, "global.tempo", "Tempo interno", 60.0f, 180.0f, 127.0f, false, [] (float x) { return fmt ("%.1f BPM", x); });
        for (int m = 0; m < 4; ++m)
            F (P::Macro1 + m, macroId (m), capitalise (macroName (m)), 0.0f, 1.0f, 0.5f, false,
               [] (float x) { return fmt ("%+.0f %%", (x - 0.5) * 200.0); });
        I (P::Seed, "global.seed", "Semilla", 1, 9999, 4721, [] (float x) { return fmt ("#%.0f", x); });
        C (P::RandomMode, "global.random", "Azar", { "Fijo", "Vivo" }, 0);
        // Recorrido de las macros: x2 en OGRO 2; los presets importados de la 1.0 usan x1 (suenan igual)
        C (P::MacroDepth, "global.macrodepth", "Profundidad de macros", { "x1 (como la 1.0)", "x2" }, 1, 4);

        // Secuenciador y groove (fase 2)
        C (P::PatChange, "global.patchange", "Cambio de patron", { "Al compas", "Al final del patron", "Inmediato" }, 0, 4);
        C (P::PolyMode, "global.polymode", "Polirritmos", { "Desde el patron", "Continuos (1.0)" }, 0, 4);
        B (P::Chain, "global.chain", "Cadena de patrones", false);
        C (P::Groove, "global.groove", "Groove", grooveNames(), 0, 16);
        F (P::GrooveAmt, "global.grooveamt", "Groove cantidad", 0.0f, 1.0f, 1.0f, false, pct, true);
        F (P::Humanize, "global.humanize", "Humanizar tiempo", 0.0f, 20.0f, 0.0f, false, [] (float x) { return fmt ("%.1f ms", x); }, true);
        C (P::Fill, "global.fill", "Fill", { "Apagado", "Cada 2 compases", "Cada 4 compases", "Cada 8 compases", "Cada 16 compases" }, 0, 8);
        C (P::FillPat, "global.fillpat", "Fill patron", { "Solo condiciones", "A", "B", "C", "D", "E", "F", "G", "H" }, 4, 16);
        C (P::Genre, "global.genre", "Genero de INVENTAR", genreNames(), 0, 16);
        F (P::Mutate, "global.mutate", "Mutar", 0.0f, 1.0f, 0.25f, false, pct);
        C (P::BfxEvery, "bfx.every", "FX de fin de compas", { "Apagado", "Cada compas", "Cada 2 compases", "Cada 4 compases", "Cada 8 compases" }, 0, 8);
        I (P::BfxLen, "bfx.len", "FX de fin de compas largo", 1, 4, 2, [] (float x) { return fmt (x < 1.5f ? "%.0f paso" : "%.0f pasos", x); });
        F (P::BfxAmount, "bfx.amount", "FX de fin de compas cantidad", 0.0f, 1.0f, 0.7f, false, pct, true);
        C (P::BfxStutterRate, "bfx.stutter.rate", "Stutter division", { "1/16", "1/32" }, 0, 4);
        // Tonalidad (fase 3): con MODO TONAL la NOTA de cada paso es un grado de la escala
        auto& scl = C (P::Scale, "global.scale", "Escala", scaleNames(), 0, 16);
        auto& ton = B (P::Tonal, "global.tonal", "Modo tonal", true);
        scl.version = ton.version = 2;
        ton.compat = 0.0f;
        for (int e = 0; e < P::kBfx; ++e)
        {
            const std::string pre = std::string ("bfx.") + bfxId (e) + ".";
            F (P::bfx (e, P::BfxProb), pre + "prob", std::string (bfxName (e)) + " probabilidad", 0.0f, 1.0f, 0.0f, false, pct, true);
            C (P::bfx (e, P::BfxTarget), pre + "target", std::string (bfxName (e)) + " destino", bfxTargetNames(), 0, 16);
        }

        // Pistas
        for (int t = 0; t < kTracks; ++t)
        {
            const std::string pre = std::string (trackId (t)) + ".";
            const std::string tn = capitalise (trackName (t));
            for (int s = 0; s < kSoundParams; ++s)
            {
                const auto info = soundParamInfo (t, s);
                if (t == Chord && s == 0)
                {
                    auto& p = C (P::sound (t, s), pre + soundId (t, s), tn + " Tipo", chordTypeNames(), 1, 32);
                    p.modulatable = true; p.track = t;
                    continue;
                }
                if (t == Melody && s == 0)
                {
                    auto& p = C (P::sound (t, s), pre + soundId (t, s), tn + " Modelo", melodyModelNames(), 0, 16);
                    p.modulatable = true; p.track = t; p.version = 2;
                    continue;
                }
                auto& p = F (P::sound (t, s), pre + soundId (t, s), tn + " " + capitalise (info.name), 0.0f, 1.0f, info.def, false,
                             [t, s] (float x) { return describeSound (t, s, x); }, true);
                p.track = t;
                if (t >= Melody) p.version = 2;
            }
            auto& lvl = F (P::track (t, P::Level), pre + "level", tn + " Nivel", 0.0f, 1.0f, 0.889f, false,
                           [] (float x) { return x <= 0.001f ? std::string ("-inf") : fmt ("%+.1f dB", levelDb (x)); }, true);
            auto& pan = F (P::track (t, P::Pan), pre + "pan", tn + " Paneo", 0.0f, 1.0f, 0.5f, false,
                           [] (float x) { const int p = (int) std::lround ((x - 0.5f) * 200.0f); return p == 0 ? std::string ("C") : (p < 0 ? fmt ("%.0f L", -p) : fmt ("%.0f R", p)); }, true);
            auto& dly = F (P::track (t, P::Delay), pre + "dly", tn + " Delay", 0.0f, 1.0f, t == Melody ? 0.22f : 0.0f, false, pct, true);
            auto& rev = F (P::track (t, P::Reverb), pre + "rev", tn + " Reverb", 0.0f, 1.0f, t == Melody ? 0.18f : 0.0f, false, pct, true);
            auto& pmp = F (P::track (t, P::Pump), pre + "pump", tn + " Pump", 0.0f, 1.0f, t >= Bass ? 0.35f : 0.0f, false, pct, true);
            auto& rng = F (P::track (t, P::Ring), pre + "ring", tn + " Ring mod", 0.0f, 1.0f, 0.0f, false, pct, true);
            auto& mut = B (P::track (t, P::Mute), pre + "mute", tn + " Mute", false);
            auto& sol = B (P::track (t, P::Solo), pre + "solo", tn + " Solo", false);
            // Swing propio de la pista (si no, usa el global)
            auto& swo = B (P::track (t, P::SwingOwn), pre + "swingown", tn + " Swing propio", false);
            auto& swa = F (P::track (t, P::SwingAmt), pre + "swing", tn + " Swing", 0.5f, 0.75f, 0.54f, false,
                           [] (float x) { return fmt ("%.0f %%", x * 100.0); });
            for (auto* p : { &lvl, &pan, &dly, &rev, &pmp, &rng, &mut, &sol, &swo, &swa })
            {
                p->track = t;
                if (t >= Melody) p->version = 2;
            }
            // Envolvente con sustain (bajo, acordes, melodía y texturas). Con SUSTAIN en 0 suena como en la 1.0.
            if (t >= Bass)
            {
                if (t != Chord)
                {
                    auto& a = F (P::track (t, P::EnvA), pre + "attack", tn + " Ataque", 0.5f, 2000.0f, 2.0f, true, [] (float x) { return fmtMs (x); }, true);
                    a.track = t;
                    if (t >= Melody) a.version = 2;
                }
                auto& s = F (P::track (t, P::EnvS), pre + "sustain", tn + " Sustain", 0.0f, 1.0f, t == Texture ? 1.0f : 0.0f, false, pct, true);
                auto& r = F (P::track (t, P::EnvR), pre + "release", tn + " Release", 5.0f, 4000.0f, t == Bass ? 120.0f : 300.0f, true,
                             [] (float x) { return fmtMs (x); }, true);
                s.track = r.track = t;
                if (t >= Melody) s.version = r.version = 2;
            }
        }

        // LFOs
        for (int l = 0; l < P::kLfos; ++l)
        {
            const std::string pre = "lfo" + std::to_string (l + 1) + ".";
            C (P::lfo (l, P::LfoShape), pre + "shape", "LFO " + std::to_string (l + 1) + " Forma", lfoShapeNames(), l % 3, 16);
            C (P::lfo (l, P::LfoRate), pre + "rate", "LFO " + std::to_string (l + 1) + " Velocidad", lfoRateNames(), 3 + l, 24);
        }

        // Matriz: la fuente y el destino se guardan por ID en el estado; la cantidad es automatizable
        for (int s = 0; s < P::kSlots; ++s)
            F (P::mod (s, P::ModAmt), "mod" + std::to_string (s + 1) + ".amt", "Mod " + std::to_string (s + 1) + " Cantidad",
               -1.0f, 1.0f, 0.0f, false, [] (float x) { return fmt ("%+.0f %%", x * 100.0); });

        // Carriles MOTION: el destino y la curva se guardan en el estado
        for (int l = 0; l < P::kMotion; ++l)
        {
            const std::string pre = "motion" + std::to_string (l + 1) + ".";
            const std::string nm = "Motion " + std::to_string (l + 1);
            I (P::lane (l, P::LaneBars), pre + "bars", nm + " Largo", 1, 8, 2, [] (float x) { return fmt (x < 1.5f ? "%.0f compas" : "%.0f compases", x); });
            F (P::lane (l, P::LaneDepth), pre + "depth", nm + " Profundidad", 0.0f, 1.0f, 0.5f, false, pct);
        }

        // Ring mod
        B (P::RingOn, "ring.on", "Ring mod", true);
        F (P::RingFreq, "ring.freq", "Ring frecuencia", 20.0f, 5000.0f, 440.0f, true, [] (float x) { return fmtHz (x); }, true);
        C (P::RingWave, "ring.wave", "Ring forma", { "Seno", "Triangulo", "Cuadrada" }, 0, 8);
        F (P::RingMix, "ring.mix", "Ring mezcla", 0.0f, 1.0f, 0.5f, false, pct, true);
        F (P::RingSpread, "ring.spread", "Ring estereo", 0.0f, 1.0f, 0.2f, false, pct, true);

        // FX
        C (P::DlyDiv, "dly.div", "Delay tiempo", delayDivNames(), 3, 16);
        F (P::DlyFb, "dly.fb", "Delay feedback", 0.0f, 0.9f, 0.4f, false, pct, true);
        F (P::DlyTone, "dly.tone", "Delay tono", 0.0f, 1.0f, 0.45f, false, pct, true);
        F (P::DlyRet, "dly.ret", "Delay retorno", 0.0f, 1.0f, 0.6f, false, pct, true);
        F (P::RevSize, "rev.size", "Reverb tamano", 0.0f, 1.0f, 0.55f, false, pct, true);
        F (P::RevDecay, "rev.decay", "Reverb decay", 0.3f, 8.0f, 2.2f, true, [] (float x) { return fmt ("%.1f s", x); }, true);
        F (P::RevRet, "rev.ret", "Reverb retorno", 0.0f, 1.0f, 0.6f, false, pct, true);
        F (P::PumpRel, "pump.rel", "Pump tiempo", 40.0f, 600.0f, 180.0f, true, [] (float x) { return fmtMs (x); }, true);

        // Drum bus
        F (P::BusComp, "bus.comp", "Drum bus compresion", 0.0f, 1.0f, 0.6f, false, pct, true);
        F (P::BusCrudo, "bus.crudo", "Drum bus crudo", 0.0f, 1.0f, 0.35f, false, pct, true);

        // Master
        F (P::MFilter, "master.filter", "Filtro DJ", -1.0f, 1.0f, 0.0f, false,
           [] (float x) { return std::fabs (x) < 0.02f ? std::string ("Abierto") : (x < 0 ? fmt ("LP %.0f %%", -x * 100.0) : fmt ("HP %.0f %%", x * 100.0)); }, true);
        F (P::MReso, "master.reso", "Filtro resonancia", 0.0f, 1.0f, 0.25f, false, pct, true);
        F (P::MLowCut, "master.lowcut", "Low-cut", 20.0f, 200.0f, 25.0f, true, [] (float x) { return fmtHz (x); });
        F (P::MBody, "master.body", "Cuerpo", -6.0f, 6.0f, 0.0f, false, db);
        F (P::MPres, "master.pres", "Presencia", -6.0f, 6.0f, 0.5f, false, db);
        F (P::MAir, "master.air", "Aire", 0.0f, 10.0f, 1.5f, false, db);
        F (P::MComp, "master.comp", "Compresion", 0.0f, 1.0f, 0.4f, false, pct);
        F (P::MColor, "master.color", "Color", 0.0f, 1.0f, 0.5f, false, pct);
        F (P::MWarmth, "master.warmth", "Calor", 0.0f, 1.0f, 0.3f, false, pct, true);
        F (P::MParallel, "master.parallel", "Mezcla", 0.0f, 1.0f, 1.0f, false, pct);
        F (P::MTexture, "master.texture", "Textura", 0.0f, 1.0f, 0.0f, false, pct, true);
        F (P::MCeiling, "master.ceiling", "Techo", -6.0f, 0.0f, -0.3f, false, db);
        F (P::MVol, "master.vol", "Volumen", -24.0f, 6.0f, 0.0f, false, db);

        // Bajo (fase 3)
        {
            auto& sub = F (P::BassSub, "bass.sub", "Bajo Sub", 0.0f, 1.0f, 0.0f, false, pct, true);
            auto& osc = C (P::BassOsc, "bass.osc", "Bajo Forma", { "Clasica (ONDA)", "Pulso", "Supersierra", "Triangulo", "Seno FM" }, 0, 16);
            auto& acc = C (P::BassAccent, "bass.accent", "Bajo Acento", { "Clasico (1.0)", "303" }, 1, 4);
            auto& leg = B (P::BassLegato, "bass.legato", "Bajo Legato", false);
            acc.compat = 0.0f;
            for (auto* p : { &sub, &osc, &acc, &leg }) { p->track = Bass; p->version = 2; }
            osc.modulatable = true;
        }
        // Acordes (fase 3)
        {
            auto& mode = C (P::ChordMode, "chord.mode", "Acordes Modo", { "Diatonico", "Paralelo" }, 0, 4);
            auto& inv = C (P::ChordInv, "chord.inv", "Acordes Inversion", { "Fundamental", "1a inversion", "2a inversion", "3a inversion" }, 0, 8);
            auto& spr = F (P::ChordSpread, "chord.spread", "Acordes Apertura", 0.0f, 1.0f, 0.0f, false, pct, true);
            auto& str = F (P::ChordStrum, "chord.strum", "Acordes Strum", 0.0f, 80.0f, 0.0f, false, [] (float x) { return fmtMs (x); }, true);
            auto& vl = B (P::ChordLead, "chord.voicelead", "Acordes Voice leading", true);
            mode.compat = 1.0f;
            vl.compat = 0.0f;
            inv.modulatable = true;
            for (auto* p : { &mode, &inv, &spr, &str, &vl }) { p->track = Chord; p->version = 2; }
        }
        // Melodía (fase 3)
        {
            auto& vo = C (P::MelVoices, "melody.voices", "Melodia Voces", { "Poly (4)", "Mono", "Mono legato" }, 0, 4);
            auto& oc = C (P::MelOctave, "melody.octave", "Melodia Octava", { "-2", "-1", "0", "+1", "+2" }, 2, 8);
            for (auto* p : { &vo, &oc }) { p->track = Melody; p->version = 2; }
        }
        // Las sesiones de antes de la fase 2 tocaban los polirritmos continuos
        v[P::PolyMode].compat = 1.0f;

        for (auto& p : v)
            if (p.exists && ! p.text)
                p.text = [kind = p.kind, choices = p.choices] (float x) {
                    if (kind == Kind::Choice) return choices[(size_t) clampv ((int) std::lround (x), 0, (int) choices.size() - 1)];
                    if (kind == Kind::Bool) return std::string (x > 0.5f ? "ON" : "OFF");
                    if (kind == Kind::Int) return fmt ("%.0f", x);
                    return fmt ("%.2f", x);
                };
        return v;
    }();
    return defs;
}

inline int paramIndex (const std::string& id)
{
    static const std::vector<std::pair<std::string, int>> sorted = [] {
        std::vector<std::pair<std::string, int>> s;
        const auto& d = paramDefs();
        for (int i = 0; i < (int) d.size(); ++i) if (d[(size_t) i].exists) s.push_back ({ d[(size_t) i].id, i });
        std::sort (s.begin(), s.end());
        return s;
    }();
    auto it = std::lower_bound (sorted.begin(), sorted.end(), std::make_pair (id, -1));
    return (it != sorted.end() && it->first == id) ? it->second : -1;
}

// Ruteo de la matriz MOD y de los carriles MOTION. Se guarda por ID en el estado (no son
// parámetros del host), así agregar destinos en una 2.x no corre nada.
struct ModRouting
{
    std::atomic<int> src[P::kSlots];   // índice de modSource()
    std::atomic<int> dst[P::kSlots];   // índice interno de parámetro (-1 = nada)
    std::atomic<int> laneDst[P::kMotion];
    ModRouting() { clear(); }
    void clear()
    {
        for (auto& v : src) v.store (SrcNone);
        for (auto& v : dst) v.store (-1);
        for (auto& v : laneDst) v.store (-1);
    }
};

// Destinos de modulación (MOD y MOTION) en el orden de los menús: por pista y global
struct ModDest { int param; std::string name; };
inline const std::vector<ModDest>& modDests()
{
    static const std::vector<ModDest> v = [] {
        std::vector<ModDest> d;
        const auto& defs = paramDefs();
        for (int t = 0; t < kTracks; ++t)
            for (int i = 0; i < (int) defs.size(); ++i)
                if (defs[(size_t) i].exists && defs[(size_t) i].modulatable && defs[(size_t) i].track == t)
                    d.push_back ({ i, defs[(size_t) i].name });
        for (int i = 0; i < (int) defs.size(); ++i)
            if (defs[(size_t) i].exists && defs[(size_t) i].modulatable && defs[(size_t) i].track < 0)
                d.push_back ({ i, defs[(size_t) i].name });
        return d;
    }();
    return v;
}

} // namespace ogro2
