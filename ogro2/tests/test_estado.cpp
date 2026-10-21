// OGRO 2 — tests del estado guardado: patrones (formato con versión, 32 pasos, p-locks),
// lectura del formato de OGRO 1.0, IDs de parámetros únicos y estables, y el importador
// de presets de la 1.0 (destinos de modulación, tipo de acorde, ring, macros).
#include "../../ogro/Source/engine/Engine.h"
#include "../../ogro/Source/engine/Presets.h"
#include "../Source/engine/Engine.h"
#include "../Source/engine/Presets.h"
#include <cstdio>
#include <set>

namespace
{
bool report (const char* name, bool ok, const std::string& detail)
{
    std::printf ("[%s] %s  %s\n", name, ok ? "OK   " : "FALLA", detail.c_str());
    return ok;
}
} // namespace

int main()
{
    bool all = true;
    using namespace ogro2;

    // ---- 1) Pasos de la 1.0 (32 bits) se leen igual en el formato de 64 bits ----
    {
        bool same = true;
        for (int i = 0; i < 2000; ++i)
        {
            ogro::Step a;
            a.on = (i % 3) != 0; a.vel = 1 + i % 127; a.note = (i % 49) - 24; a.prob = i % 101; a.ratchet = 1 + i % 4; a.slide = (i % 5) == 0;
            const Step b = Step::unpack ((uint64_t) a.pack());
            same &= b.on == a.on && b.vel == a.vel && b.note == a.note && b.prob == a.prob && b.ratchet == a.ratchet && b.slide == a.slide
                    && b.gate == 0 && b.nudge == 0 && b.cond == 0 && b.octave == 0 && b.chordType == 0;
        }
        all &= report ("pasos 1.0", same, "2000 pasos de la 1.0 leidos sin cambios");
    }

    // ---- 2) Campos nuevos del paso (nudge y octava con signo) ----
    {
        bool same = true;
        for (int n = -50; n <= 50; n += 7)
            for (int o = -3; o <= 3; ++o)
            {
                Step s; s.on = true; s.nudge = n; s.octave = o; s.gate = 17; s.cond = 9; s.chordType = 21; s.ratchet = 8;
                const auto b = Step::unpack (s.pack());
                same &= b.nudge == n && b.octave == o && b.gate == 17 && b.cond == 9 && b.chordType == 21 && b.ratchet == 8;
            }
        all &= report ("pasos 2.0", same, "nudge, octava, gate, condicion, tipo de acorde y ratchet 8");
    }

    // ---- 3) Banco: guardar y leer (32 pasos, largos, p-locks por ID, carriles) ----
    {
        PatternBank a, b;
        factoryPresets()[5].build (a);
        Step s; s.on = true; s.vel = 77; s.note = -5; s.nudge = -12; s.gate = 20;
        a.set (3, Bass, 27, s);
        a.setLength (3, Bass, 32);
        a.setPatternLength (3, 32);
        a.setLock (3, Bass, 27, P::sound (Bass, 2), 0.25f);
        a.setLock (3, Bass, 27, P::track (Bass, P::EnvS), 0.8f);
        const auto& defs = paramDefs();
        const auto txt = a.serialise ([&defs] (int p) { return defs[(size_t) p].id; });
        b.deserialise (txt, [] (const std::string& id) { return paramIndex (id); });
        bool same = true;
        for (int p = 0; p < kPatterns; ++p)
        {
            same &= a.getPatternLength (p) == b.getPatternLength (p);
            for (int t = 0; t < kTracks; ++t)
            {
                same &= a.getLength (p, t) == b.getLength (p, t);
                for (int st = 0; st < kSteps; ++st)
                {
                    same &= a.get (p, t, st).pack() == b.get (p, t, st).pack();
                    for (int k = 0; k < kMaxLocks; ++k)
                    {
                        const auto la = a.getLock (p, t, st, k), lb = b.getLock (p, t, st, k);
                        same &= la.param == lb.param && std::fabs (la.value - lb.value) < 1e-4f;
                    }
                }
            }
        }
        for (int l = 0; l < kLanes; ++l) for (int i = 0; i < kLanePoints; ++i) same &= std::fabs (a.lane (l, i) - b.lane (l, i)) < 0.002f;
        same &= std::fabs (b.lockValue (3, Bass, 27, P::sound (Bass, 2)) - 0.25f) < 1e-4f;
        all &= report ("banco", same, "guardar y leer con p-locks (" + std::to_string (txt.size()) + " bytes)");
        // Lo que no se entiende se ignora (versiones futuras)
        PatternBank c;
        const bool tolerant = c.deserialise (txt + "X algo nuevo 1 2 3\nT 0 pista_nueva 16 0=1\nK 0 bass 0 param.futuro 0.5\n",
                                             [] (const std::string& id) { return paramIndex (id); });
        all &= report ("banco futuro", tolerant && c.get (3, Bass, 27).pack() == s.pack(), "ignora lineas, pistas y parametros desconocidos");
    }

    // ---- 4) Banco de la 1.0 (texto de su serialise) leído por OGRO 2 ----
    {
        bool same = true;
        for (const auto& p : ogro::factoryPresets())
        {
            ogro::PatternBank a;
            p.build (a);
            PatternBank b;
            same &= b.deserialise (a.serialise());
            for (int pat = 0; pat < 8; ++pat)
                for (int t = 0; t < 8; ++t)
                {
                    same &= a.getLength (pat, t) == b.getLength (pat, t);
                    for (int st = 0; st < 16; ++st) same &= (uint64_t) a.get (pat, t, st).pack() == b.get (pat, t, st).pack();
                    for (int st = 16; st < kSteps; ++st) same &= ! b.get (pat, t, st).on;
                }
            for (int l = 0; l < 4; ++l) for (int i = 0; i < 64; ++i) same &= std::fabs (a.lane (l, i) - b.lane (l, i)) < 0.002f;
        }
        all &= report ("banco 1.0", same, "los patrones de los 12 presets de la 1.0 se leen igual");
    }

    // ---- 5) IDs: únicos, con formato estable y versión ----
    {
        std::set<std::string> ids;
        bool ok = true;
        int count = 0;
        for (const auto& d : paramDefs())
        {
            if (! d.exists) continue;
            ++count;
            ok &= ids.insert (d.id).second;
            ok &= d.id.find ('.') != std::string::npos && d.version >= 1;
            for (char c : d.id) ok &= (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_';
            if (d.kind == Kind::Choice) ok &= (int) d.choices.size() >= d.validChoices && d.validChoices > 0;
        }
        ok &= paramIndex ("bass.cutoff") == P::sound (Bass, 2) && paramIndex ("chord.type") == P::sound (Chord, 0) && paramIndex ("no.existe") < 0;
        all &= report ("ids", ok, std::to_string (count) + " parametros, IDs unicos en minuscula con punto");
    }

    // ---- 6) Importador: cada destino de modulación de la 1.0 va al mismo parámetro ----
    {
        const auto& d1 = ogro::modDests();
        bool ok = true;
        for (int i = 1; i < (int) d1.size(); ++i)
        {
            const int p1 = d1[(size_t) i].param;
            const int p2 = v1::destParam (i);
            ok &= p2 >= 0 && ogro::paramDefs()[(size_t) p1].name == paramDefs()[(size_t) p2].name;
        }
        all &= report ("destinos 1.0", ok, std::to_string (d1.size() - 1) + " destinos de la 1.0 traducidos por nombre");
    }

    // ---- 7) Importador: tipo de acorde, ring, macros, compases de MOTION y azar VIVO ----
    {
        bool ok = true;
        for (int k = 0; k < 8; ++k)
        {
            const auto st = v1::convert ({ { "t8_p1", ogro::stepCentre (k, 8) } });
            ok &= (int) st.values[(size_t) P::sound (Chord, 0)] == k;
        }
        auto st = v1::convert ({ { "t3_ring", 1.0f }, { "macro1", 0.62f }, { "lane2_bars", 3.0f }, { "mod4_src", (float) SrcLfo2 },
                                 { "mod4_dst", (float) v1::dest (Perc, 0) } });
        ok &= st.values[(size_t) P::track (Rim, P::Ring)] == 1.0f;
        ok &= st.values[P::Macro1] == 0.62f && st.values[P::MacroDepth] == 0.0f;
        ok &= st.values[(size_t) P::lane (1, P::LaneBars)] == 8.0f;
        ok &= st.src[3] == SrcLfo2 && st.dst[3] == P::sound (Perc, 0);
        ok &= st.values[P::RandomMode] == 1.0f && st.values[P::Tempo] == 124.0f;
        all &= report ("importador", ok, "acordes, ring, macros x1, MOTION y azar VIVO");
    }

    // ---- 8) Presets de OGRO 2: todos los IDs existen y los 12 de la 1.0 siguen primero ----
    {
        bool ok = true;
        int v2 = 0;
        std::string bad;
        for (size_t i = 0; i < factoryPresets().size(); ++i)
        {
            const auto& p = factoryPresets()[i];
            ok &= p.v2 == (i >= 12);
            if (! p.v2) continue;
            ++v2;
            for (const auto& kv : p.values2) if (paramIndex (kv.first) < 0) { ok = false; bad += kv.first + " "; }
            for (const auto& m : p.mods) if (modSourceIndex (std::get<0> (m)) < 0 || paramIndex (std::get<1> (m)) < 0) ok = false;
            for (const auto& l : p.lanes) if (! l.empty() && paramIndex (l) < 0) { ok = false; bad += l + " "; }
            const auto st = factoryState (p);
            ok &= st.values[P::Tempo] == 127.0f && st.values[P::Tonal] == 1.0f;
        }
        all &= report ("presets 2.0", ok && v2 >= 8, std::to_string (v2) + " presets de OGRO 2 a 127 BPM con IDs validos " + bad);
    }

    std::printf (all ? "ESTADO OK\n" : "ESTADO CON FALLAS\n");
    return all ? 0 : 1;
}
