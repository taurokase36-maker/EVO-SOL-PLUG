// OGRO 2 — tests del secuenciador de la fase 2: condiciones por paso, fills, cadena de
// patrones, cambio de patrón, polirritmos desde el patrón, nudge, groove, humanización,
// swing por pista, p-locks, octava por paso y efectos de fin de compás.
#include "../Source/engine/Engine.h"
#include "../Source/engine/Generator.h"
#include <chrono>
#include <cstdio>
#include <functional>
#include <set>

using namespace ogro2;

namespace
{
constexpr double kFs = 44100.0;
constexpr int64_t kBar = 88200;     // 1 compás a 120 BPM
constexpr int64_t kStep = kBar / 16;

std::vector<float> defaults()
{
    const auto& d = paramDefs();
    std::vector<float> v (d.size());
    for (size_t i = 0; i < d.size(); ++i) v[i] = d[i].def;
    v[P::Swing] = 0.5f;
    v[P::Tempo] = 120.0f;
    v[P::Tonal] = 0.0f;   // estos tests usan semitonos
    v[P::MComp] = 0; v[P::MWarmth] = 0; v[P::BusComp] = 0; v[P::BusCrudo] = 0; v[P::MTexture] = 0;
    v[P::DlyRet] = 0; v[P::RevRet] = 0;
    for (int m = 0; m < 4; ++m) v[(size_t) (P::Macro1 + m)] = 0.5f;
    for (int t = 0; t < kTracks; ++t) v[(size_t) P::sound (t, 7)] = t >= Bass ? v[(size_t) P::sound (t, 7)] : 0.0f;
    return v;
}

struct Run
{
    std::vector<float> l, r;
    std::vector<int64_t> trig[kTracks];
};

Run run (Engine& eng, std::vector<float> v, int64_t total, double startPpq = 0.0, int block = 512,
         const std::function<void (int64_t, std::vector<float>&)>& automation = {})
{
    eng.prepare (kFs, block);
    Run r;
    r.l.assign ((size_t) total, 0.0f); r.r.assign ((size_t) total, 0.0f);
    for (int64_t pos = 0; pos < total; pos += block)
    {
        const int n = (int) std::min<int64_t> (block, total - pos);
        if (automation) automation (pos, v);
        HostInfo h; h.valid = true; h.playing = true; h.bpm = 120.0;
        h.ppq = startPpq + (double) pos * 2.0 / kFs;
        eng.setParams (v.data(), (int) v.size());
        eng.process (h, nullptr, 0, r.l.data() + pos, r.r.data() + pos, n);
    }
    const uint32_t c = eng.triggerLog.count.load();
    for (uint32_t k = c > (uint32_t) TriggerLog::kSize ? c - (uint32_t) TriggerLog::kSize : 0; k < c; ++k)
        r.trig[eng.triggerLog.entries[k % TriggerLog::kSize].track].push_back (eng.triggerLog.entries[k % TriggerLog::kSize].sample);
    return r;
}

std::set<int> barsOf (const std::vector<int64_t>& t)
{
    std::set<int> b;
    for (auto s : t) b.insert ((int) ((s + kStep / 2) / kBar));   // los golpes pueden caer una muestra antes
    return b;
}
std::string list (const std::set<int>& s)
{
    std::string o;
    for (int x : s) o += (o.empty() ? "" : ",") + std::to_string (x);
    return "{" + o + "}";
}
bool report (const char* name, bool ok, const std::string& detail)
{
    std::printf ("[%s] %s  %s\n", name, ok ? "OK   " : "FALLA", detail.c_str());
    return ok;
}
Step hit (int vel = 110) { Step s; s.on = true; s.vel = vel; return s; }
double energy (const Run& r, int64_t a, int64_t b)
{
    double e = 0;
    for (int64_t i = std::max<int64_t> (0, a); i < std::min<int64_t> ((int64_t) r.l.size(), b); ++i) e += (double) r.l[(size_t) i] * r.l[(size_t) i];
    return e;
}
double maxDiff (const Run& a, const Run& b, int64_t from, int64_t to)
{
    double d = 0;
    for (int64_t i = from; i < to; ++i)
        d = std::max ({ d, (double) std::fabs (a.l[(size_t) i] - b.l[(size_t) i]), (double) std::fabs (a.r[(size_t) i] - b.r[(size_t) i]) });
    return d;
}
} // namespace

