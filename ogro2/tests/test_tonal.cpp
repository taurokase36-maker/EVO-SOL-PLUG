// OGRO 2 — tests de la fase 3: escalas y MODO TONAL, acordes (paralelo / diatónico,
// inversión, apertura, voice leading, strum, tipo por paso), bajo (sub, formas, 303,
// legato), voz de MELODÍA (afinación de cada modelo, poly / mono) y generador en tono.
#include "../Source/engine/Engine.h"
#include "../Source/engine/Generator.h"
#include "../Source/engine/Presets.h"
#include <chrono>
#include <cstdio>
#include <functional>

using namespace ogro2;

namespace
{
constexpr double kFs = 44100.0;
constexpr int64_t kBar = 88200;

std::vector<float> defaults()
{
    const auto& d = paramDefs();
    std::vector<float> v (d.size());
    for (size_t i = 0; i < d.size(); ++i) v[i] = d[i].def;
    v[P::Swing] = 0.5f;
    v[P::MComp] = 0; v[P::MWarmth] = 0; v[P::BusComp] = 0; v[P::BusCrudo] = 0; v[P::MTexture] = 0;
    v[P::DlyRet] = 0; v[P::RevRet] = 0;
    for (int t = 0; t < kTracks; ++t) { v[(size_t) P::track (t, P::Delay)] = 0; v[(size_t) P::track (t, P::Reverb)] = 0; v[(size_t) P::track (t, P::Pump)] = 0; }
    for (int m = 0; m < 4; ++m) v[(size_t) (P::Macro1 + m)] = 0.5f;
    v[P::MCeiling] = 0.0f;
    return v;
}

struct Out { std::vector<float> l, r; };
Out run (Engine& eng, const std::vector<float>& v, int64_t total)
{
    eng.prepare (kFs, 512);
    Out o;
    o.l.assign ((size_t) total, 0.0f); o.r.assign ((size_t) total, 0.0f);
    for (int64_t pos = 0; pos < total; pos += 512)
    {
        const int n = (int) std::min<int64_t> (512, total - pos);
        HostInfo h; h.valid = true; h.playing = true; h.bpm = 120.0; h.ppq = (double) pos * 2.0 / kFs;
        eng.setParams (v.data(), (int) v.size());
        eng.process (h, nullptr, 0, o.l.data() + pos, o.r.data() + pos, n);
    }
    return o;
}
double maxDiff (const Out& a, const Out& b)
{
    double d = 0;
    for (size_t i = 0; i < a.l.size(); ++i) d = std::max ({ d, (double) std::fabs (a.l[i] - b.l[i]), (double) std::fabs (a.r[i] - b.r[i]) });
    return d;
}
double rms (const std::vector<float>& x, size_t a, size_t b)
{
    double e = 0;
    for (size_t i = a; i < std::min (b, x.size()); ++i) e += (double) x[i] * x[i];
    return std::sqrt (e / (double) std::max<size_t> (1, std::min (b, x.size()) - a));
}
// Frecuencia fundamental por autocorrelación (entre 60 y 2000 Hz)
double pitchOf (const std::vector<float>& x, size_t from, size_t len)
{
    const int minLag = (int) (kFs / 2000.0), maxLag = (int) (kFs / 60.0);
    double best = -1e9; int bestLag = minLag;
    std::vector<double> c ((size_t) maxLag + 2, 0.0);
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double s = 0, e1 = 0, e2 = 0;
        for (size_t i = from; i < from + len; ++i) { s += x[i] * x[i + (size_t) lag]; e1 += x[i] * x[i]; e2 += x[i + (size_t) lag] * x[i + (size_t) lag]; }
        c[(size_t) lag] = s / std::sqrt (e1 * e2 + 1e-20);
    }
    // El primer pico fuerte (evita elegir la octava de abajo)
    double maxC = 0;
    for (int lag = minLag; lag <= maxLag; ++lag) maxC = std::max (maxC, c[(size_t) lag]);
    for (int lag = minLag + 1; lag < maxLag; ++lag)
        if (c[(size_t) lag] > 0.9 * maxC && c[(size_t) lag] >= c[(size_t) lag - 1] && c[(size_t) lag] >= c[(size_t) lag + 1]) { bestLag = lag; best = c[(size_t) lag]; break; }
    (void) best;
    // Interpolación parabólica
    const double a = c[(size_t) bestLag - 1], b = c[(size_t) bestLag], d = c[(size_t) bestLag + 1];
    const double off = 0.5 * (a - d) / (a - 2 * b + d + 1e-20);
    return kFs / ((double) bestLag + off);
}
double cents (double f, double ref) { return 1200.0 * std::log2 (f / ref); }

