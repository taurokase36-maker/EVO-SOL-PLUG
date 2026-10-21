// OGRO 2 — tests de la fase 5: CARÁCTER (ORIGINAL / LIMPIO: menos aliasing y sin zipper),
// tira analógica por pista, escenas A/B con MORPH, deriva lenta y suma de consola.
#include "../Source/engine/Engine.h"
#include "../Source/engine/Generator.h"
#include "../Source/engine/Presets.h"
#include <chrono>
#include <complex>
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
    v[P::MComp] = 0; v[P::MWarmth] = 0; v[P::BusComp] = 0; v[P::BusCrudo] = 0; v[P::MTexture] = 0; v[P::MCeiling] = 0;
    v[P::DlyRet] = 0; v[P::RevRet] = 0; v[P::MAir] = 0; v[P::MPres] = 0;
    for (int t = 0; t < kTracks; ++t) { v[(size_t) P::track (t, P::Delay)] = 0; v[(size_t) P::track (t, P::Reverb)] = 0; v[(size_t) P::track (t, P::Pump)] = 0; }
    v[P::Tonal] = 0;
    return v;
}
struct Out { std::vector<float> l, r; };
Out run (Engine& eng, std::vector<float> v, int64_t total, const std::function<void (int64_t, std::vector<float>&)>& autom = {}, int block = 256)
{
    eng.prepare (kFs, block);
    Out o;
    o.l.assign ((size_t) total, 0.0f); o.r.assign ((size_t) total, 0.0f);
    for (int64_t pos = 0; pos < total; pos += block)
    {
        const int n = (int) std::min<int64_t> (block, total - pos);
        if (autom) autom (pos, v);
        HostInfo h; h.valid = true; h.playing = true; h.bpm = 120.0; h.ppq = (double) pos * 2.0 / kFs;
        eng.setParams (v.data(), (int) v.size());
        eng.process (h, nullptr, 0, o.l.data() + pos, o.r.data() + pos, n);
    }
    return o;
}
double rms (const std::vector<float>& x, size_t a, size_t b)
{
    double e = 0;
    b = std::min (b, x.size());
    for (size_t i = a; i < b; ++i) e += (double) x[i] * x[i];
    return std::sqrt (e / (double) std::max<size_t> (1, b - a));
}
double maxDiff (const Out& a, const Out& b)
{
    double d = 0;
    for (size_t i = 0; i < a.l.size(); ++i) d = std::max ({ d, (double) std::fabs (a.l[i] - b.l[i]), (double) std::fabs (a.r[i] - b.r[i]) });
    return d;
}
// Aliasing: energía fuera de los armónicos de f (DFT con ventana)
double aliasDb (const std::vector<float>& x, size_t from, double f)
{
    const int N = 16384;
    double harm = 0, other = 0;
    for (int k = 4; k < N / 2; k += 2)
    {
        std::complex<double> s = 0;
        const double w = 2 * 3.14159265358979 * k / N;
        for (int i = 0; i < N; ++i) s += (double) x[from + (size_t) i] * (0.5 - 0.5 * std::cos (2 * 3.14159265358979 * i / N)) * std::polar (1.0, -w * i);
        const double fk = k * kFs / N, hn = fk / f, d = std::fabs (hn - std::round (hn)) * f;
        (d < 30 ? harm : other) += std::norm (s);
    }
    return 10.0 * std::log10 (other / harm);
}
bool report (const char* name, bool ok, const std::string& detail)
{
    std::printf ("[%s] %s  %s\n", name, ok ? "OK   " : "FALLA", detail.c_str());
    return ok;
}
Step hit (int note = 0, int vel = 110, int gate = 0) { Step s; s.on = true; s.vel = vel; s.note = note; s.gate = gate; return s; }
} // namespace