int main()
{
    bool all = true;
    char buf[256];

    // ---- 1) Condiciones A:B y primera vuelta ----
    {
        struct C { int cond; std::set<int> want; };
        const C cases[] = { { CondRatioBase + 0, { 0, 2, 4, 6 } },        // 1:2
                            { CondRatioBase + 6, { 1, 5 } },              // 2:4
                            { CondFirst, { 0 } },
                            { CondNotFirst, { 1, 2, 3, 4, 5, 6, 7 } } };
        for (const auto& c : cases)
        {
            Engine eng;
            eng.bank.clearAll();
            Step s = hit(); s.cond = c.cond;
            eng.bank.set (0, Rim, 0, s);
            const auto r = run (eng, defaults(), kBar * 8);
            const auto got = barsOf (r.trig[Rim]);
            std::snprintf (buf, sizeof (buf), "condicion %-5s suena en los compases %s", condLabel (c.cond).c_str(), list (got).c_str());
            all &= report ("condiciones", got == c.want, buf);
        }
    }

    // ---- 2) Fill cada 4 compases: condiciones FILL / !FILL y patrón de fill ----
    {
        Engine eng;
        eng.bank.clearAll();
        Step f = hit(); f.cond = CondFill;
        Step nf = hit(); nf.cond = CondNotFill;
        eng.bank.set (0, Rim, 0, f);
        eng.bank.set (0, Clap, 0, nf);
        auto v = defaults();
        v[P::Fill] = 2; v[P::FillPat] = 0;
        const auto r = run (eng, v, kBar * 8);
        const bool ok = barsOf (r.trig[Rim]) == std::set<int> { 3, 7 } && barsOf (r.trig[Clap]) == std::set<int> { 0, 1, 2, 4, 5, 6 };
        all &= report ("fill", ok, "FILL en " + list (barsOf (r.trig[Rim])) + ", !FILL en " + list (barsOf (r.trig[Clap])));

        Engine e2;
        e2.bank.clearAll();
        e2.bank.set (0, Rim, 0, hit());
        e2.bank.set (1, Clap, 0, hit());
        v[P::FillPat] = 2;   // patrón B
        const auto r2 = run (e2, v, kBar * 8);
        const bool ok2 = barsOf (r2.trig[Rim]) == std::set<int> { 0, 1, 2, 4, 5, 6 } && barsOf (r2.trig[Clap]) == std::set<int> { 3, 7 };
        all &= report ("fill", ok2, "patron B como fill: A en " + list (barsOf (r2.trig[Rim])) + ", B en " + list (barsOf (r2.trig[Clap])));
    }

    // ---- 3) PRE: suena solo si el paso anterior con probabilidad sonó ----
    {
        Engine eng;
        eng.bank.clearAll();
        Step a = hit(); a.prob = 50;
        Step b = hit(); b.cond = CondPre;
        Step c = hit(); c.cond = CondNotPre;
        eng.bank.set (0, Rim, 0, a);
        eng.bank.set (0, Rim, 4, b);
        eng.bank.set (0, Rim, 8, c);
        const auto r = run (eng, defaults(), kBar * 16);
        std::set<int> first, pre, notPre;
        for (auto s : r.trig[Rim])
        {
            const int64_t q = s + kStep / 2;
            const int step = (int) ((q % kBar) / kStep);
            (step == 0 ? first : step == 4 ? pre : notPre).insert ((int) (q / kBar));
        }
        std::set<int> rest;
        for (int i = 0; i < 16; ++i) if (! first.count (i)) rest.insert (i);
        const bool ok = pre == first && notPre == rest && ! first.empty() && first.size() < 16;
        std::snprintf (buf, sizeof (buf), "el 50%% sono en %zu de 16 compases; PRE lo siguio y !PRE sono en los otros", first.size());
        all &= report ("pre", ok, buf);
    }

    // ---- 4) Cadena de patrones: A x2, B x1 (sale de la posición del tema) ----
    {
        for (double startBar : { 0.0, 2.0, 4.0 })
        {
            Engine eng;
            eng.bank.clearAll();
            eng.bank.set (0, Rim, 0, hit());
            eng.bank.set (1, Clap, 0, hit());
            eng.bank.setChain ({ { 0, 2 }, { 1, 1 } });
            auto v = defaults();
            v[P::Chain] = 1;
            const auto r = run (eng, v, kBar * 6, startBar * 4.0);
            std::set<int> a, b;
            for (int x : barsOf (r.trig[Rim])) a.insert (x + (int) startBar);
            for (int x : barsOf (r.trig[Clap])) b.insert (x + (int) startBar);
            std::set<int> wa, wb;
            for (int bar = (int) startBar; bar < (int) startBar + 6; ++bar) (bar % 3 == 2 ? wb : wa).insert (bar);
            std::snprintf (buf, sizeof (buf), "arrancando en el compas %d: A en %s, B en %s", (int) startBar, list (a).c_str(), list (b).c_str());
            all &= report ("cadena", a == wa && b == wb, buf);
        }
    }

    // ---- 5) Cambio de patrón: al compás o al final de un patrón de 32 pasos ----
    {
        for (int mode : { 0, 1 })
        {
            Engine eng;
            eng.bank.clearAll();
            eng.bank.setPatternLength (0, 32);
            for (int t : { Rim }) { eng.bank.setLength (0, t, 32); eng.bank.set (0, t, 0, hit()); eng.bank.set (0, t, 16, hit()); }
            eng.bank.set (1, Clap, 0, hit());
            auto v = defaults();
            v[P::PatChange] = (float) mode;
            const auto r = run (eng, v, kBar * 4, 0.0, 512, [] (int64_t pos, std::vector<float>& vv) { if (pos >= kBar / 2) vv[P::Pattern] = 1; });
            const int firstB = r.trig[Clap].empty() ? -1 : (int) ((r.trig[Clap][0] + kStep / 2) / kBar);
            std::snprintf (buf, sizeof (buf), "%s: B pedido en el compas 0 entra en el compas %d", mode == 0 ? "al compas" : "al final del patron", firstB);
            all &= report ("cambio", firstB == (mode == 0 ? 1 : 2), buf);
        }
    }

    // ---- 6) Polirritmos: "desde el patrón" reinicia la pista de 3 pasos al cambiar de patrón ----
    {
        for (int poly : { 0, 1 })
        {
            Engine eng;
            eng.bank.clearAll();
            for (int p : { 0, 1 }) { eng.bank.setLength (p, Rim, 3); eng.bank.set (p, Rim, 0, hit()); }
            auto v = defaults();
            v[P::PolyMode] = (float) poly;
            const auto r = run (eng, v, kBar * 2, 0.0, 512, [] (int64_t pos, std::vector<float>& vv) { if (pos >= kBar / 2) vv[P::Pattern] = 1; });
            int64_t firstAfter = -1;
            for (auto s : r.trig[Rim]) if (s >= kBar - kStep / 2) { firstAfter = s; break; }
            const int step = (int) ((firstAfter - kBar + kStep / 2) / kStep);
            std::snprintf (buf, sizeof (buf), "%s: el primer golpe del patron nuevo cae en el paso %d", poly == 0 ? "desde el patron" : "continuos (1.0)", step);
            all &= report ("polirritmo", step == (poly == 0 ? 0 : 2), buf);
        }
    }

    // ---- 7) Nudge, groove, humanización y swing por pista ----
    {
        Engine eng;
        eng.bank.clearAll();
        Step a = hit(); a.nudge = 40;
        Step b = hit(); b.nudge = -40;
        eng.bank.set (0, Rim, 0, a);
        eng.bank.set (0, Rim, 4, b);
        const auto r = run (eng, defaults(), kBar);
        const int64_t want0 = (int64_t) (0.4 * kStep), want4 = 4 * kStep - (int64_t) (0.4 * kStep);
        const bool ok = r.trig[Rim].size() == 2 && std::llabs (r.trig[Rim][0] - want0) <= 1 && std::llabs (r.trig[Rim][1] - want4) <= 1;
        std::snprintf (buf, sizeof (buf), "+40%% y -40%%: %lld y %lld muestras (esperado %lld y %lld)", (long long) (r.trig[Rim].size() > 0 ? r.trig[Rim][0] : -1),
                       (long long) (r.trig[Rim].size() > 1 ? r.trig[Rim][1] : -1), (long long) want0, (long long) want4);
        all &= report ("nudge", ok, buf);
    }
    {
        Engine eng;
        eng.bank.clearAll();
        for (int s = 0; s < 16; ++s) eng.bank.set (0, Rim, s, hit());
        auto v = defaults();
        v[P::Groove] = 3; v[P::GrooveAmt] = 1.0f;
        const auto r = run (eng, v, kBar);
        static const float loose[16] = { 0.0f, 0.06f, -0.03f, 0.09f, 0.0f, 0.04f, -0.05f, 0.07f, -0.02f, 0.05f, -0.04f, 0.08f, 0.01f, 0.03f, -0.06f, 0.10f };
        int64_t worst = 0;
        bool count = r.trig[Rim].size() >= 16;
        for (size_t i = 0; count && i < 16; ++i)
        {
            const double want = ((double) i + loose[i]) * (double) kBar / 16.0;
            worst = std::max (worst, std::llabs (r.trig[Rim][i] - (int64_t) want));
        }
        std::snprintf (buf, sizeof (buf), "Suelto (Villalobos): 16 golpes corridos como la plantilla, error max %lld muestras", (long long) worst);
        all &= report ("groove", count && worst <= 1, buf);
    }
    {
        auto v = defaults();
        v[P::Humanize] = 10.0f;
        std::vector<int64_t> runs[2];
        for (int k = 0; k < 2; ++k)
        {
            Engine eng;
            eng.bank.clearAll();
            for (int s = 0; s < 16; ++s) eng.bank.set (0, Rim, s, hit());
            for (auto s : run (eng, v, kBar * 2).trig[Rim]) if (s < kBar * 2 - kStep / 2) runs[k].push_back (s);
        }
        int64_t worst = 0, moved = 0;
        for (size_t i = 0; i < runs[0].size(); ++i)
        {
            const int64_t d = runs[0][i] - (int64_t) i * kStep;
            worst = std::max (worst, std::llabs (d));
            moved += std::llabs (d) > 20 ? 1 : 0;
        }
        const bool ok = runs[0] == runs[1] && runs[0].size() == 32 && worst <= 446 && moved > 16;
        std::snprintf (buf, sizeof (buf), "10 ms: corrimiento max %.1f ms, %lld de 32 golpes movidos, dos renders iguales", worst * 1000.0 / kFs, (long long) moved);
        all &= report ("humanizar", ok, buf);
    }
    {
        Engine eng;
        eng.bank.clearAll();
        eng.bank.set (0, Rim, 1, hit());
        eng.bank.set (0, Clap, 1, hit());
        auto v = defaults();
        v[(size_t) P::track (Rim, P::SwingOwn)] = 1; v[(size_t) P::track (Rim, P::SwingAmt)] = 0.66f;
        const auto r = run (eng, v, kBar);
        const int64_t wantRim = kStep + (int64_t) (0.32 * kStep);
        const bool ok = r.trig[Rim].size() == 1 && r.trig[Clap].size() == 1 && std::llabs (r.trig[Rim][0] - wantRim) <= 1 && r.trig[Clap][0] == kStep;
        all &= report ("swing pista", ok, "rim con swing propio 66%, clap recto");
    }

    // ---- 8) P-locks: un lock en todos los pasos = mover la perilla; y vuelve en el paso sin lock ----
    {
        const int cutoff = P::sound (Bass, 2);
        const auto& d = paramDefs()[(size_t) cutoff];
        Engine a, b;
        for (auto* e : { &a, &b })
        {
            e->bank.clearAll();
            for (int s = 0; s < 16; s += 4) e->bank.set (0, Bass, s, hit());
        }
        for (int s = 0; s < 16; s += 4) a.bank.setLock (0, Bass, s, cutoff, 0.85f);
        auto v = defaults();
        const auto ra = run (a, v, kBar * 2);
        v[(size_t) cutoff] = d.fromNorm (0.85f);
        const auto rb = run (b, v, kBar * 2);
        const double diff = maxDiff (ra, rb, 0, kBar * 2);
        std::snprintf (buf, sizeof (buf), "lock de corte en cada paso vs perilla: diferencia max %.1f dB", 20.0 * std::log10 (diff + 1e-12));
        all &= report ("p-lock", diff < 1e-3, buf);

        Engine c;
        c.bank.clearAll();
        c.bank.set (0, Bass, 0, hit());
        c.bank.set (0, Bass, 8, hit());
        c.bank.setLock (0, Bass, 0, P::track (Bass, P::Level), 0.0f);
        const auto rc = run (c, defaults(), kBar);
        const double e0 = energy (rc, 0, 8 * kStep), e8 = energy (rc, 8 * kStep, 16 * kStep);
        std::snprintf (buf, sizeof (buf), "nivel en 0 solo en el paso 1: energia %.2g, paso 9 sin lock: %.2g", e0, e8);
        all &= report ("p-lock", e0 < 1e-6 && e8 > 1e-3, buf);
    }

    // ---- 9) Octava por paso = nota +12 ----
    {
        Engine a, b;
        a.bank.clearAll(); b.bank.clearAll();
        Step s = hit(); s.octave = 1;
        a.bank.set (0, Bass, 0, s);
        s.octave = 0; s.note = 12;
        b.bank.set (0, Bass, 0, s);
        const auto ra = run (a, defaults(), kBar), rb = run (b, defaults(), kBar);
        all &= report ("octava", maxDiff (ra, rb, 0, kBar) == 0.0, "octava +1 suena igual que +12 semitonos");
    }

    // ---- 10) Efectos de fin de compás: solo en la ventana, sin NaN, con nivel sano ----
    {
        auto groove = [] (Engine& e) {
            e.bank.clearAll();
            for (int s = 0; s < 16; ++s) e.bank.set (0, HatC, s, hit (90));
            for (int s = 0; s < 16; s += 4) { e.bank.set (0, Kick, s, hit()); e.bank.set (0, Bass, s + 2, hit()); }
            e.bank.set (0, Chord, 0, hit()); e.bank.set (0, Clap, 4, hit()); e.bank.set (0, Clap, 12, hit());
        };
        auto v = defaults();
        v[P::DlyRet] = 0.5f; v[P::RevRet] = 0.5f;
        Engine dry;
        groove (dry);
        const auto r0 = run (dry, v, kBar * 2);
        for (int k = 0; k < P::kBfx; ++k)
            for (int target : { 0, 1, 2, 3 + Bass })
            {
                Engine eng;
                groove (eng);
                auto w = v;
                w[P::BfxEvery] = 1; w[P::BfxLen] = 2; w[P::BfxAmount] = 1.0f;
                w[(size_t) P::bfx (k, P::BfxProb)] = 1.0f;
                w[(size_t) P::bfx (k, P::BfxTarget)] = (float) target;
                const auto r = run (eng, w, kBar * 2);
                const int64_t win = 14 * kStep;
                const double before = maxDiff (r, r0, 0, win - 2);
                const double inside = maxDiff (r, r0, win + 200, win + kBar / 2);
                bool finite = true;
                float peak = 0;
                for (size_t i = 0; i < r.l.size(); ++i) { finite &= std::isfinite (r.l[i]) && std::isfinite (r.r[i]); peak = std::max (peak, std::fabs (r.l[i])); }
                std::snprintf (buf, sizeof (buf), "%-18s en %-15s antes %.1g, en la ventana %.3f, pico %.2f", bfxName (k),
                               bfxTargetNames()[(size_t) target].c_str(), before, inside, peak);
                all &= report ("fin de compas", finite && before == 0.0 && inside > 1e-3 && peak < 1.5f, buf);
            }
    }
    {
        // Azar FIJO: probabilidad 50 %, dos renders iguales y algunos compases sí y otros no
        auto v = defaults();
        v[P::BfxEvery] = 1; v[P::BfxLen] = 2;
        v[(size_t) P::bfx (BfxStutter, P::BfxProb)] = 0.5f;
        Run rr[2];
        uint32_t seen = 0;
        for (int k = 0; k < 2; ++k)
        {
            Engine eng;
            eng.bank.clearAll();
            for (int s = 0; s < 16; ++s) eng.bank.set (0, HatC, s, hit (90));
            rr[k] = run (eng, v, kBar * 8);
        }
        Engine ref;
        ref.bank.clearAll();
        for (int s = 0; s < 16; ++s) ref.bank.set (0, HatC, s, hit (90));
        auto v0 = v; v0[(size_t) P::bfx (BfxStutter, P::BfxProb)] = 0.0f;
        const auto r0 = run (ref, v0, kBar * 8);
        int barsWith = 0;
        for (int bar = 0; bar < 8; ++bar) barsWith += maxDiff (rr[0], r0, bar * kBar + 14 * kStep + 200, (bar + 1) * kBar) > 1e-3 ? 1 : 0;
        (void) seen;
        std::snprintf (buf, sizeof (buf), "stutter al 50%%: sono en %d de 8 compases, dos renders iguales", barsWith);
        all &= report ("fin de compas", maxDiff (rr[0], rr[1], 0, kBar * 8) == 0.0 && barsWith > 0 && barsWith < 8, buf);
    }
    {
        // CPU con los 7 efectos en cada compás (preset completo)
        Engine eng;
        eng.bank.clearAll();
        for (int t = 0; t < kTracks; ++t) for (int s = 0; s < 16; s += (t % 3) + 1) eng.bank.set (0, t, s, hit (100));
        auto v = defaults();
        v[P::BfxEvery] = 1; v[P::BfxLen] = 4;
        for (int k = 0; k < P::kBfx; ++k) v[(size_t) P::bfx (k, P::BfxProb)] = 1.0f;
        v[(size_t) P::bfx (BfxFilter, P::BfxTarget)] = 1;
        v[(size_t) P::bfx (BfxCrush, P::BfxTarget)] = 2;
        v[(size_t) P::bfx (BfxTapeStop, P::BfxTarget)] = 3 + Chord;
        const auto t0 = std::chrono::steady_clock::now();
        run (eng, v, kBar * 8);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        const double cpu = secs / (8.0 * 2.0) * 100.0;
        std::snprintf (buf, sizeof (buf), "8 pistas + 7 efectos en cada compas: %.1f %% de un nucleo", cpu);
        all &= report ("cpu", cpu < 20.0, buf);
    }

    // ---- 11) Generador: euclidiano, INVENTAR por género (repetible) y MUTAR ----
    {
        PatternBank b;
        euclidPattern (b, 0, Perc, 3, 8, 0);
        std::string got;
        for (int i = 0; i < 8; ++i) got += b.get (0, Perc, i).on ? 'x' : '.';
        bool ok = got == "x..x..x." && euclidPreview (3, 8, 0) == got && b.getLength (0, Perc) == 8;
        euclidPattern (b, 0, Perc, 5, 16, 2);
        int n = 0;
        for (int i = 0; i < 16; ++i) n += b.get (0, Perc, i).on ? 1 : 0;
        ok &= n == 5 && b.get (0, Perc, 2).on;
        all &= report ("euclidiano", ok, "E(3,8) = " + got + ", E(5,16) rotado 2 = " + euclidPreview (5, 16, 2));
    }
    {
        bool ok = true;
        std::string detail;
        for (int g = 0; g < (int) genreNames().size(); ++g)
        {
            PatternBank a, b;
            inventAll (a, 0, 1234, g);
            inventAll (b, 0, 1234, g);
            ok &= a.serialise() == b.serialise();
            int kicks = 0, notes = 0;
            for (int s = 0; s < 16; ++s) { kicks += a.get (0, Kick, s).on ? 1 : 0; notes += a.get (0, Bass, s).on ? 1 : 0; }
            ok &= kicks >= 2 && notes >= 1;
            if (g != GenreGarage) ok &= a.get (0, Kick, 0).on && a.get (0, Kick, 4).on && a.get (0, Kick, 8).on;
            PatternBank c;
            inventAll (c, 0, 999, g);
            ok &= c.serialise() != a.serialise();
            detail += std::to_string (kicks) + "/" + std::to_string (notes) + " ";
        }
        all &= report ("inventar", ok, "5 generos repetibles con la misma semilla (bombos/notas de bajo: " + detail + ")");
    }
    {
        PatternBank a;
        inventAll (a, 0, 77, GenreMinimal);
        PatternBank b;
        b.deserialise (a.serialise());
        mutatePattern (b, 0, HatC, 0.0f, 5);
        bool ok = a.serialise() == b.serialise();
        for (int t = 0; t < kTracks; ++t) mutatePattern (b, 0, t, 1.0f, 5);
        ok &= a.serialise() != b.serialise();
        for (int s = 0; s < 16; s += 4) ok &= b.get (0, Kick, s).on == a.get (0, Kick, s).on;
        all &= report ("mutar", ok, "0 % no cambia nada; 100 % cambia sin tocar los tiempos del bombo");
    }

    std::printf (all ? "SECUENCIA OK\n" : "SECUENCIA CON FALLAS\n");
    return all ? 0 : 1;
}