bool report (const char* name, bool ok, const std::string& detail)
{
    std::printf ("[%s] %s  %s\n", name, ok ? "OK   " : "FALLA", detail.c_str());
    return ok;
}
Step hit (int note = 0, int vel = 100) { Step s; s.on = true; s.vel = vel; s.note = note; return s; }
std::string notesText (const float* n, int c)
{
    std::string o;
    for (int i = 0; i < c; ++i) o += (i ? " " : "") + std::to_string ((int) std::lround (n[i]));
    return o;
}
} // namespace

int main()
{
    bool all = true;
    char buf[256];

    // ---- 1) Escalas: grados -> semitonos y vuelta ----
    {
        bool ok = degreeToSemis (0, 0) == 0 && degreeToSemis (2, 0) == 3 && degreeToSemis (7, 0) == 12 && degreeToSemis (-1, 0) == -2
                  && degreeToSemis (3, 3) == 5 && degreeToSemis (6, 3) == 11 && degreeToSemis (5, 6) == 12 && degreeToSemis (-2, 6) == -5
                  && degreeToSemis (1, 2) == 1 && degreeToSemis (2, 4) == 4;
        for (int sc = 0; sc < (int) scaleNames().size(); ++sc)
            for (int d = -14; d <= 14; ++d) ok &= semisToDegree (degreeToSemis (d, sc), sc) == d;
        all &= report ("escalas", ok, "7 escalas: grados a semitonos y vuelta (de -2 a +2 octavas)");
    }

    // ---- 2) MODO TONAL: el grado 2 en La menor suena igual que +3 semitonos sin modo tonal ----
    {
        for (int t : { (int) Bass, (int) Perc, (int) Melody })
        {
            Engine a, b;
            a.bank.clearAll(); b.bank.clearAll();
            a.bank.set (0, t, 0, hit (2)); a.bank.set (0, t, 8, hit (-3));
            b.bank.set (0, t, 0, hit (3)); b.bank.set (0, t, 8, hit (-5));
            auto va = defaults(); va[P::Tonal] = 1; va[P::Scale] = 0; va[P::Root] = 9;
            auto vb = defaults(); vb[P::Tonal] = 0; vb[P::Root] = 9;
            const double d = maxDiff (run (a, va, kBar), run (b, vb, kBar));
            std::snprintf (buf, sizeof (buf), "%s: grados 2 y -3 = +3 y -5 semitonos (diferencia %.2g)", trackName (t), d);
            all &= report ("modo tonal", d == 0.0, buf);
        }
    }

    // ---- 3) Acordes: paralelo igual a la 1.0, diatónico en tono, inversión y apertura ----
    {
        Tonality k; k.tonal = true; k.root = 9; k.scale = 0;   // La menor
        float n[kMaxChordNotes];
        ChordShape sh;
        sh.type = 0; sh.diatonic = false;
        int c = buildChord (sh, k, 57.0f, 0.0f, 0, nullptr, 0, n);
        bool ok = c == 4 && notesText (n, c) == "57 60 64 67";
        sh.diatonic = true;
        c = buildChord (sh, k, 48.0f + 9.0f + (float) degreeToSemis (2, 0), 2.0f, 0, nullptr, 0, n);   // III de La menor = Do maj7
        const std::string third = notesText (n, c);
        ok &= third == "60 64 67 71";
        c = buildChord (sh, k, 48.0f + 9.0f + (float) degreeToSemis (4, 0), 4.0f, 0, nullptr, 0, n);   // v = Mi m7
        const std::string fifth = notesText (n, c);
        ok &= fifth == "64 67 71 74";
        Tonality h = k; h.scale = 3;                                                                     // menor armónica: V = Mi 7
        c = buildChord (sh, h, 48.0f + 9.0f + (float) degreeToSemis (4, 3), 4.0f, 0, nullptr, 0, n);
        ok &= notesText (n, c) == "64 68 71 74";
        sh.diatonic = false; sh.inversion = 1;
        c = buildChord (sh, k, 57.0f, 0.0f, 0, nullptr, 0, n);
        ok &= notesText (n, c) == "60 64 67 69";
        sh.inversion = 0; sh.spread = 1.0f;
        c = buildChord (sh, k, 57.0f, 0.0f, 0, nullptr, 0, n);
        ok &= notesText (n, c) == "57 64 72 79";
        all &= report ("acordes", ok, "Am7 = 57 60 64 67; diatonico III = " + third + ", v = " + fifth + "; inversion y apertura");
    }

    // ---- 4) Voice leading: i - VI - iv - v en La menor se mueve poco y queda en registro ----
    {
        Tonality k; k.tonal = true; k.root = 9; k.scale = 0;
        ChordShape sh; sh.type = 0; sh.diatonic = true; sh.voiceLeading = true;
        float prev[kMaxChordNotes]; int pc = 0;
        float worst = 0.0f, lo = 200.0f, hi = 0.0f;
        std::string prog;
        for (int deg : { 0, 5, 3, 4, 0, 5, 3, 4 })
        {
            float n[kMaxChordNotes];
            const int c = buildChord (sh, k, 57.0f + (float) degreeToSemis (deg, 0), (float) deg, 0, prev, pc, n);
            if (pc > 0)
            {
                float move = 0.0f;
                for (int i = 0; i < c; ++i) { float dm = 99.0f; for (int j = 0; j < pc; ++j) dm = std::min (dm, std::fabs (n[i] - prev[j])); move = std::max (move, dm); }
                worst = std::max (worst, move);
            }
            for (int i = 0; i < c; ++i) { lo = std::min (lo, n[i]); hi = std::max (hi, n[i]); }
            if (prog.size() < 60) prog += "[" + notesText (n, c) + "] ";
            std::copy (n, n + c, prev); pc = c;
        }
        std::snprintf (buf, sizeof (buf), "cada voz se mueve como mucho %.0f semitonos, registro %.0f-%.0f: %s", worst, lo, hi, prog.c_str());
        all &= report ("voice leading", worst <= 4.0f && lo >= 40.0f && hi <= 88.0f, buf);
    }

    // ---- 5) Tipo de acorde por paso y strum ----
    {
        Engine a, b;
        a.bank.clearAll(); b.bank.clearAll();
        Step s = hit (0); s.chordType = 9 + 1;   // M por paso
        a.bank.set (0, Chord, 0, s);
        b.bank.set (0, Chord, 0, hit (0));
        auto va = defaults(); va[P::Tonal] = 0; va[P::ChordMode] = 1; va[P::ChordLead] = 0;
        auto vb = va; vb[(size_t) P::sound (Chord, 0)] = 9;
        const double d = maxDiff (run (a, va, kBar / 2), run (b, vb, kBar / 2));
        all &= report ("tipo por paso", d == 0.0, "un paso con tipo M suena igual que la pista en M");

        Engine c, e;
        c.bank.clearAll(); e.bank.clearAll();
        Step held = hit (0); held.gate = 32;
        c.bank.set (0, Chord, 0, held); e.bank.set (0, Chord, 0, held);
        va[(size_t) P::sound (Chord, 7)] = 0.0f;
        va[(size_t) P::track (Chord, P::EnvS)] = 1.0f;   // el acorde se sostiene
        auto vs = va; vs[P::ChordStrum] = 60.0f;
        const auto rs = run (c, vs, kBar / 2), r0 = run (e, va, kBar / 2);
        const size_t ms = (size_t) (kFs * 0.001);
        const double early = rms (rs.l, 0, 40 * ms) / (rms (r0.l, 0, 40 * ms) + 1e-12);
        const double late = rms (rs.l, 250 * ms, 350 * ms) / (rms (r0.l, 250 * ms, 350 * ms) + 1e-12);
        std::snprintf (buf, sizeof (buf), "strum 60 ms: los primeros 40 ms suenan al %.0f %% (entra de a una nota), despues %.0f %%", early * 100.0, late * 100.0);
        all &= report ("strum", early < 0.75 && late > 0.75 && late < 1.3, buf);
    }

    // ---- 6) Bajo: sub, formas nuevas y acento 303 ----
    {
        auto bassRun = [] (std::function<void (std::vector<float>&)> f, bool accent) {
            Engine e;
            e.bank.clearAll();
            for (int s = 0; s < 16; s += 4) e.bank.set (0, Bass, s, hit (0, accent && s == 4 ? 127 : 100));
            auto v = defaults(); v[P::Tonal] = 0;
            f (v);
            return run (e, v, kBar);
        };
        const auto base = bassRun ([] (std::vector<float>&) {}, false);
        const auto withSub = bassRun ([] (std::vector<float>& v) { v[P::BassSub] = 1.0f; }, false);
        // Energía bajo 70 Hz (filtro de un polo dos veces)
        auto lowEnergy = [] (const Out& o) {
            double a = 0, b = 0, e = 0;
            const double c = 1.0 - std::exp (-2.0 * 3.14159265 * 70.0 / kFs);
            for (float x : o.l) { a += c * (x - a); b += c * (a - b); e += b * b; }
            return e;
        };
        const double gain = 10.0 * std::log10 (lowEnergy (withSub) / (lowEnergy (base) + 1e-20));
        bool ok = gain > 3.0;
        std::string forms;
        for (int f = 1; f <= 4; ++f)
        {
            const auto o = bassRun ([f] (std::vector<float>& v) { v[P::BassOsc] = (float) f; v[(size_t) P::sound (Bass, 6)] = 0.6f; }, false);
            bool fin = true;
            for (float x : o.l) fin &= std::isfinite (x);
            const double r = rms (o.l, 0, o.l.size());
            ok &= fin && r > 0.01 && maxDiff (o, base) > 0.01;
            forms += std::to_string ((int) std::lround (20.0 * std::log10 (r))) + " ";
        }
        const auto acc303 = bassRun ([] (std::vector<float>& v) { v[P::BassAccent] = 1; }, true);
        const auto accOld = bassRun ([] (std::vector<float>& v) { v[P::BassAccent] = 0; }, true);
        const size_t s4 = (size_t) (kBar / 4);
        const double accGain = 20.0 * std::log10 (rms (acc303.l, s4, s4 + 4000) / (rms (accOld.l, s4, s4 + 4000) + 1e-12));
        ok &= accGain > 0.5;
        std::snprintf (buf, sizeof (buf), "sub +%.1f dB abajo de 70 Hz; 4 formas nuevas (dB: %s); acento 303 %+.1f dB sobre el clasico", gain, forms.c_str(), accGain);
        all &= report ("bajo", ok, buf);
    }
    {
        // Legato: dos notas pisadas (gate largo) no vuelven a disparar la envolvente
        Engine a, b;
        for (auto* e : { &a, &b })
        {
            e->bank.clearAll();
            Step s1 = hit (0); s1.gate = 32;
            e->bank.set (0, Bass, 0, s1);
            e->bank.set (0, Bass, 2, hit (7));
        }
        auto v = defaults(); v[P::Tonal] = 0; v[(size_t) P::track (Bass, P::EnvS)] = 0.8f;
        v[(size_t) P::track (Bass, P::EnvA)] = 30.0f;   // ataque lento: el re-disparo se nota
        auto vl = v; vl[P::BassLegato] = 1;
        const auto r0 = run (a, v, kBar / 2), r1 = run (b, vl, kBar / 2);
        // Después del segundo golpe: sin legato la envolvente arranca de cero (hay un hueco)
        const size_t at = (size_t) (2 * kBar / 16);
        const double dip0 = rms (r0.l, at + 150, at + 700), dip1 = rms (r1.l, at + 150, at + 700);
        std::snprintf (buf, sizeof (buf), "nivel en el cambio de nota: con legato %.3f, sin legato %.3f", dip1, dip0);
        all &= report ("legato", dip1 > dip0 * 1.5, buf);
    }

    // ---- 7) MELODÍA: cada modelo afinado (La 440 con la tónica en La) ----
    {
        std::string detail;
        bool ok = true;
        for (int model : { 0, 1, 3 })
        {
            Engine e;
            e.bank.clearAll();
            Step s = hit (0); s.gate = 32;
            e.bank.set (0, Melody, 0, s);
            auto v = defaults(); v[P::Root] = 9;
            v[(size_t) P::sound (Melody, 0)] = (float) model;
            v[(size_t) P::sound (Melody, 6)] = 0.0f;   // sin vibrato
            v[(size_t) P::sound (Melody, 2)] = 0.9f;
            v[(size_t) P::track (Melody, P::EnvS)] = 0.8f;
            const auto o = run (e, v, kBar / 2);
            const double f = pitchOf (o.l, 4000, 4096);
            double c = cents (f, 440.0);
            if (model == 1 && std::fabs (c + 1200.0) < 15.0) c += 1200.0;   // el LEAD tiene sub: el período es el de la octava de abajo
            ok &= std::fabs (c) < 15.0;
            detail += melodyModelNames()[(size_t) model] + " " + (c >= 0 ? "+" : "") + std::to_string ((int) std::lround (c)) + " ct  ";
        }
        {
            Engine e;
            e.bank.clearAll();
            e.bank.set (0, Melody, 0, hit (0));
            auto v = defaults(); v[(size_t) P::sound (Melody, 0)] = 2.0f;
            const auto o = run (e, v, kBar / 2);
            bool fin = true;
            for (float x : o.l) fin &= std::isfinite (x);
            ok &= fin && rms (o.l, 0, 20000) > 0.01;
            detail += "Campana suena";
        }
        all &= report ("melodia", ok, detail);
    }
    {
        // Poly: 4 notas que se pisan suenan juntas; mono: solo la última
        auto chordOf = [] (int mode) {
            Engine e;
            e.bank.clearAll();
            for (int i = 0; i < 4; ++i) { Step s = hit (i * 2); s.gate = 32; e.bank.set (0, Melody, i, s); }
            auto v = defaults(); v[P::MelVoices] = (float) mode;
            v[(size_t) P::sound (Melody, 0)] = 1.0f;
            v[(size_t) P::track (Melody, P::EnvS)] = 1.0f;
            v[(size_t) P::sound (Melody, 6)] = 0.0f;
            return run (e, v, kBar / 2);
        };
        const auto poly = chordOf (0), mono = chordOf (1);
        const size_t at = (size_t) (5 * kBar / 16);
        const double rp = rms (poly.l, at, at + 4000), rmn = rms (mono.l, at, at + 4000);
        std::snprintf (buf, sizeof (buf), "4 notas pisadas: poly %.3f, mono %.3f", rp, rmn);
        all &= report ("voces", rp > rmn * 1.3, buf);
    }

    // ---- 8) Importar de la 1.0 deja todo como sonaba; los defaults nuevos son los de OGRO 2 ----
    {
        const auto st = v1::convert ({});
        bool ok = st.values[P::Tonal] == 0.0f && st.values[P::ChordMode] == 1.0f && st.values[P::BassAccent] == 0.0f && st.values[P::ChordLead] == 0.0f;
        ok &= paramDefs()[P::Tonal].def == 1.0f && paramDefs()[P::ChordMode].def == 0.0f && paramDefs()[P::BassAccent].def == 1.0f;
        all &= report ("compatibilidad", ok, "importado de la 1.0: modo tonal apagado, acordes en paralelo, acento clasico");
    }

    // ---- 9) Generador en tono: las notas son grados razonables ----
    {
        PatternBank b;
        bool ok = true;
        int notes = 0;
        for (uint32_t seed = 1; seed < 40; ++seed)
            for (int g = 0; g < (int) genreNames().size(); ++g)
            {
                inventAll (b, 0, seed, g, true);
                for (int t : { (int) Bass, (int) Chord, (int) Perc, (int) Melody })
                    for (int s = 0; s < 16; ++s)
                    {
                        const auto st = b.get (0, t, s);
                        if (! st.on) continue;
                        ++notes;
                        ok &= st.note >= -7 && st.note <= 9;
                    }
                bool mel = false;
                for (int s = 0; s < 16; ++s) mel |= b.get (0, Melody, s).on;
                ok &= mel;
            }
        all &= report ("generador", ok, std::to_string (notes) + " notas en grados de -7 a +9; la melodia siempre tiene algo");
    }

    // ---- 10) CPU: las 10 pistas con melodía poly (órgano, el modelo más pesado) y acordes con strum ----
    for (int model : { 0, 3 })
    {
        Engine e;
        e.bank.clearAll();
        inventAll (e.bank, 0, 99, GenreMinimal, true);
        for (int s = 0; s < 16; s += 2) { Step st = hit ((s / 2) % 5); st.gate = 24; e.bank.set (0, Melody, s, st); }
        auto v = defaults();
        v[(size_t) P::sound (Melody, 0)] = (float) model;
        v[(size_t) P::track (Melody, P::EnvS)] = 0.7f;
        v[P::ChordStrum] = 30.0f;
        v[P::MComp] = 0.5f; v[P::MWarmth] = 0.4f; v[P::BusComp] = 0.6f; v[P::DlyRet] = 0.5f; v[P::RevRet] = 0.5f;
        const auto t0 = std::chrono::steady_clock::now();
        run (e, v, kBar * 8);
        const double cpu = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / 16.0 * 100.0;
        std::snprintf (buf, sizeof (buf), "10 pistas con melodia poly (%s): %.1f %% de un nucleo", melodyModelNames()[(size_t) model].c_str(), cpu);
        all &= report ("cpu", cpu < 20.0, buf);
    }

    std::printf (all ? "TONAL OK\n" : "TONAL CON FALLAS\n");
    return all ? 0 : 1;
}
