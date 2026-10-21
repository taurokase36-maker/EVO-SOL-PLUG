// OGRO 2 — comparación A/B contra OGRO 1.0.
// Compila los dos motores en el mismo programa (namespaces ogro y ogro2) y:
//  1) renderiza los 12 presets de fábrica con los dos (16 compases, A -> B -> D)
//     y mide la diferencia muestra a muestra;
//  2) calcula la "huella" de cada pista sola (nivel por paso y energía por banda)
//     y la compara con la de la 1.0 con tolerancias, para cuando un cambio
//     buscado impida la comparación exacta.
// Uso: ab_v1 [--exacto | --normal] [--huellas <archivo.json>]
//   --exacto   OGRO 2 con el preset importado de la 1.0 (azar VIVO, sin saltear voces):
//              las mezclas tienen que dar iguales (diferencia de redondeo como mucho)
//   --normal   OGRO 2 como lo usás (azar FIJO, voces en silencio salteadas): solo se
//              comparan las huellas, con tolerancia
//   --huellas  guarda las huellas de la 1.0 en un JSON
#include "../../ogro/Source/engine/Engine.h"
#include "../../ogro/Source/engine/Presets.h"
#include "../Source/engine/Engine.h"
#include "../Source/engine/Presets.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
constexpr int kFs = 44100;

bool gNormal = false;

struct V1
{
    using Engine = ogro::Engine;
    using HostInfo = ogro::HostInfo;
    static const auto& presets() { return ogro::factoryPresets(); }
    static const auto& defs() { return ogro::paramDefs(); }
    static int paramIndex (const std::string& id) { return ogro::paramIndex (id); }
    static int pattern() { return ogro::P::Pattern; }
    static int tempo() { return ogro::P::Tempo; }
    static int mute (int t) { return ogro::P::track (t, ogro::P::Mute); }
    static int dlyRet() { return ogro::P::DlyRet; }
    static int revRet() { return ogro::P::RevRet; }
    static int texture() { return ogro::P::MTexture; }
    static void afterPrepare (Engine&, size_t) {}
    static std::vector<float> setup (Engine&, size_t pi)
    {
        const auto& d = defs();
        std::vector<float> v (d.size());
        for (size_t i = 0; i < d.size(); ++i) v[i] = d[i].def;
        for (const auto& [id, val] : presets()[pi].values) v[(size_t) paramIndex (id)] = val;
        return v;
    }
};

struct V2
{
    using Engine = ogro2::Engine;
    using HostInfo = ogro2::HostInfo;
    static const auto& presets() { return ogro2::factoryPresets(); }
    static const auto& defs() { return ogro2::paramDefs(); }
    static int paramIndex (const std::string& id) { return ogro2::paramIndex (id); }
    static int pattern() { return ogro2::P::Pattern; }
    static int tempo() { return ogro2::P::Tempo; }
    static int mute (int t) { return ogro2::P::track (t, ogro2::P::Mute); }
    static int dlyRet() { return ogro2::P::DlyRet; }
    static int revRet() { return ogro2::P::RevRet; }
    static int texture() { return ogro2::P::MTexture; }
    static std::vector<float> setup (Engine& eng, size_t pi)
    {
        const auto st = ogro2::factoryState (presets()[pi], gNormal);
        eng.setExactLegacy (! gNormal);
        return st.values;
    }
    // El ruteo se aplica después de prepare(), como en la 1.0 (donde era un parámetro)
    static void afterPrepare (Engine& eng, size_t pi) { ogro2::applyRouting (eng.routing, ogro2::factoryState (presets()[pi], gNormal)); }
};


struct Stereo { std::vector<float> l, r; int latency = 0; };

// Mezcla completa: 16 compases A (8) -> B (4) -> D (4), como render_demos
template <typename S>
Stereo renderMix (size_t pi, int bars)
{
    typename S::Engine eng;
    S::presets()[pi].build (eng.bank);
    auto v = S::setup (eng, pi);
    const double bpm = v[(size_t) S::tempo()];
    eng.prepare (kFs, 512);
    S::afterPrepare (eng, pi);
    const double spb = 4.0 * 60.0 / bpm * kFs;
    const int total = (int) (spb * bars) + kFs;
    Stereo out;
    out.l.assign ((size_t) total, 0.0f);
    out.r.assign ((size_t) total, 0.0f);
    for (int pos = 0; pos < total; pos += 512)
    {
        const double bar = pos / spb;
        v[(size_t) S::pattern()] = bar < bars * 0.5 ? 0.0f : (bar < bars * 0.75 ? 1.0f : 3.0f);
        typename S::HostInfo h;
        h.valid = true; h.playing = pos < (int) (spb * bars); h.bpm = bpm; h.ppq = (double) pos / kFs * bpm / 60.0;
        eng.setParams (v.data(), (int) v.size());
        eng.process (h, nullptr, 0, out.l.data() + pos, out.r.data() + pos, std::min (512, total - pos));
    }
    out.latency = eng.getLatencySamples();
    return out;
}