int main()
{
    bool all = true;
    char buf[300];

    // ---- 1) LIMPIO + ALTA: menos aliasing en un bajo agudo y brillante ----
    {
        double a[2];
        for (int mode = 0; mode < 2; ++mode)
        {
            Engine e;
            e.bank.clearAll();
            e.bank.set (0, Bass, 0, hit (31, 110, 63));
            auto v = defaults();
            v[(size_t) P::sound (Bass, 0)] = 0.95f;   // +2 octavas
            v[(size_t) P::sound (Bass, 2)] = 1.0f;    // filtro abierto
            v[(size_t) P::sound (Bass, 3)] = 0.0f;
            v[(size_t) P::sound (Bass, 5)] = 0.6f;    // drive
            v[(size_t) P::track (Bass, P::EnvS)] = 1.0f;
            v[P::BassOsc] = 0; v[(size_t) P::sound (Bass, 6)] = 0.0f;   // sierra
            v[P::Character] = (float) mode; v[P::Quality] = 1;
            const auto o = run (e, v, 30000);
            const double f = 440.0 * std::pow (2.0, (36 + 9 + 24 + 31 - 69) / 12.0);
            a[mode] = aliasDb (o.l, 8000, f);
        }
        std::snprintf (buf, sizeof (buf), "bajo a %.0f Hz con drive: ORIGINAL %.1f dB, LIMPIO + ALTA %.1f dB de aliasing", 440.0 * std::pow (2.0, (36 + 9 + 24 + 31 - 69) / 12.0), a[0], a[1]);
        all &= report ("limpio", a[1] < a[0] - 3.0, buf);
    }
    {
        // Zipper: un bajo seno y saltos bruscos de DRIVE cada 2048 muestras. En ORIGINAL cada salto
        // es un escalón (pico en la derivada); en LIMPIO se desliza en 8 ms.
        double spike[2];
        for (int mode = 0; mode < 2; ++mode)
        {
            Engine e;
            e.bank.clearAll();
            e.bank.set (0, Bass, 0, hit (0, 110, 63));
            auto v = defaults();
            v[(size_t) P::track (Bass, P::EnvS)] = 1.0f;
            v[P::BassOsc] = 4; v[(size_t) P::sound (Bass, 6)] = 0.0f;   // seno puro
            v[(size_t) P::sound (Bass, 2)] = 1.0f; v[(size_t) P::sound (Bass, 3)] = 0.0f;
            v[P::Character] = (float) mode;
            const auto o = run (e, v, 40000, [] (int64_t pos, std::vector<float>& vv) { vv[(size_t) P::sound (Bass, 5)] = (pos / 2048) % 2 ? 0.9f : 0.1f; });
            double mx = 0.0;
            for (size_t i = 6000; i < o.l.size(); ++i) mx = std::max (mx, (double) std::fabs (o.l[i] - o.l[i - 1]));
            spike[mode] = mx;
        }
        std::snprintf (buf, sizeof (buf), "saltos de DRIVE sobre un seno: escalon max ORIGINAL %.4f, LIMPIO %.4f", spike[0], spike[1]);
        all &= report ("suavizado", spike[1] < spike[0] * 0.6, buf);
    }
    {
        // LIMPIO no cambia el nivel ni el color de los presets (menos de 1 dB)
        double worst = 0.0;
        for (int p = 0; p < (int) factoryPresets().size(); ++p)
        {
            double r[2];
            for (int mode = 0; mode < 2; ++mode)
            {
                Engine e;
                const auto st = factoryState (factoryPresets()[(size_t) p]);
                auto v = st.values;
                v[P::Character] = (float) mode;
                factoryPresets()[(size_t) p].build (e.bank);
                applyRouting (e.routing, st);
                r[mode] = rms (run (e, v, kBar * 2).l, 0, (size_t) (kBar * 2));
            }
            worst = std::max (worst, std::fabs (20.0 * std::log10 (r[1] / r[0])));
        }
        std::snprintf (buf, sizeof (buf), "12 presets en LIMPIO: nivel distinto en %.2f dB como mucho", worst);
        all &= report ("limpio nivel", worst < 1.0, buf);
    }

    // ---- 2) Tira analógica ----
    {
        auto drums = [] (Engine& e) {
            e.bank.clearAll();
            for (int s = 0; s < 16; s += 4) e.bank.set (0, Kick, s, hit());
            for (int s = 2; s < 16; s += 4) e.bank.set (0, Clap, s, hit());
            for (int s = 0; s < 16; ++s) e.bank.set (0, HatC, s, hit (0, 80));
            e.bank.set (0, Bass, 2, hit (0, 110, 12)); e.bank.set (0, Chord, 0, hit (0, 100, 24));
        };
        Engine e0;
        drums (e0);
        const auto ref = run (e0, defaults(), kBar);
        struct Case { const char* name; int field; float value; int track; };
        const Case cases[] = { { "drive cinta", P::StripDrive, 0.7f, Clap }, { "cuerpo", P::StripBody, 0.8f, Kick }, { "ruido", P::StripNoise, 1.0f, Chord },
                               { "transiente +", P::StripTrans, 0.8f, Clap }, { "transiente -", P::StripTrans, -0.8f, Clap }, { "brillo", P::StripBright, 1.0f, HatC },
                               { "comp VCA", P::StripComp, 0.8f, Bass }, { "drive valvula", P::StripDrive, 0.7f, Bass } };
        std::string d;
        bool ok = true;
        for (const auto& c : cases)
        {
            Engine e;
            drums (e);
            auto v = defaults();
            v[(size_t) P::track (c.track, c.field)] = c.value;
            if (std::string (c.name) == "drive valvula") v[(size_t) P::track (c.track, P::StripType)] = 1;
            const auto o = run (e, v, kBar);
            bool fin = true;
            float peak = 0;
            for (size_t i = 0; i < o.l.size(); ++i) { fin &= std::isfinite (o.l[i]) && std::isfinite (o.r[i]); peak = std::max (peak, std::fabs (o.l[i])); }
            const double diff = maxDiff (o, ref);
            ok &= fin && diff > 1e-3 && peak < 1.2f;
            d += std::string (c.name) + " ";
        }
        // Los 4 tipos de drive suenan distinto entre sí
        Out types[4];
        for (int ty = 0; ty < 4; ++ty)
        {
            Engine e;
            drums (e);
            auto v = defaults();
            v[(size_t) P::track (Bass, P::StripDrive)] = 0.8f;
            v[(size_t) P::track (Bass, P::StripType)] = (float) ty;
            types[ty] = run (e, v, kBar / 2);
        }
        for (int a = 0; a < 4; ++a) for (int b = a + 1; b < 4; ++b) ok &= maxDiff (types[a], types[b]) > 1e-3;
        all &= report ("tira", ok, d + "· cinta, valvula, transistor y diodo distintos");

        // El ruido sigue a la envolvente: con la pista en silencio no hay ruido
        Engine en;
        en.bank.clearAll();
        en.bank.set (0, Rim, 0, hit());
        auto vn = defaults();
        vn[(size_t) P::track (Rim, P::StripNoise)] = 1.0f;
        const auto on = run (en, vn, kBar);
        const double during = rms (on.l, 0, 2000), after = rms (on.l, kBar / 2, kBar);
        std::snprintf (buf, sizeof (buf), "ruido durante el golpe %.4f, en silencio %.6f", during, after);
        all &= report ("tira ruido", after < 1e-4 && during > 1e-3, buf);

        // Compresor: achica la diferencia entre una nota fuerte y una suave (VCA y FET)
        for (int fet : { 0, 1 })
        {
            double ratio[2];
            for (int on = 0; on < 2; ++on)
            {
                Engine ec;
                ec.bank.clearAll();
                ec.bank.set (0, Chord, 0, hit (0, 127, 56));
                ec.bank.set (0, Chord, 8, hit (0, 30, 56));
                auto vc = defaults();
                vc[(size_t) P::track (Chord, P::EnvS)] = 1.0f;
                vc[(size_t) P::track (Chord, P::StripComp)] = on ? 1.0f : 0.0f;
                vc[(size_t) P::track (Chord, P::StripCompType)] = (float) fet;
                const auto oc = run (ec, vc, kBar);
                ratio[on] = rms (oc.l, 4000, kBar / 2 - 2000) / (rms (oc.l, kBar / 2 + 4000, kBar - 2000) + 1e-9);
            }
            std::snprintf (buf, sizeof (buf), "%s en acordes: fuerte / suave %.1f dB -> %.1f dB", fet ? "FET" : "VCA",
                           20.0 * std::log10 (ratio[0]), 20.0 * std::log10 (ratio[1]));
            all &= report ("tira comp", ratio[1] < ratio[0] * 0.8, buf);
        }
    }
    {
        // CPU con un patrón realista (INVENTAR): sin tira y con la tira completa en las 10 pistas
        auto cpuOf = [] (bool strip) {
            Engine e;
            e.bank.clearAll();
            inventAll (e.bank, 0, 99, GenreMinimal, false);
            auto v = defaults();
            v[P::MComp] = 0.4f; v[P::DlyRet] = 0.5f; v[P::RevRet] = 0.5f; v[P::BusComp] = 0.6f;
            if (strip)
                for (int t = 0; t < kTracks; ++t)
                {
                    v[(size_t) P::track (t, P::StripDrive)] = 0.5f; v[(size_t) P::track (t, P::StripBody)] = 0.4f; v[(size_t) P::track (t, P::StripNoise)] = 0.3f;
                    v[(size_t) P::track (t, P::StripTrans)] = 0.3f; v[(size_t) P::track (t, P::StripBright)] = 0.4f; v[(size_t) P::track (t, P::StripComp)] = 0.5f;
                }
            v[P::Character] = 1; v[P::Quality] = 0;
            double best = 1e9;
            for (int k = 0; k < 2; ++k)   // el mejor de dos (la máquina puede estar ocupada)
            {
                const auto t0 = std::chrono::steady_clock::now();
                run (e, v, kBar * 4, {}, 512);
                best = std::min (best, std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / 8.0 * 100.0);
            }
            return best;
        };
        const double base = cpuOf (false), full = cpuOf (true);
        std::snprintf (buf, sizeof (buf), "patron realista, LIMPIO: sin tira %.1f %%, tira completa en las 10 pistas %.1f %% (x%.2f)", base, full, full / base);
        all &= report ("cpu", full / base < 1.7 && full < 25.0, buf);
    }

    // ---- 3) Escenas A/B y MORPH ----
    {
        auto bassPat = [] (Engine& e) { e.bank.clearAll(); for (int s = 0; s < 16; s += 4) e.bank.set (0, Bass, s, hit (0, 110, 6)); };
        const int cut = P::sound (Bass, 2), res = P::sound (Bass, 3);
        auto v = defaults();
        std::vector<float> a = v, b = v;
        a[(size_t) cut] = 0.2f;
        b[(size_t) cut] = 0.8f;   // la resonancia es igual en las dos: queda la de la perilla
        Engine ea, eb, em0, em1, emh, ref05;
        for (auto* e : { &ea, &eb, &em0, &em1, &emh, &ref05 }) bassPat (*e);
        const auto ra = run (ea, a, kBar / 2), rb = run (eb, b, kBar / 2);
        auto vm = v; vm[P::MorphOn] = 1;
        for (auto* e : { &em0, &em1, &emh }) { e->setSnapshot (0, a); e->setSnapshot (1, b); }
        vm[P::Morph] = 0.0f; const auto r0 = run (em0, vm, kBar / 2);
        vm[P::Morph] = 1.0f; const auto r1 = run (em1, vm, kBar / 2);
        vm[P::Morph] = 0.5f; const auto rh = run (emh, vm, kBar / 2);
        auto half = v; half[(size_t) cut] = 0.2f + (0.8f - 0.2f) * 0.5f;
        (void) res;
        const auto rr = run (ref05, half, kBar / 2);
        const bool ok = maxDiff (r0, ra) == 0.0 && maxDiff (r1, rb) == 0.0 && maxDiff (rh, rr) == 0.0;
        std::snprintf (buf, sizeof (buf), "MORPH 0 = escena A, 1 = escena B, 0.5 = punto medio (dif %.2g)", maxDiff (rh, rr));
        all &= report ("morph", ok, buf);
    }

    // ---- 4) Deriva lenta: se mueve, sale igual en cada render y no rompe nada ----
    {
        auto pat = [] (Engine& e) { e.bank.clearAll(); for (int s = 0; s < 16; s += 2) { e.bank.set (0, HatC, s, hit()); e.bank.set (0, Chord, s, hit (0, 100, 8)); } };
        Engine a, b, c;
        pat (a); pat (b); pat (c);
        auto v = defaults();
        auto vd = v; vd[P::Drift] = 1.0f; vd[P::DriftBars] = 0;
        const auto r0 = run (a, v, kBar * 4), r1 = run (b, vd, kBar * 4), r2 = run (c, vd, kBar * 4);
        bool fin = true;
        for (float x : r1.l) fin &= std::isfinite (x);
        std::snprintf (buf, sizeof (buf), "con deriva cambia (dif %.3f), dos renders iguales (dif %.2g)", maxDiff (r0, r1), maxDiff (r1, r2));
        all &= report ("deriva", fin && maxDiff (r0, r1) > 1e-3 && maxDiff (r1, r2) == 0.0, buf);
    }

    // ---- 5) Suma de consola: sutil ----
    {
        Engine a, b;
        for (auto* e : { &a, &b }) { e->bank.clearAll(); for (int s = 0; s < 16; s += 4) { e->bank.set (0, Kick, s, hit()); e->bank.set (0, Bass, s + 2, hit (0, 110, 6)); } }
        auto v = defaults();
        auto vc = v; vc[P::Console] = 1;
        const auto r0 = run (a, v, kBar), r1 = run (b, vc, kBar);
        const double db = 20.0 * std::log10 (rms (r1.l, 0, r1.l.size()) / rms (r0.l, 0, r0.l.size()));
        std::snprintf (buf, sizeof (buf), "consola: cambia el sonido (dif %.4f) con %+.2f dB de nivel", maxDiff (r0, r1), db);
        all &= report ("consola", maxDiff (r0, r1) > 1e-4 && std::fabs (db) < 1.0, buf);
    }

    std::printf (all ? "ANALOGICO OK\n" : "ANALOGICO CON FALLAS\n");
    return all ? 0 : 1;
}
