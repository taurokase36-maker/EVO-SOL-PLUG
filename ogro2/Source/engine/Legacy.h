// OGRO 2 — compatibilidad con OGRO 1.0: traduce un preset de la 1.0 (IDs, destinos de
// modulación por índice, tipo de acorde, ruteo del ring, macros y patrones) al formato
// de OGRO 2, para que suene igual. Lo usan el botón "Importar preset de OGRO 1.0" y
// los 12 presets de fábrica (escritos con los IDs de la 1.0).
#pragma once

#include "ParamDefs.h"
#include "Sequencer.h"
#include <cstdlib>
#include <utility>

namespace ogro2::v1
{
// Campos de pista de la 1.0 en su lista de destinos de modulación
enum DestField { Level = 8, Pan = 9, Delay = 10, Reverb = 11 };

// Índice de destino de la 1.0 (lista modDests() de la 1.0, congelada)
constexpr int dest (int t, int field) { return 1 + t * 12 + field; }
enum GlobalDest { MFilterDest = 97, RingFreqDest, RingMixDest, DlyFbDest, DlyRetDest, RevRetDest, RevSizeDest, BusCrudoDest, MWarmthDest };

// Destino de la 1.0 -> parámetro de OGRO 2 (-1 = nada)
inline int destParam (int idx)
{
    if (idx <= 0) return -1;
    if (idx <= 96)
    {
        const int t = (idx - 1) / 12, f = (idx - 1) % 12;
        if (f < 8) return P::sound (t, f);
        static const int fields[] = { P::Level, P::Pan, P::Delay, P::Reverb };
        return P::track (t, fields[f - 8]);
    }
    static const int globals[] = { P::MFilter, P::RingFreq, P::RingMix, P::DlyFb, P::DlyRet, P::RevRet, P::RevSize, P::BusCrudo, P::MWarmth };
    return idx - 97 < 9 ? globals[idx - 97] : -1;
}


struct State
{
    std::vector<float> values;
    int src[P::kSlots] {}, dst[P::kSlots] {}, laneDst[P::kMotion] {};
};

// Convierte los valores de un preset de la 1.0 (pares ID viejo / valor)
inline State convert (const std::vector<std::pair<std::string, float>>& old)
{
    const auto& defs = paramDefs();
    State st;
    st.values.resize (defs.size());
    for (size_t i = 0; i < defs.size(); ++i) st.values[i] = defs[i].def;
    for (auto& d : st.dst) d = -1;
    for (auto& d : st.laneDst) d = -1;
    // Lo nuevo de OGRO 2 queda como sonaba antes (MODO TONAL apagado, acordes en paralelo...)
    for (size_t i = 0; i < defs.size(); ++i) if (defs[i].hasCompat()) st.values[i] = defs[i].compat;
    st.values[P::FillStyle] = 1.0f;   // ROLL: la 1.0 no tenía fills; si se prenden, que se escuchen
    // Valores de la 1.0 que cambiaron de valor por defecto
    st.values[P::Tempo] = 124.0f;
    st.values[P::RandomMode] = 1.0f; // VIVO: el azar de la 1.0
    st.values[P::MacroDepth] = 0.0f; // macros x1: mismo recorrido que la 1.0
    st.values[P::PolyMode] = 1.0f;   // polirritmos continuos, como la 1.0
    auto setP = [&st] (int idx, float v) { if (idx >= 0) st.values[(size_t) idx] = v; };

    static const std::vector<std::pair<std::string, int>> direct = {
        { "swing", P::Swing }, { "pattern", P::Pattern }, { "root", P::Root }, { "tempo", P::Tempo },
        { "ring_on", P::RingOn }, { "ring_freq", P::RingFreq }, { "ring_wave", P::RingWave }, { "ring_mix", P::RingMix },
        { "ring_spread", P::RingSpread }, { "dly_div", P::DlyDiv }, { "dly_fb", P::DlyFb }, { "dly_tone", P::DlyTone },
        { "dly_ret", P::DlyRet }, { "rev_size", P::RevSize }, { "rev_decay", P::RevDecay }, { "rev_ret", P::RevRet },
        { "pump_rel", P::PumpRel }, { "bus_comp", P::BusComp }, { "bus_crudo", P::BusCrudo }, { "m_filter", P::MFilter },
        { "m_reso", P::MReso }, { "m_lowcut", P::MLowCut }, { "m_body", P::MBody }, { "m_pres", P::MPres }, { "m_air", P::MAir },
        { "m_comp", P::MComp }, { "m_color", P::MColor }, { "m_warmth", P::MWarmth }, { "m_parallel", P::MParallel },
        { "m_texture", P::MTexture }, { "m_ceiling", P::MCeiling }, { "m_vol", P::MVol },
    };
    for (const auto& [id, v] : old)
    {
        bool done = false;
        for (const auto& [oid, idx] : direct)
            if (id == oid) { setP (idx, v); done = true; break; }
        if (done) continue;
        if (id.rfind ("macro", 0) == 0 && id.size() == 6)
        {
            const int m = id[5] - '1';
            if (m >= 0 && m < 4) setP (P::Macro1 + m, v);
            continue;
        }
        if (id.size() > 3 && id[0] == 't' && id[2] == '_')
        {
            const int t = id[1] - '1';
            if (t < 0 || t >= 8) continue;
            const std::string f = id.substr (3);
            if (f.size() == 2 && f[0] == 'p')
            {
                const int p = f[1] - '1';
                if (p < 0 || p >= kSoundParams) continue;
                if (t == Chord && p == 0) setP (P::sound (t, 0), (float) maps::chordType (v)); // de 0..1 a tipo
                else setP (P::sound (t, p), v);
            }
            else if (f == "lvl") setP (P::track (t, P::Level), v);
            else if (f == "pan") setP (P::track (t, P::Pan), v);
            else if (f == "dly") setP (P::track (t, P::Delay), v);
            else if (f == "rev") setP (P::track (t, P::Reverb), v);
            else if (f == "pump") setP (P::track (t, P::Pump), v);
            else if (f == "ring") setP (P::track (t, P::Ring), v > 0.5f ? 1.0f : 0.0f); // ruta ON = cantidad 100 %
            else if (f == "mute") setP (P::track (t, P::Mute), v);
            else if (f == "solo") setP (P::track (t, P::Solo), v);
            continue;
        }
        if (id.rfind ("lfo", 0) == 0 && id.size() > 5)
        {
            const int l = id[3] - '1';
            if (l < 0 || l >= P::kLfos) continue;
            if (id.substr (5) == "shape") setP (P::lfo (l, P::LfoShape), v);
            else if (id.substr (5) == "rate") setP (P::lfo (l, P::LfoRate), v);
            continue;
        }
        if (id.rfind ("mod", 0) == 0)
        {
            const auto us = id.find ('_');
            const int s = std::atoi (id.substr (3, us - 3).c_str()) - 1;
            if (s < 0 || s >= P::kSlots || us == std::string::npos) continue;
            const std::string f = id.substr (us + 1);
            if (f == "src") st.src[s] = clampv ((int) std::lround (v), 0, SrcCount - 1);
            else if (f == "dst") st.dst[s] = destParam ((int) std::lround (v));
            else if (f == "amt") setP (P::mod (s, P::ModAmt), v);
            continue;
        }
        if (id.rfind ("lane", 0) == 0)
        {
            const auto us = id.find ('_');
            const int l = std::atoi (id.substr (4, us - 4).c_str()) - 1;
            if (l < 0 || l >= P::kMotion || us == std::string::npos) continue;
            const std::string f = id.substr (us + 1);
            static const int bars[] = { 1, 2, 4, 8 };
            if (f == "dst") st.laneDst[l] = destParam ((int) std::lround (v));
            else if (f == "bars") setP (P::lane (l, P::LaneBars), (float) bars[clampv ((int) std::lround (v), 0, 3)]);
            else if (f == "depth") setP (P::lane (l, P::LaneDepth), v);
        }
    }
    return st;
}

} // namespace ogro2::v1