// Pista sola, patrón A, sin retornos de delay/reverb ni textura
template <typename S>
Stereo renderStem (size_t pi, int track, int bars)
{
    typename S::Engine eng;
    S::presets()[pi].build (eng.bank);
    auto v = S::setup (eng, pi);
    for (int t = 0; t < 8; ++t) v[(size_t) S::mute (t)] = t == track ? 0.0f : 1.0f;
    v[(size_t) S::dlyRet()] = 0; v[(size_t) S::revRet()] = 0; v[(size_t) S::texture()] = 0;
    const double bpm = v[(size_t) S::tempo()];
    eng.prepare (kFs, 512);
    S::afterPrepare (eng, pi);
    const double spb = 4.0 * 60.0 / bpm * kFs;
    const int total = (int) (spb * bars);
    Stereo out;
    out.l.assign ((size_t) total, 0.0f);
    out.r.assign ((size_t) total, 0.0f);
    for (int pos = 0; pos < total; pos += 512)
    {
        typename S::HostInfo h;
        h.valid = true; h.playing = true; h.bpm = bpm; h.ppq = (double) pos / kFs * bpm / 60.0;
        eng.setParams (v.data(), (int) v.size());
        eng.process (h, nullptr, 0, out.l.data() + pos, out.r.data() + pos, std::min (512, total - pos));
    }
    out.latency = eng.getLatencySamples();
    return out;
}

double db (double energy, size_t n) { return 10.0 * std::log10 (energy / (double) std::max<size_t> (1, n) + 1e-20); }

// Huella: nivel por semicorchea y energía en 10 bandas de octava (31 Hz a 16 kHz)
struct Fingerprint
{
    double rms = -200.0;
    std::vector<double> steps, bands;
};

Fingerprint fingerprint (const Stereo& s, double bpm, int bars)
{
    Fingerprint f;
    const size_t n = s.l.size();
    double e = 0;
    for (size_t i = 0; i < n; ++i) e += 0.5 * ((double) s.l[i] * s.l[i] + (double) s.r[i] * s.r[i]);
    f.rms = db (e, n);
    const double stepLen = 60.0 / bpm * kFs / 4.0;
    for (int k = 0; k < bars * 16; ++k)
    {
        const size_t a = (size_t) std::lround (k * stepLen) + (size_t) s.latency;
        const size_t b = std::min (n, (size_t) std::lround ((k + 1) * stepLen) + (size_t) s.latency);
        double se = 0;
        for (size_t i = a; i < b; ++i) se += 0.5 * ((double) s.l[i] * s.l[i] + (double) s.r[i] * s.r[i]);
        f.steps.push_back (b > a ? db (se, b - a) : -200.0);
    }
    for (int band = 0; band < 10; ++band)
    {
        evo2::Biquad bp;
        bp.set (evo2::Biquad::Type::BandPass, kFs, 31.25 * std::pow (2.0, band), 1.41);
        double be = 0;
        for (size_t i = 0; i < n; ++i) { const float y = bp.process (0.5f * (s.l[i] + s.r[i])); be += (double) y * y; }
        f.bands.push_back (db (be, n));
    }
    return f;
}

// Diferencias en dB, ignorando lo que está por debajo de -60 dB en las dos huellas
double worstDiff (const std::vector<double>& a, const std::vector<double>& b)
{
    double worst = 0;
    for (size_t i = 0; i < a.size() && i < b.size(); ++i)
        if (a[i] > -60.0 || b[i] > -60.0) worst = std::max (worst, std::fabs (std::max (a[i], -60.0) - std::max (b[i], -60.0)));
    return worst;
}

void writeJsonArray (FILE* f, const std::vector<double>& v)
{
    std::fputc ('[', f);
    for (size_t i = 0; i < v.size(); ++i) std::fprintf (f, "%s%.1f", i ? "," : "", std::max (v[i], -200.0));
    std::fputc (']', f);
}
} // namespace

