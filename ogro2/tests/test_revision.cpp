// OGRO 2 — tests de regresión de la revisión 2.1 (un test por error arreglado):
//  1) el hat abierto no se apaga cuando el hat cerrado del mismo paso cae unos ms después
//  2) un golpe nuevo de hat abierto no hereda un corte a medias
//  3) notas MIDI de hat abierto y cerrado en el mismo lugar, en cualquier orden
//  4) el hat cerrado del paso siguiente sigue cortando al abierto (como en las máquinas clásicas)
//  5) los fills automáticos no tocan el hat abierto
//  6) fill editado a mano: suena solo en el compás de fill, manda sobre el estilo y se guarda
#include "../Source/engine/Engine.h"
#include <cstdio>
#include <functional>
#include <set>

using namespace ogro2;

namespace
{
constexpr double kFs = 44100.0;
constexpr double kBpm = 127.0;
const double kStepSamples = kFs * 60.0 / kBpm / 4.0;

std::vector<float> defaults()
{
    const auto& d = paramDefs();
    std::vector<float> v (d.size());
    for (size_t i = 0; i < d.size(); ++i) v[i] = d[i].def;
    v[P::Swing] = 0.5f;
    v[P::Tempo] = (float) kBpm;
    v[P::MComp] = 0; v[P::MWarmth] = 0; v[P::BusComp] = 0; v[P::BusCrudo] = 0; v[P::MTexture] = 0;
    v[P::DlyRet] = 0; v[P::RevRet] = 0;
    for (int m = 0; m < 4; ++m) v[(size_t) (P::Macro1 + m)] = 0.5f;
    for (int t = 0; t < kTracks; ++t) v[(size_t) P::sound (t, 7)] = t >= Bass ? v[(size_t) P::sound (t, 7)] : 0.0f;   // sin VARY
    return v;
}

struct Run
{
    std::vector<float> l;
    std::vector<int64_t> trig[kTracks];
    int latency = 0;
};

Run run (Engine& eng, const std::vector<float>& v, int bars, const std::vector<MidiTrigger>& midi = {}, bool playing = true)
{
    const int block = 256;
    const int64_t total = (int64_t) (kStepSamples * 16 * bars);
    eng.prepare (kFs, block);
    Run r;
    r.l.assign ((size_t) total, 0.0f);
    std::vector<float> right ((size_t) total, 0.0f);
    for (int64_t pos = 0; pos < total; pos += block)
    {
        const int n = (int) std::min<int64_t> (block, total - pos);
        HostInfo h; h.valid = true; h.playing = playing; h.bpm = kBpm;
        h.ppq = (double) pos * kBpm / 60.0 / kFs;
        std::vector<MidiTrigger> here;
        for (auto m : midi)
            if (m.sample >= pos && m.sample < pos + n) { m.sample -= (int) pos; here.push_back (m); }
        eng.setParams (v.data(), (int) v.size());
        eng.process (h, here.empty() ? nullptr : here.data(), (int) here.size(), r.l.data() + pos, right.data() + pos, n);
    }
    const uint32_t c = eng.triggerLog.count.load();
    for (uint32_t k = c > (uint32_t) TriggerLog::kSize ? c - (uint32_t) TriggerLog::kSize : 0; k < c; ++k)
        r.trig[eng.triggerLog.entries[k % TriggerLog::kSize].track].push_back (eng.triggerLog.entries[k % TriggerLog::kSize].sample);
    r.latency = eng.getLatencySamples();
    return r;
}

// Nivel (dB) de un golpe entre "fromMs" y "toMs" después del disparo
double levelDb (const Run& r, int64_t trig, double fromMs, double toMs)
{
    const int64_t a = trig + r.latency + (int64_t) (fromMs * 0.001 * kFs), b = trig + r.latency + (int64_t) (toMs * 0.001 * kFs);
    if (b >= (int64_t) r.l.size()) return 1000.0;   // no entra: no se cuenta
    double e = 0;
    for (int64_t i = a; i < b; ++i) e += (double) r.l[(size_t) i] * r.l[(size_t) i];
    return 10.0 * std::log10 (e / (double) (b - a) + 1e-20);
}

// Cuántos golpes de la pista quedaron más de 12 dB por debajo de "ref" (el nivel del hat abierto
// sonando solo, sin hat cerrado), medido de 30 a 70 ms después del disparo
struct Weak { int hits = 0, weak = 0; double worst = 0; };
Weak weakHits (const Run& r, int track, double ref)
{
    Weak w;
    for (auto s : r.trig[track])
    {
        const double d = levelDb (r, s, 30.0, 70.0);
        if (d > 999.0) continue;
        ++w.hits; w.weak += d < ref - 12.0 ? 1 : 0; w.worst = std::min (w.worst, d - ref);
    }
    return w;
}

Step hit (int vel = 100) { Step s; s.on = true; s.vel = vel; return s; }
void soloHatO (std::vector<float>& v) { v[(size_t) P::track (HatO, P::Solo)] = 1.0f; }
void openOffbeats (Engine& e) { for (int s : { 2, 6, 10, 14 }) e.bank.set (0, HatO, s, hit()); }
std::set<int> stepsInBar (const std::vector<int64_t>& t, int bar)
{
    std::set<int> o;
    for (auto s : t)
    {
        const int64_t k = (int64_t) std::floor (((double) s + 2.0) / kStepSamples);   // las repeticiones cuentan en su paso
        if (k / 16 == bar) o.insert ((int) (k % 16));
    }
    return o;
}
std::string list (const std::set<int>& s)
{
    std::string o;
    for (int x : s) o += (o.empty() ? "" : ",") + std::to_string (x + 1);
    return "{" + o + "}";
}
bool report (const char* name, bool ok, const std::string& detail)
{
    std::printf ("[%s] %s  %s\n", name, ok ? "OK   " : "FALLA", detail.c_str());
    return ok;
}
} // namespace

