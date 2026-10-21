// OGRO 2 — tests de la fase 4 (samples): lector .mse, análisis de afinación, pool compartido con
// manifiesto editable, reproductor (afinación en LIVE y ALTA, antialias, reversa, inicio y largo),
// pista TEXTURAS en tono, capa de samples de ACORDES y azar por paso. Usa archivos sintéticos:
// la librería Orbhitz no está en el repo.
#include "../Source/engine/Engine.h"
#include <chrono>
#include <cstdio>
#include <sys/stat.h>
#include <thread>

using namespace ogro2;

namespace
{
constexpr double kFs = 44100.0;
constexpr int64_t kBar = 88200;

bool report (const char* name, bool ok, const std::string& detail)
{
    std::printf ("[%s] %s  %s\n", name, ok ? "OK   " : "FALLA", detail.c_str());
    return ok;
}
std::vector<int16_t> tone (std::vector<int> notes, double sr, double secs, bool saw, int channels = 2)
{
    const size_t n = (size_t) (sr * secs);
    std::vector<int16_t> pcm (n * (size_t) channels);
    for (size_t i = 0; i < n; ++i)
    {
        double v = 0.0;
        for (int m : notes)
        {
            const double f = 440.0 * std::pow (2.0, (m - 69) / 12.0);
            const double ph = std::fmod (f * (double) i / sr, 1.0);
            v += saw ? (2.0 * ph - 1.0) : std::sin (2.0 * 3.14159265358979 * ph);
        }
        v *= 0.25 * std::exp (-(double) i / sr * 0.8);
        for (int c = 0; c < channels; ++c) pcm[i * (size_t) channels + (size_t) c] = (int16_t) std::lround (std::clamp (v, -1.0, 1.0) * 32000.0);
    }
    return pcm;
}
void writeFile (const std::string& path, const std::vector<uint8_t>& d)
{
    if (FILE* f = std::fopen (path.c_str(), "wb")) { std::fwrite (d.data(), 1, d.size(), f); std::fclose (f); }
}
double pitchOf (const std::vector<float>& x, size_t from, size_t len)
{
    const int minLag = (int) (kFs / 2000.0), maxLag = (int) (kFs / 50.0);
    std::vector<double> c ((size_t) maxLag + 2, 0.0);
    double maxC = 0.0;
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        double s = 0, e1 = 0, e2 = 0;
        for (size_t i = from; i < from + len; ++i) { s += x[i] * x[i + (size_t) lag]; e1 += x[i] * x[i]; e2 += x[i + (size_t) lag] * x[i + (size_t) lag]; }
        c[(size_t) lag] = s / std::sqrt (e1 * e2 + 1e-20);
        maxC = std::max (maxC, c[(size_t) lag]);
    }
    int best = minLag;
    for (int lag = minLag + 1; lag < maxLag; ++lag)
        if (c[(size_t) lag] > 0.9 * maxC && c[(size_t) lag] >= c[(size_t) lag - 1] && c[(size_t) lag] >= c[(size_t) lag + 1]) { best = lag; break; }
    const double a = c[(size_t) best - 1], b = c[(size_t) best], d = c[(size_t) best + 1];
    return kFs / ((double) best + 0.5 * (a - d) / (a - 2 * b + d + 1e-20));
}
double rms (const std::vector<float>& x, size_t a, size_t b)
{
    double e = 0;
    for (size_t i = a; i < b && i < x.size(); ++i) e += (double) x[i] * x[i];
    return std::sqrt (e / (double) std::max<size_t> (1, b - a));
}
std::vector<float> defaults()
{
    const auto& d = paramDefs();
    std::vector<float> v (d.size());
    for (size_t i = 0; i < d.size(); ++i) v[i] = d[i].def;
    v[P::Swing] = 0.5f;
    v[P::MComp] = 0; v[P::MWarmth] = 0; v[P::BusComp] = 0; v[P::BusCrudo] = 0; v[P::MTexture] = 0; v[P::MCeiling] = 0;
    v[P::DlyRet] = 0; v[P::RevRet] = 0;
    for (int t = 0; t < kTracks; ++t) { v[(size_t) P::track (t, P::Delay)] = 0; v[(size_t) P::track (t, P::Reverb)] = 0; v[(size_t) P::track (t, P::Pump)] = 0; }
    v[P::MAir] = 0; v[P::MPres] = 0;
    return v;
}
std::vector<float> render (Engine& eng, const std::vector<float>& v, int64_t total, bool offline = false)
{
    eng.prepare (kFs, 512);
    eng.setOffline (offline);
    std::vector<float> l ((size_t) total), r ((size_t) total);
    for (int64_t pos = 0; pos < total; pos += 512)
    {
        const int n = (int) std::min<int64_t> (512, total - pos);
        HostInfo h; h.valid = true; h.playing = true; h.bpm = 120.0; h.ppq = (double) pos * 2.0 / kFs;
        eng.setParams (v.data(), (int) v.size());
        eng.process (h, nullptr, 0, l.data() + pos, r.data() + pos, n);
    }
    return l;
}
} // namespace