int main (int argc, char** argv)
{
    bool exact = false;
    const char* jsonPath = nullptr;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp (argv[i], "--exacto") == 0) exact = true;
        else if (std::strcmp (argv[i], "--normal") == 0) gNormal = true;
        else if (std::strcmp (argv[i], "--huellas") == 0 && i + 1 < argc) jsonPath = argv[++i];
    }
    bool ok = true;
    const size_t numPresets = V1::presets().size();
    if (V2::presets().size() < numPresets) { std::printf ("FALTAN PRESETS DE LA 1.0 EN OGRO 2\n"); return 1; }

    // ---- 1) Mezclas completas ----
    for (size_t pi = 0; pi < numPresets; ++pi)
    {
        const auto a = renderMix<V1> (pi, 16);
        const auto b = renderMix<V2> (pi, 16);
        double ea = 0, ed = 0; float maxDiff = 0;
        const size_t n = std::min (a.l.size(), b.l.size());
        for (size_t i = 0; i < n; ++i)
        {
            const float dl = a.l[i] - b.l[i], dr = a.r[i] - b.r[i];
            ea += (double) a.l[i] * a.l[i] + (double) a.r[i] * a.r[i];
            ed += (double) dl * dl + (double) dr * dr;
            maxDiff = std::max ({ maxDiff, std::fabs (dl), std::fabs (dr) });
        }
        const double residual = ed > 0 ? 10.0 * std::log10 (ed / ea) : -1000.0;
        // Igual = idéntico o con diferencias de redondeo (macros convertidas al recorrido doble)
        const bool same = a.l.size() == b.l.size() && a.latency == b.latency && (maxDiff == 0.0f || residual < -100.0);
        std::printf ("[mezcla %2zu] %-18s diferencia max=%.3g  residuo=%s  latencia %d/%d\n", pi + 1,
                     V1::presets()[pi].name.c_str(), (double) maxDiff,
                     ed > 0 ? (std::to_string ((int) std::lround (residual)) + " dB").c_str() : "nulo (identico)",
                     a.latency, b.latency);
        if (exact) ok &= same;
    }

    // ---- 2) Huellas por pista ----
    FILE* jf = jsonPath ? std::fopen (jsonPath, "w") : nullptr;
    if (jf) std::fprintf (jf, "{\n  \"descripcion\": \"Huellas de OGRO 1.0: pista sola, patron A, 2 compases, sin delay/reverb/textura. dB.\",\n  \"presets\": [\n");
    const char* trackNames[] = { "KICK", "CLAP", "RIM", "HAT C", "HAT A", "PERC", "BAJO", "ACORDES" };
    for (size_t pi = 0; pi < numPresets; ++pi)
    {
        ogro::Engine tmp;
        const double bpm = V1::setup (tmp, pi)[(size_t) V1::tempo()];
        if (jf) std::fprintf (jf, "    { \"nombre\": \"%s\", \"bpm\": %.1f, \"pistas\": [\n", V1::presets()[pi].name.c_str(), bpm);
        double worstStep = 0, worstBand = 0, worstRms = 0;
        for (int t = 0; t < 8; ++t)
        {
            const auto fa = fingerprint (renderStem<V1> (pi, t, 2), bpm, 2);
            const auto fb = fingerprint (renderStem<V2> (pi, t, 2), bpm, 2);
            if (fa.rms > -60.0 || fb.rms > -60.0)
            {
                worstRms = std::max (worstRms, std::fabs (fa.rms - fb.rms));
                worstStep = std::max (worstStep, worstDiff (fa.steps, fb.steps));
                worstBand = std::max (worstBand, worstDiff (fa.bands, fb.bands));
            }
            if (jf)
            {
                std::fprintf (jf, "      { \"pista\": \"%s\", \"rms\": %.1f, \"pasos\": ", trackNames[t], std::max (fa.rms, -200.0));
                writeJsonArray (jf, fa.steps);
                std::fprintf (jf, ", \"bandas\": ");
                writeJsonArray (jf, fa.bands);
                std::fprintf (jf, " }%s\n", t < 7 ? "," : "");
            }
        }
        if (jf) std::fprintf (jf, "    ] }%s\n", pi + 1 < numPresets ? "," : "");
        std::printf ("[huella %2zu] %-18s peor diferencia: nivel %.2f dB, por paso %.2f dB, por banda %.2f dB\n", pi + 1,
                     V1::presets()[pi].name.c_str(), worstRms, worstStep, worstBand);
        // Con azar FIJO cambian las decisiones al azar (qué golpe con probabilidad suena, el S&H):
        // ahí se compara el nivel y el color (bandas), no golpe por golpe
        if (gNormal) ok &= worstRms < 2.0 && worstBand < 3.5;
        else ok &= worstRms < 1.0 && worstStep < 1.5 && worstBand < 1.5;
    }
    if (jf) { std::fprintf (jf, "  ]\n}\n"); std::fclose (jf); }

    std::printf (ok ? "A/B OK\n" : "A/B CON DIFERENCIAS\n");
    return ok ? 0 : 1;
}