int main()
{
    bool all = true;
    char buf[320];

    // Referencia: el hat abierto solo, con decay largo (lo único que lo puede apagar es un corte)
    auto openOnly = [] (std::vector<float>& v) { soloHatO (v); v[(size_t) P::sound (HatO, 1)] = 0.6f; };
    double ref = 0.0;
    {
        Engine e; e.bank.clearAll(); openOffbeats (e);
        auto v = defaults(); openOnly (v);
        const auto r = run (e, v, 4);
        for (auto s : r.trig[HatO]) { const double d = levelDb (r, s, 30.0, 70.0); if (d < 999.0) ref = std::min (ref, d); }
        std::printf ("(referencia: hat abierto solo = %.1f dB)\n", ref);
    }

    // ---- 1) Hat cerrado en el mismo paso + HUMANIZE: el abierto no se apaga ----
    {
        Engine e; e.bank.clearAll(); openOffbeats (e);
        for (int s = 0; s < 16; ++s) e.bank.set (0, HatC, s, hit (80));
        auto v = defaults(); openOnly (v); v[P::Humanize] = 8.0f;
        const auto w = weakHits (run (e, v, 16), HatO, ref);
        std::snprintf (buf, sizeof (buf), "HUMANIZE 8 ms con hat cerrado en las 16: %d de %d hats abiertos apagados (el peor %.1f dB)", w.weak, w.hits, w.worst);
        all &= report ("hat abierto", w.hits >= 60 && w.weak == 0, buf);
    }
    {
        // Swing propio del hat cerrado: en los pasos impares cae después que el abierto
        Engine e; e.bank.clearAll();
        for (int s : { 1, 5, 9, 13 }) e.bank.set (0, HatO, s, hit());
        for (int s = 0; s < 16; ++s) e.bank.set (0, HatC, s, hit (80));
        auto v = defaults(); openOnly (v);
        v[(size_t) P::track (HatC, P::SwingOwn)] = 1.0f; v[(size_t) P::track (HatC, P::SwingAmt)] = 0.62f;
        const auto w = weakHits (run (e, v, 8), HatO, ref);
        std::snprintf (buf, sizeof (buf), "swing propio del hat cerrado (62 %%): %d de %d apagados", w.weak, w.hits);
        all &= report ("hat abierto", w.hits >= 28 && w.weak == 0, buf);
    }

    // ---- 2) Corte a medias: el hat cerrado suena 9 ms antes (nudge) que un golpe nuevo del abierto,
    //         mientras la cola del golpe anterior sigue viva. El golpe nuevo tiene que sonar entero. ----
    {
        Engine e; e.bank.clearAll();
        e.bank.set (0, HatO, 2, hit()); e.bank.set (0, HatO, 4, hit());
        Step c = hit (80); c.nudge = -8;
        e.bank.set (0, HatC, 4, c);
        auto v = defaults(); openOnly (v);
        const auto w = weakHits (run (e, v, 8), HatO, ref);
        std::snprintf (buf, sizeof (buf), "golpe nuevo 9 ms después de un corte: %d de %d apagados (el peor %.1f dB)", w.weak, w.hits, w.worst);
        all &= report ("hat abierto", w.hits >= 15 && w.weak == 0, buf);
    }

    // ---- 3) MIDI: hat abierto y cerrado en el mismo lugar, en los dos órdenes ----
    {
        for (int order = 0; order < 2; ++order)
        {
            Engine e; e.bank.clearAll();
            auto v = defaults(); openOnly (v); v[P::SeqOn] = 0.0f;
            std::vector<MidiTrigger> midi;
            for (int bar = 0; bar < 4; ++bar)
                for (int q : { 2, 6, 10, 14 })
                {
                    const int s = (int) ((bar * 16 + q) * kStepSamples);
                    const MidiTrigger o { s, HatO, 0.8f }, c { s, HatC, 0.6f };
                    if (order == 0) { midi.push_back (c); midi.push_back (o); }
                    else { midi.push_back (o); midi.push_back (c); }
                }
            const auto w = weakHits (run (e, v, 4, midi), HatO, ref);
            std::snprintf (buf, sizeof (buf), "MIDI, %s: %d de %d apagados", order == 0 ? "cerrado antes que abierto" : "abierto antes que cerrado", w.weak, w.hits);
            all &= report ("hat abierto", w.hits >= 15 && w.weak == 0, buf);
        }
    }

    // ---- 4) El hat cerrado del paso siguiente sigue cortando al abierto ----
    {
        auto tail = [] (bool closedNext)
        {
            Engine e; e.bank.clearAll();
            e.bank.set (0, HatO, 2, hit());
            if (closedNext) e.bank.set (0, HatC, 3, hit (80));
            auto v = defaults(); soloHatO (v);
            v[(size_t) P::sound (HatO, 1)] = 0.6f;
            const auto r = run (e, v, 4);
            // cola del abierto entre 60 y 100 ms después del paso siguiente
            double db = -300.0;
            for (auto s : r.trig[HatO]) db = std::max (db, levelDb (r, s + (int64_t) kStepSamples, 60.0, 100.0) > 999.0 ? -300.0 : levelDb (r, s + (int64_t) kStepSamples, 60.0, 100.0));
            return db;
        };
        const double free = tail (false), cut = tail (true);
        std::snprintf (buf, sizeof (buf), "cola del abierto después del paso siguiente: libre %.1f dB, con hat cerrado %.1f dB", free, cut);
        all &= report ("hat abierto", cut < free - 30.0, buf);
    }

    // ---- 5) Los fills automáticos no tocan el hat abierto ----
    {
        auto groove = [] (Engine& e)
        {
            e.bank.clearAll(); openOffbeats (e);
            for (int q = 0; q < 16; q += 4) e.bank.set (0, Kick, q, hit());
            for (int q = 0; q < 16; q += 2) e.bank.set (0, HatC, q, hit (80));
            e.bank.set (0, Clap, 4, hit()); e.bank.set (0, Clap, 12, hit());
        };
        auto v = defaults(); soloHatO (v); v[P::Fill] = 1;   // cada 2 compases
        v[(size_t) P::sound (HatO, 1)] = 0.2f;
        Engine plainE; groove (plainE);
        auto vp = v; vp[P::Fill] = 0;
        const auto plain = run (plainE, vp, 8);
        static const char* names[] = { "", "ROLL", "BREAK", "BUILD", "RANDOM" };
        for (int style = FillRoll; style <= FillRandom; ++style)
        {
            Engine e; groove (e);
            v[P::FillStyle] = (float) style;
            const auto r = run (e, v, 8);
            const bool sameHits = r.trig[HatO] == plain.trig[HatO];
            // en los compases de fill (1, 3, 5, 7), cada golpe suena igual de fuerte que sin fill (30 a 100 ms)
            double worst = 0.0;
            for (size_t i = 0; i < std::min (r.trig[HatO].size(), plain.trig[HatO].size()); ++i)
            {
                const double a = levelDb (r, r.trig[HatO][i], 30.0, 100.0), b = levelDb (plain, plain.trig[HatO][i], 30.0, 100.0);
                if (a > 999.0 || b > 999.0) continue;
                worst = std::min (worst, a - b);
            }
            std::snprintf (buf, sizeof (buf), "%-6s: %zu golpes (sin fill %zu), el que más baja %.1f dB", names[style], r.trig[HatO].size(), plain.trig[HatO].size(), worst);
            all &= report ("fill y hat abierto", sameHits && worst > -1.0, buf);
        }
    }

    // ---- 6) Fill editado a mano ----
    {
        // a) agrega un golpe que no está en el patrón: suena solo en los compases de fill
        Engine e; e.bank.clearAll();
        for (int q = 0; q < 16; q += 4) e.bank.set (0, Kick, q, hit());
        e.bank.setFillStep (0, Rim, 5, hit (110));
        Step off = e.bank.get (0, Kick, 8); off.on = false;
        e.bank.setFillStep (0, Kick, 8, off);                    // b) saca un golpe del patrón
        auto v = defaults(); v[P::Fill] = 1; v[P::FillStyle] = FillRoll;
        const auto r = run (e, v, 4);
        const bool rimOk = stepsInBar (r.trig[Rim], 0).empty() && stepsInBar (r.trig[Rim], 2).empty()
                        && stepsInBar (r.trig[Rim], 1).count (5) == 1 && stepsInBar (r.trig[Rim], 3).count (5) == 1;
        const bool kickOk = stepsInBar (r.trig[Kick], 0) == std::set<int> { 0, 4, 8, 12 } && stepsInBar (r.trig[Kick], 1).count (8) == 0;
        all &= report ("fill editado", rimOk && kickOk, "rim en el paso 6 solo en los fills: compás 2 " + list (stepsInBar (r.trig[Rim], 1))
                                                         + "; bombo del compás 2 " + list (stepsInBar (r.trig[Kick], 1)));

        // c) lo editado manda sobre el estilo: ROLL prende el clap en 13..16; a mano lo apagamos en 16
        Engine e2; e2.bank.clearAll();
        Step no; no.on = false;
        e2.bank.setFillStep (0, Clap, 15, no);
        const auto r2 = run (e2, v, 2);
        const auto claps = stepsInBar (r2.trig[Clap], 1);
        all &= report ("fill editado", claps == std::set<int> { 12, 13, 14 }, "ROLL con el paso 16 apagado a mano: claps en " + list (claps));

        // d) se guarda y vuelve igual; un estado sin fills editados no trae ninguno
        const std::string text = e.bank.serialise();
        Engine e3; e3.bank.clearAll();
        e3.bank.setFillStep (2, Perc, 3, hit());     // tiene que desaparecer al cargar
        const bool loaded = e3.bank.deserialise (text);
        Step back;
        const bool same = loaded && e3.bank.getFillStep (0, Rim, 5, back) && back.on && back.vel == 110
                       && e3.bank.getFillStep (0, Kick, 8, back) && ! back.on
                       && e3.bank.fillEdits (0) == 2 && e3.bank.fillEdits (2) == 0 && e3.bank.serialise() == text;
        all &= report ("fill editado", same, "guardar y cargar: 2 pasos editados vuelven iguales");

        // e) copiar y borrar una pista se lleva sus pasos de fill
        e3.bank.copyTrack (0, Rim, 1, Rim);
        const bool copied = e3.bank.hasFillStep (1, Rim, 5);
        e3.bank.clearTrack (1, Rim);
        e3.bank.clearFillTrack (0, Kick);
        all &= report ("fill editado", copied && ! e3.bank.hasFillStep (1, Rim, 5) && e3.bank.fillEdits (0) == 1,
                       "copiar pista copia el fill; borrar pista lo borra; RESET deja el estilo");
    }

    std::printf (all ? "REVISION OK\n" : "REVISION CON FALLAS\n");
    return all ? 0 : 1;
}