int main()
{
    bool all = true;
    char buf[300];

    // ---- 1) .mse: escribir y leer ----
    {
        const auto pcm = tone ({ 57, 60, 64, 67 }, 48000.0, 0.5, true);
        const auto bytes = makeMse ("CHD-Prueba", 2, 48000, pcm, 57);
        MseSample s;
        std::string err;
        const bool ok = parseMse (bytes, "CHD-Prueba", s, err) && s.channels == 2 && s.sampleRate == 48000 && s.frames == 24000 && s.fileRoot == 57
                        && s.pcm == pcm;
        MseSample bad;
        const bool rejects = ! parseMse (std::vector<uint8_t> (100, 7), "x", bad, err);
        all &= report ("mse", ok && rejects, "48 kHz estereo de ida y vuelta; un archivo roto se rechaza (\"" + err + "\")");
    }

    // ---- 2) Análisis de afinación ----
    {
        struct C { const char* name; std::vector<int> notes; const char* q; int root; };
        const C cases[] = { { "Am7", { 57, 60, 64, 67 }, "m7", 57 }, { "C mayor", { 48, 52, 55, 60 }, "M", 48 }, { "Dm9", { 50, 53, 57, 60, 64 }, "m9", 50 },
                            { "Sol suelto", { 55 }, "nota", 55 }, { "Fsus2", { 53, 55, 60 }, "sus2", 53 } };
        bool ok = true;
        std::string d;
        for (const auto& c : cases)
        {
            const auto pcm = tone (c.notes, 44100.0, 1.2, true, 1);
            std::vector<float> mono (pcm.size());
            for (size_t i = 0; i < pcm.size(); ++i) mono[i] = pcm[i] / 32768.0f;
            const auto a = analyseSample (mono, 44100.0);
            const bool good = a.tonal && a.rootMidi == c.root && analysisQualities()[(size_t) a.quality] == c.q;
            ok &= good;
            d += std::string (c.name) + (good ? " ok " : " MAL ");
        }
        std::vector<float> noise (60000);
        uint32_t s = 7;
        for (auto& x : noise) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; x = (float) (int32_t) s / 2147483648.0f * 0.3f; }
        ok &= ! analyseSample (noise, 44100.0).tonal;
        all &= report ("analisis", ok, d + "· ruido: sin altura");
    }

    // ---- 3) Pool: carpeta, carga en el fondo, manifiesto editable ----
    const std::string dir = "out/samples_prueba";
    mkdir ("out", 0755);
    mkdir (dir.c_str(), 0755);
    std::remove ((dir + "/ogro2_samples.json").c_str());
    writeFile (dir + "/CHD-AmSiete.mse", makeMse ("CHD-AmSiete", 2, 44100, tone ({ 57, 60, 64, 67 }, 44100.0, 1.0, true), 60));
    writeFile (dir + "/PAD-DoSeno.mse", makeMse ("PAD-DoSeno", 2, 48000, tone ({ 60 }, 48000.0, 2.0, false), 60));
    writeFile (dir + "/HIT-Agudo.mse", makeMse ("HIT-Agudo", 1, 44100, tone ({ 126 }, 44100.0, 1.0, false, 1), 60));   // ~11.8 kHz
    writeFile (dir + "/BAS-NoVa.mse", makeMse ("BAS-NoVa", 2, 44100, tone ({ 36 }, 44100.0, 0.5, true), 36));
    auto& pool = SamplePool::get();
    const int found = pool.scan (dir);
    for (int i = 0; i < 200 && pool.busy(); ++i) std::this_thread::sleep_for (std::chrono::milliseconds (20));
    const int am = pool.find ("CHD-AmSiete"), doS = pool.find ("PAD-DoSeno"), hi = pool.find ("HIT-Agudo");
    {
        bool ok = found == 3 && pool.find ("BAS-NoVa") < 0 && am >= 0 && doS >= 0 && hi >= 0 && pool.loadedCount() >= 3;
        ok &= pool.entry (am)->rootMidi == 57 && analysisQualities()[(size_t) pool.entry (am)->quality] == "m7" && pool.entry (am)->tonal;
        ok &= pool.entry (doS)->rootMidi == 60 && pool.entry (doS)->tonal;
        FILE* f = std::fopen ((dir + "/ogro2_samples.json").c_str(), "rb");
        std::string json;
        if (f) { char b[4096]; size_t n; while ((n = std::fread (b, 1, sizeof (b), f)) > 0) json.append (b, n); std::fclose (f); }
        ok &= json.find ("\"CHD-AmSiete\"") != std::string::npos && json.find ("\"calidad\": \"m7\"") != std::string::npos;
        std::snprintf (buf, sizeof (buf), "%d samples (sin BAS), analizados en el fondo; manifiesto con %zu bytes", found, json.size());
        all &= report ("pool", ok, buf);
        // A mano: el usuario corrige la raíz del pad a 62 y lo marca atonal; un pool nuevo lo respeta
        const auto at = json.find ("\"PAD-DoSeno\"");
        std::string edited = json;
        if (at != std::string::npos)
        {
            const auto r = edited.find ("\"raiz\": 60", at);
            if (r != std::string::npos) edited.replace (r, 10, "\"raiz\": 62");
            const auto t = edited.find ("\"tonal\": true", at);
            if (t != std::string::npos) edited.replace (t, 13, "\"tonal\": false");
        }
        writeFile (dir + "/ogro2_samples.json", std::vector<uint8_t> (edited.begin(), edited.end()));
    }

    // ---- 4) TEXTURAS en tono: un Do (C4) en La menor, grado 0 -> La (A3 = 220 Hz), LIVE y ALTA ----
    for (int q : { 0, 1 })
    {
        Engine eng;
        eng.bank.clearAll();
        Step s; s.on = true; s.vel = 110;
        eng.bank.set (0, Texture, 0, s);
        eng.setSelectedSample (Engine::SlotTexture, doS);
        auto v = defaults(); v[P::Root] = 9; v[P::Scale] = 0; v[P::Quality] = (float) q;
        const auto out = render (eng, v, kBar / 2);
        const double f = pitchOf (out, 4000, 4096);
        const double cents = 1200.0 * std::log2 (f / 220.0);
        std::snprintf (buf, sizeof (buf), "%s: Do de 48 kHz a La 220 Hz -> %.2f Hz (%+.1f ct)", q == 0 ? "LIVE (Hermite)" : "ALTA (sinc)", f, cents);
        all &= report ("texturas", std::fabs (cents) < 5.0, buf);
    }
    {
        // Antialias: un tono de 11.8 kHz una octava arriba (23.6 kHz) no tiene que volver como alias en ALTA
        double r[2];
        for (int q : { 0, 1 })
        {
            Engine eng;
            eng.bank.clearAll();
            Step s; s.on = true; s.vel = 110;
            eng.bank.set (0, Texture, 0, s);
            eng.setSelectedSample (Engine::SlotTexture, hi);
            auto v = defaults(); v[P::Quality] = (float) q; v[P::TexFollow] = 0;
            v[(size_t) P::sound (Texture, 2)] = 0.75f;   // +12 st
            r[q] = rms (render (eng, v, kBar / 4), 2000, 8000);
        }
        std::snprintf (buf, sizeof (buf), "+12 st sobre 11.8 kHz: LIVE %.4f, ALTA %.4f (alias %.0f dB menos)", r[0], r[1], 20.0 * std::log10 (r[0] / (r[1] + 1e-12)));
        all &= report ("antialias", r[1] < r[0] * 0.35, buf);
    }
    {
        // Reversa, inicio y largo
        Engine a, b;
        for (auto* e : { &a, &b }) { e->bank.clearAll(); Step s; s.on = true; s.vel = 110; e->bank.set (0, Texture, 0, s); e->setSelectedSample (Engine::SlotTexture, doS); }
        auto v = defaults(); v[P::TexFollow] = 0;
        v[(size_t) P::sound (Texture, 1)] = 0.25f;   // LARGO 25 %
        const auto fwd = render (a, v, kBar);
        auto vr = v; vr[P::TexReverse] = 1;
        const auto rev = render (b, vr, kBar);
        const size_t len = (size_t) (0.25 * 2.0 * 48000.0 * kFs / 48000.0);
        const double tailF = rms (fwd, len + 2000, len + 6000), tailR = rms (rev, len + 2000, len + 6000);
        const double headF = rms (fwd, 200, 2000), headR = rms (rev, 200, 2000);
        std::snprintf (buf, sizeof (buf), "LARGO 25 %%: despues del tramo %.4f / %.4f; reversa arranca mas bajo (%.3f vs %.3f)", tailF, tailR, headR, headF);
        all &= report ("edicion", tailF < 1e-4 && tailR < 1e-4 && headR < headF, buf);
    }
    {
        // Sample que no está: silencio y sin errores
        Engine eng;
        eng.bank.clearAll();
        Step s; s.on = true; eng.bank.set (0, Texture, 0, s);
        eng.setSelectedSample (Engine::SlotTexture, -1);
        const auto out = render (eng, defaults(), kBar / 4);
        all &= report ("sin sample", rms (out, 0, out.size()) == 0.0, "pista TEXTURAS sin sample: silencio");
    }

    // ---- 5) Manifiesto editado a mano: el pad queda atonal (no se transpone) ----
    {
        // El pool ya tiene las entradas cargadas: la edición se lee al volver a escanear otra carpeta con el mismo contenido
        const std::string dir2 = "out/samples_prueba2";
        mkdir (dir2.c_str(), 0755);
        writeFile (dir2 + "/PAD-DoSenoB.mse", makeMse ("PAD-DoSenoB", 2, 48000, tone ({ 60 }, 48000.0, 2.0, false), 60));
        FILE* f = std::fopen ((dir2 + "/ogro2_samples.json").c_str(), "wb");
        if (f) { std::fputs ("{ \"samples\": [ { \"nombre\": \"PAD-DoSenoB\", \"categoria\": \"PAD\", \"raiz\": 62, \"calidad\": \"nota\", \"tonal\": false } ] }", f); std::fclose (f); }
        pool.scan (dir2);
        for (int i = 0; i < 200 && pool.busy(); ++i) std::this_thread::sleep_for (std::chrono::milliseconds (20));
        const int idx = pool.find ("PAD-DoSenoB");
        bool ok = idx >= 0 && pool.entry (idx)->rootMidi == 62 && ! pool.entry (idx)->tonal;
        Engine eng;
        eng.bank.clearAll();
        Step s; s.on = true; s.vel = 110; s.note = 2;
        eng.bank.set (0, Texture, 0, s);
        eng.setSelectedSample (Engine::SlotTexture, idx);
        auto v = defaults(); v[P::Root] = 9;
        const double fr = pitchOf (render (eng, v, kBar / 2), 4000, 4096);
        ok &= std::fabs (1200.0 * std::log2 (fr / 261.63)) < 5.0;
        std::snprintf (buf, sizeof (buf), "raiz 62 y tonal false a mano: se respeta y no se transpone (%.1f Hz)", fr);
        all &= report ("manifiesto", ok, buf);
    }

    // ---- 6) ACORDES con sample: el Am7 en el grado III (Do) de La menor suena con raíz Do ----
    {
        Engine eng;
        eng.bank.clearAll();
        Step s; s.on = true; s.vel = 110; s.note = 2; s.gate = 32;
        eng.bank.set (0, Chord, 0, s);
        eng.setSelectedSample (Engine::SlotChord, am);
        auto v = defaults(); v[P::Root] = 9; v[P::ChordSource] = 1; v[P::ChordMode] = 0;
        v[(size_t) P::sound (Chord, 2)] = 1.0f;   // filtro abierto
        v[(size_t) P::track (Chord, P::EnvS)] = 1.0f;
        const auto out = render (eng, v, kBar / 2);
        const auto a = analyseSample (std::vector<float> (out.begin() + 2000, out.end()), kFs);
        static const char* pcs[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        std::snprintf (buf, sizeof (buf), "sample Am7 en el grado III: suena %s%s", pcs[a.rootMidi % 12], analysisQualities()[(size_t) a.quality].c_str());
        all &= report ("acordes sample", a.rootMidi % 12 == 0 && rms (out, 0, out.size()) > 0.01, buf);
    }

    // ---- 7) Azar por paso (FIJO): repetible y usa más de un sample ----
    {
        std::vector<float> outs[2];
        for (int k = 0; k < 2; ++k)
        {
            Engine eng;
            eng.bank.clearAll();
            for (int st = 0; st < 16; ++st) { Step s; s.on = true; s.vel = 100; eng.bank.set (0, Texture, st, s); }
            eng.setCandidates (Engine::SlotTexture, { am, doS, hi });
            auto v = defaults(); v[P::TexRandStep] = 1; v[(size_t) P::sound (Texture, 1)] = 0.05f;
            outs[k] = render (eng, v, kBar);
        }
        double maxd = 0.0;
        for (size_t i = 0; i < outs[0].size(); ++i) maxd = std::max (maxd, (double) std::fabs (outs[0][i] - outs[1][i]));
        // Distintos golpes, distintos samples: los niveles por golpe no son todos iguales
        std::vector<double> levels;
        for (int st = 0; st < 16; ++st) levels.push_back (rms (outs[0], (size_t) (st * kBar / 16) + 200, (size_t) (st * kBar / 16) + 1800));
        std::sort (levels.begin(), levels.end());
        const bool varied = levels.back() > levels.front() * 1.3;
        std::snprintf (buf, sizeof (buf), "dos renders iguales (dif %.2g) y golpes con samples distintos (niveles %.3f a %.3f)", maxd, levels.front(), levels.back());
        all &= report ("azar por paso", maxd == 0.0 && varied, buf);
    }

    std::printf (all ? "SAMPLES OK\n" : "SAMPLES CON FALLAS\n");
    return all ? 0 : 1;
}
