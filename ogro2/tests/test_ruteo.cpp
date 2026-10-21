// OGRO 2 — tests de la fase 7: salidas separadas por pista (alineadas con la principal, que no
// cambia), PUMP desde un kick externo (sidechain), exportar el patrón como clip MIDI y MIDI learn.
#include "../Source/engine/Engine.h"
#include "../Source/engine/MidiExport.h"
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
    v[P::Tonal] = 0;
    return v;
}
struct Out
{
    std::vector<float> l, r;
    std::vector<float> aux[kTracks][2];
};
// io: salidas separadas encendidas; sc: señal de sidechain (o vacía)
Out run (Engine& eng, const std::vector<float>& v, int64_t total, bool withAux, const std::vector<float>& sc = {})
{
    eng.prepare (kFs, 512);
    Out o;
    o.l.assign ((size_t) total, 0.0f); o.r.assign ((size_t) total, 0.0f);
    for (auto& a : o.aux) for (auto& c : a) c.assign ((size_t) total, 0.0f);
    for (int64_t pos = 0; pos < total; pos += 512)
    {
        const int n = (int) std::min<int64_t> (512, total - pos);
        Engine::Io io;
        if (withAux)
            for (int t = 0; t < kTracks; ++t) { io.aux[t][0] = o.aux[t][0].data() + pos; io.aux[t][1] = o.aux[t][1].data() + pos; }
        // El sidechain comparte memoria con la salida principal, como en el plugin
        if (! sc.empty())
        {
            for (int i = 0; i < n; ++i) { o.l[(size_t) (pos + i)] = sc[(size_t) (pos + i)]; o.r[(size_t) (pos + i)] = sc[(size_t) (pos + i)]; }
            io.sc[0] = o.l.data() + pos; io.sc[1] = o.r.data() + pos;
        }
        HostInfo h; h.valid = true; h.playing = true; h.bpm = 120.0; h.ppq = (double) pos * 2.0 / kFs;
        eng.setParams (v.data(), (int) v.size());
        eng.process (h, nullptr, 0, o.l.data() + pos, o.r.data() + pos, n, &io);
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
bool report (const char* name, bool ok, const std::string& detail)
{
    std::printf ("[%s] %s  %s\n", name, ok ? "OK   " : "FALLA", detail.c_str());
    return ok;
}
Step hit (int vel = 110, int gate = 0) { Step s; s.on = true; s.vel = vel; s.gate = gate; return s; }
} // namespace

int main()
{
    bool all = true;
    char buf[300];

    // ---- 1) Salidas separadas ----
    {
        auto pat = [] (Engine& e) {
            e.bank.clearAll();
            for (int s = 0; s < 16; s += 4) e.bank.set (0, Kick, s, hit());
            e.bank.set (0, Bass, 2, hit (110, 8));
        };
        Engine a, b;
        pat (a); pat (b);
        auto v = defaults();
        const auto withAux = run (a, v, kBar, true), plain = run (b, v, kBar, false);
        double mainDiff = 0;
        for (size_t i = 0; i < plain.l.size(); ++i) mainDiff = std::max (mainDiff, (double) std::fabs (plain.l[i] - withAux.l[i]));
        const double kickRms = rms (withAux.aux[Kick][0], 0, (size_t) kBar), hatRms = rms (withAux.aux[HatC][0], 0, (size_t) kBar);
        std::snprintf (buf, sizeof (buf), "principal igual con o sin salidas separadas (dif %.2g); KICK %.3f, HAT C %.3f", mainDiff, kickRms, hatRms);
        all &= report ("salidas", mainDiff == 0.0 && kickRms > 0.01 && hatRms == 0.0, buf);

        // Alineación: solo un hat sonando (banda ancha: la correlación tiene un pico neto)
        Engine c;
        c.bank.clearAll();
        c.bank.set (0, HatC, 0, hit());
        const auto k = run (c, v, kBar / 2, true);
        // Desfasaje por correlación cruzada (el pre-eco del sobremuestreo adelanta el primer valor alto)
        int bestLag = 0;
        double best = -1e9;
        for (int lag = -40; lag <= 40; ++lag)
        {
            double c2 = 0;
            for (size_t i = 200; i + 200 < k.l.size(); ++i) c2 += (double) k.l[i] * k.aux[HatC][0][(size_t) ((int64_t) i + lag)];
            if (c2 > best) { best = c2; bestLag = lag; }
        }
        std::snprintf (buf, sizeof (buf), "salida del hat contra la principal: desfasaje %d muestras (latencia del plugin %d)", bestLag, c.getLatencySamples());
        all &= report ("salidas", std::abs (bestLag) <= 1, buf);
    }

    // ---- 2) PUMP desde un kick externo por sidechain ----
    {
        // Pulsos de 1 kHz de 20 ms en cada negra (un kick de otra pista)
        std::vector<float> sc ((size_t) kBar, 0.0f);
        for (int beat = 0; beat < 4; ++beat)
            for (int i = 0; i < 882; ++i) sc[(size_t) (beat * kBar / 4 + i)] = 0.8f * std::sin (2.0f * 3.14159265f * 1000.0f * (float) i / 44100.0f);
        auto pat = [] (Engine& e) { e.bank.clearAll(); e.bank.set (0, Chord, 0, hit (110, 63)); };
        double ratio[2];
        for (int mode = 0; mode < 2; ++mode)
        {
            Engine e;
            pat (e);
            auto v = defaults();
            v[(size_t) P::track (Chord, P::EnvS)] = 1.0f;
            v[(size_t) P::track (Chord, P::Pump)] = 1.0f;
            v[P::PumpSource] = (float) mode;
            v[P::DlyRet] = 0; v[P::RevRet] = 0;
            const auto o = run (e, v, kBar, false, sc);
            // Nivel justo después del segundo pulso contra el nivel antes del tercero
            const size_t b2 = (size_t) (kBar / 4), b3 = (size_t) (kBar / 2);
            const size_t lat = (size_t) e.getLatencySamples();
            ratio[mode] = rms (o.l, b2 + 250 + lat, b2 + 900 + lat) / (rms (o.l, b3 - 4000 + lat, b3 - 500 + lat) + 1e-9);
        }
        std::snprintf (buf, sizeof (buf), "acordes con PUMP: con kick interno (sin kick) %.2f, con sidechain %.2f (se agachan despues de cada pulso)", ratio[0], ratio[1]);
        all &= report ("sidechain", ratio[1] < 0.5 && ratio[0] > 0.8, buf);
    }

    // ---- 3) Exportar el patrón como clip MIDI (archivo .mid estándar) ----
    {
        PatternBank b;
        Step s = hit (100, 0);
        b.set (0, Kick, 0, s); b.set (0, Kick, 8, s);
        Step r = hit (90, 0); r.ratchet = 2;
        b.set (0, HatC, 4, r);
        Step g = hit (127, 16);
        b.set (0, Bass, 2, g);
        const auto mid = patternToMidi (b, 0, -1, 0.5f, 96);
        // Leer de vuelta: cabecera, un track y las notas
        bool ok = mid.size() > 22 && std::string (mid.begin(), mid.begin() + 4) == "MThd" && std::string (mid.begin() + 14, mid.begin() + 18) == "MTrk";
        const auto notes = parseMidiNotes (mid);
        int kicks = 0, hats = 0, bass = 0;
        int bassLen = 0;
        for (const auto& n : notes)
        {
            if (n.note == 36) ++kicks;
            if (n.note == 39) ++hats;
            if (n.note == 42) { ++bass; bassLen = n.length; }
        }
        ok &= kicks == 2 && hats == 2 && bass == 1 && bassLen == 48;   // gate 16/8 = 2 pasos = 48 ticks (96 por negra)
        std::snprintf (buf, sizeof (buf), "%zu bytes: %d kicks, %d golpes del ratchet, bajo de %d ticks (2 pasos)", mid.size(), kicks, hats, bassLen);
        all &= report ("midi", ok, buf);
        // Una sola pista
        const auto solo = parseMidiNotes (patternToMidi (b, 0, Kick, 0.5f, 96));
        bool onlyKick = ! solo.empty();
        for (const auto& n : solo) onlyKick &= n.note == 36;
        all &= report ("midi", onlyKick, "exportar solo una pista");
    }

    // ---- 4) Secuenciador interno apagado y notas MIDI que toman la nota del patrón ----
    {
        Engine e;
        e.bank.clearAll();
        for (int st = 0; st < 16; st += 4) e.bank.set (0, Kick, st, hit());
        auto v = defaults();
        v[P::SeqOn] = 0;
        run (e, v, kBar, false);
        all &= report ("secuenciador", e.triggerLog.count.load() == 0, "apagado: el patrón no dispara nada");
    }
    {
        // Bajo: un clip MIDI dispara G1 (bajo) en el paso 2; el patrón tiene ese paso con nota +7
        auto pitchOf = [] (const std::vector<float>& x, size_t from, size_t len) {
            // Autocorrelación: el primer pico fuerte (evita la octava de abajo)
            std::vector<double> c (1300, 0.0);
            double mx = 0;
            for (int lag = 60; lag < 1200; ++lag)
            {
                double a = 0, e1 = 0, e2 = 0;
                for (size_t i = from; i < from + len; ++i) { a += x[i] * x[i + (size_t) lag]; e1 += x[i] * x[i]; e2 += x[i + (size_t) lag] * x[i + (size_t) lag]; }
                c[(size_t) lag] = a / std::sqrt (e1 * e2 + 1e-20);
                mx = std::max (mx, c[(size_t) lag]);
            }
            for (int lag = 61; lag < 1199; ++lag)
                if (c[(size_t) lag] > 0.9 * mx && c[(size_t) lag] >= c[(size_t) lag - 1] && c[(size_t) lag] >= c[(size_t) lag + 1]) return kFs / lag;
            return 0.0;
        };
        double f[2];
        for (int mode = 0; mode < 2; ++mode)
        {
            Engine e;
            e.bank.clearAll();
            Step st = hit (110); st.note = 7;
            e.bank.set (0, Bass, 2, st);
            auto v = defaults();
            v[P::SeqOn] = 0; v[P::MidiNotes] = (float) mode;
            v[(size_t) P::track (Bass, P::EnvS)] = 1.0f;
            v[(size_t) P::sound (Bass, 2)] = 0.6f; v[(size_t) P::sound (Bass, 6)] = 0.0f;
            e.prepare (kFs, 512);
            std::vector<float> l ((size_t) kBar), r ((size_t) kBar);
            for (int64_t pos = 0; pos < kBar; pos += 512)
            {
                std::vector<MidiTrigger> m;
                const int at = (int) (2 * kBar / 16);
                if (at >= pos && at < pos + 512) m.push_back ({ (int) (at - pos), Bass, 0.86f, false });
                HostInfo h; h.valid = true; h.playing = true; h.bpm = 120.0; h.ppq = (double) pos * 2.0 / kFs;
                e.setParams (v.data(), (int) v.size());
                e.process (h, m.data(), (int) m.size(), l.data() + pos, r.data() + pos, 512);
            }
            f[mode] = pitchOf (l, (size_t) (3 * kBar / 16), 8000);
        }
        const double semis = 12.0 * std::log2 (f[1] / f[0]);
        std::snprintf (buf, sizeof (buf), "nota MIDI del bajo: tonica %.1f Hz, del patron %.1f Hz (%+.1f semitonos)", f[0], f[1], semis);
        all &= report ("notas midi", std::fabs (semis - 7.0) < 0.3, buf);
    }

    std::printf (all ? "RUTEO OK\n" : "RUTEO CON FALLAS\n");
    return all ? 0 : 1;
}
