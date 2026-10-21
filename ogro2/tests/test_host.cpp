// OGRO 2 — host simulado: reproduce cómo Ableton le pasa el transporte al plugin
// (posición al inicio de cada bloque, loop que salta a mitad de bloque, tempo
// automatizado, buffers de 32 a 2048 y de tamaño variable, 44.1/48/96 kHz) y
// comprueba que cada paso suene una sola vez y a tiempo.
#include "../Source/engine/Engine.h"
#include "../Source/engine/Presets.h"
#include <cstdio>
#include <functional>
#include <random>

using namespace ogro2;

namespace
{
std::vector<float> defaults()
{
    const auto& d = paramDefs();
    std::vector<float> v (d.size());
    for (size_t i = 0; i < d.size(); ++i) v[i] = d[i].def;
    v[P::Swing] = 0.5f;
    v[P::MComp] = 0; v[P::MWarmth] = 0; v[P::BusComp] = 0; v[P::BusCrudo] = 0; v[P::MTexture] = 0;
    v[P::DlyRet] = 0; v[P::RevRet] = 0;
    for (int m = 0; m < 4; ++m) v[(size_t) (P::Macro1 + m)] = 0.5f;
    return v;
}

// Línea de tiempo del host: tempo (puede variar por muestra) y loop opcional.
// La posición "real" de cada muestra salta exactamente en el fin del loop,
// aunque caiga a mitad de bloque (como el audio de Ableton).
struct Timeline
{
    double fs = 44100.0;
    std::function<double (int64_t)> bpmAt = [] (int64_t) { return 120.0; };
    bool looping = false;
    double loopStart = 0.0, loopEnd = 4.0;
    double startPpq = 0.0;

    std::vector<double> positions (int64_t total) const
    {
        std::vector<double> ppq ((size_t) total);
        double p = startPpq;
        for (int64_t i = 0; i < total; ++i)
        {
            ppq[(size_t) i] = p;
            p += bpmAt (i) / 60.0 / fs;
            if (looping && p >= loopEnd) p -= loopEnd - loopStart;
        }
        return ppq;
    }
};

struct Run
{
    std::vector<float> out;
    std::vector<int64_t> triggers[kTracks]; // muestra exacta de cada disparo (registro del motor)
    int latency = 0;
};

// Procesa con el tamaño de bloque que devuelva blockSize(i) (fijo o variable)
Run run (Engine& eng, std::vector<float> v, const Timeline& tl, int64_t total, const std::function<int (int)>& blockSize,
         int maxBlock, const std::function<void (int64_t, std::vector<float>&)>& automation = {}, bool prepare = true)
{
    if (prepare) eng.prepare (tl.fs, maxBlock);
    const auto ppq = tl.positions (total);
    Run r;
    r.out.assign ((size_t) total, 0.0f);
    std::vector<float> right ((size_t) total);
    int blockIndex = 0;
    for (int64_t pos = 0; pos < total;)
    {
        const int n = (int) std::min<int64_t> (blockSize (blockIndex++), total - pos);
        if (automation) automation (pos, v);
        HostInfo h;
        h.valid = true; h.playing = true; h.ppq = ppq[(size_t) pos]; h.bpm = tl.bpmAt (pos);
        h.looping = tl.looping; h.loopStart = tl.loopStart; h.loopEnd = tl.loopEnd;
        eng.setParams (v.data(), (int) v.size());
        eng.process (h, nullptr, 0, r.out.data() + pos, right.data() + pos, n);
        pos += n;
    }
    const uint32_t count = eng.triggerLog.count.load();
    for (uint32_t k = count > (uint32_t) TriggerLog::kSize ? count - (uint32_t) TriggerLog::kSize : 0; k < count; ++k)
    {
        const auto& e = eng.triggerLog.entries[k % TriggerLog::kSize];
        r.triggers[e.track].push_back (e.sample);
    }
    r.latency = eng.getLatencySamples();
    return r;
}

// Pasos que tendrían que sonar: cada vez que la posición real cruza una semicorchea
// (sin swing). Devuelve la muestra exacta de cada cruce y el número absoluto de paso.
struct Expected { int64_t sample; int64_t step; };
std::vector<Expected> expectedSteps (const Timeline& tl, int64_t total)
{
    const auto ppq = tl.positions (total);
    const double eps = 1e-9;
    auto stepOf = [eps] (double p) { return (int64_t) std::floor (p * 4.0 + eps); };
    auto onGrid = [eps] (double p) { return std::fabs (p * 4.0 - std::round (p * 4.0)) < eps; };
    std::vector<Expected> e;
    for (int64_t i = 0; i < total; ++i)
    {
        const double cur = ppq[(size_t) i];
        if (i == 0 || cur < ppq[(size_t) i - 1]) // arranque o salto del loop
        {
            if (onGrid (cur)) e.push_back ({ i, stepOf (cur) });
            continue;
        }
        for (int64_t k = stepOf (ppq[(size_t) i - 1]) + 1; k <= stepOf (cur); ++k) e.push_back ({ i, k });
    }
    return e;
}

// Rim muy corto: sirve para medir cada golpe con precisión de muestra
std::vector<float> rimOnly()
{
    auto v = defaults();
    for (int t = 0; t < kTracks; ++t) v[(size_t) P::track (t, P::Mute)] = t == Rim ? 0.0f : 1.0f;
    v[(size_t) P::sound (Rim, 1)] = 0.0f;  // decay mínimo
    v[(size_t) P::sound (Rim, 7)] = 0.0f;  // sin VARIA
    return v;
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
    char buf[256];

    // ---- 1) Buffers fijos de 32 a 2048 y variables, a 44.1 / 48 / 96 kHz: cada paso una vez y a tiempo ----
    for (double fs : { 44100.0, 48000.0, 96000.0 })
        for (int block : { 32, 64, 128, 256, 441, 512, 1024, 2048, -1 })
        {
            Engine eng;
            eng.bank.clearAll();
            Step s; s.on = true; s.vel = 110;
            for (int k = 0; k < kSteps; k += 3) eng.bank.set (0, Rim, k, s);
            Timeline tl; tl.fs = fs;
            const int64_t total = (int64_t) (fs * 4.0); // 2 compases a 120
            std::mt19937 rng (7);
            auto bs = [block, &rng] (int) { return block > 0 ? block : (int) (rng() % 2048) + 1; };
            const auto r = run (eng, rimOnly(), tl, total, bs, 2048);
            std::vector<int64_t> on;
            for (auto smp : r.triggers[Rim]) if (smp < total - 2) on.push_back (smp);
            std::vector<int64_t> want;
            for (const auto& e : expectedSteps (tl, total))
                if ((e.step % 16) % 3 == 0 && e.sample < total - 2) want.push_back (e.sample);
            double worst = 0;
            const bool sameCount = on.size() == want.size();
            for (size_t i = 0; i < std::min (on.size(), want.size()); ++i) worst = std::max (worst, (double) std::llabs (on[i] - want[i]));
            std::snprintf (buf, sizeof (buf), "%5.1f kHz, buffer %-8s golpes %zu/%zu, error max %.0f muestras",
                           fs / 1000.0, block > 0 ? std::to_string (block).c_str() : "variable", on.size(), want.size(), worst);
            all &= report ("buffers", sameCount && worst <= 1.0, buf);
        }

    // ---- 2) Loop de Ableton (salta a mitad de bloque) con una pista de 13 pasos ----
    //      Al volver al inicio del loop tiene que sonar exactamente lo mismo que la primera vuelta.
    for (double loopStart : { 0.0, 8.0 })
        for (int block : { 256, 512, 1024 })
        {
            Engine eng;
            eng.bank.clearAll();
            Step s; s.on = true; s.vel = 110;
            eng.bank.setLength (0, Rim, 13);
            eng.bank.set (0, Rim, 0, s);
            Timeline tl;
            tl.looping = true; tl.loopStart = loopStart; tl.loopEnd = loopStart + 4.0; tl.startPpq = loopStart;
            const int64_t total = (int64_t) (tl.fs * 2.0 * 4.0); // 4 vueltas de 1 compás a 120
            const auto r = run (eng, rimOnly(), tl, total, [block] (int) { return block; }, block);
            int64_t want = 0, got = 0;
            for (const auto& e : expectedSteps (tl, total)) want += (e.step % 13) == 0 && e.sample < total - 2 ? 1 : 0;
            for (auto smp : r.triggers[Rim]) got += smp < total - 2 ? 1 : 0;
            std::snprintf (buf, sizeof (buf), "loop de 1 compas desde la negra %.0f, buffer %4d: disparos %lld, esperados %lld",
                           loopStart, block, (long long) got, (long long) want);
            all &= report ("loop", got == want, buf);
        }

    // ---- 3) Tempo automatizado: rampa 120 -> 140 BPM y saltos bruscos, buffers grandes ----
    for (int kind = 0; kind < 2; ++kind)
        for (int block : { 512, 2048 })
        {
            Engine eng;
            eng.bank.clearAll();
            Step s; s.on = true; s.vel = 110;
            for (int k = 0; k < kSteps; ++k) eng.bank.set (0, Rim, k, s);
            Timeline tl;
            const int64_t total = (int64_t) (tl.fs * 16.0);
            if (kind == 0) tl.bpmAt = [total] (int64_t i) { return 120.0 + 20.0 * (double) i / (double) total; };
            else tl.bpmAt = [] (int64_t i) { return (i / 30000) % 2 == 0 ? 120.0 : 127.0; };
            const auto r = run (eng, rimOnly(), tl, total, [block] (int) { return block; }, block);
            size_t want = 0, got = 0;
            for (const auto& e : expectedSteps (tl, total)) want += e.sample < total - 2 ? 1 : 0;
            for (auto smp : r.triggers[Rim]) got += smp < total - 2 ? 1 : 0;
            std::snprintf (buf, sizeof (buf), "%s, buffer %4d: disparos %zu, esperados %zu",
                           kind == 0 ? "rampa 120->140" : "saltos 120<->127", block, got, want);
            all &= report ("tempo", got == want, buf);
        }

    // ---- 4) Arranque a mitad de compás: el primer golpe es el del próximo paso ----
    {
        Engine eng;
        eng.bank.clearAll();
        Step s; s.on = true; s.vel = 110;
        for (int k = 0; k < kSteps; ++k) eng.bank.set (0, Rim, k, s);
        Timeline tl; tl.startPpq = 5.37;
        const int64_t total = (int64_t) (tl.fs * 1.0);
        const auto r = run (eng, rimOnly(), tl, total, [] (int) { return 512; }, 512);
        const auto e = expectedSteps (tl, total);
        const auto& on = r.triggers[Rim];
        const bool ok = ! e.empty() && ! on.empty() && e.front().step == 22 && std::llabs (on.front() - e.front().sample) <= 1;
        std::snprintf (buf, sizeof (buf), "arranca en la negra 5.37: primer paso esperado %lld en la muestra %lld, sono en %lld",
                       e.empty() ? -1LL : (long long) e.front().step, e.empty() ? -1LL : (long long) e.front().sample,
                       on.empty() ? -1LL : (long long) on.front());
        all &= report ("arranque", ok, buf);
    }

    // ---- 5) Cambio de patrón a mitad de compás: entra en el próximo compás ----
    {
        Engine eng;
        eng.bank.clearAll();
        Step s; s.on = true; s.vel = 110;
        eng.bank.set (0, Rim, 0, s);                       // A: solo el paso 1
        for (int k = 0; k < kSteps; ++k) eng.bank.set (1, Rim, k, s); // B: los 16
        Timeline tl;
        const int64_t total = (int64_t) (tl.fs * 6.0);      // 3 compases a 120
        const int64_t switchAt = (int64_t) (tl.fs * 2.0 * 1.4); // a mitad del compás 2
        const auto r = run (eng, rimOnly(), tl, total, [] (int) { return 512; }, 512,
                            [switchAt] (int64_t pos, std::vector<float>& v) { v[P::Pattern] = pos >= switchAt ? 1.0f : 0.0f; });
        // Esperado: compás 1 = 1 golpe, compás 2 = 1 golpe (sigue A), compás 3 = 16 golpes (B)
        int perBar[3] {};
        for (auto smp : r.triggers[Rim])
            if (smp + 2 < total) perBar[std::min<int64_t> (2, (smp + 2) / (int64_t) (tl.fs * 2.0))]++;
        std::snprintf (buf, sizeof (buf), "golpes por compas %d / %d / %d (esperado 1 / 1 / 16)", perBar[0], perBar[1], perBar[2]);
        all &= report ("patron", perBar[0] == 1 && perBar[1] == 1 && perBar[2] == 16, buf);
    }

    // ---- 6) Probabilidad 0 % con ratchet: no tiene que sonar ningún subgolpe ----
    for (int block : { 32, 256, 2048 })
    {
        Engine eng;
        eng.bank.clearAll();
        Step s; s.on = true; s.vel = 110; s.prob = 0; s.ratchet = 4;
        for (int k = 0; k < kSteps; k += 4) eng.bank.set (0, Rim, k, s);
        Timeline tl;
        const auto r = run (eng, rimOnly(), tl, (int64_t) (tl.fs * 4.0), [block] (int) { return block; }, block);
        std::snprintf (buf, sizeof (buf), "prob 0 %%, ratchet 4, buffer %4d: sonaron %zu subgolpes (esperado 0)", block, r.triggers[Rim].size());
        all &= report ("prob+ratchet", r.triggers[Rim].empty(), buf);
    }

    // ---- 7) Azar reproducible (FIJO): dos renders desde el mismo lugar salen iguales, aunque
    //      antes haya sonado otra cosa (con efectos, compresores y textura incluidos) ----
    {
        const auto& preset = factoryPresets()[2]; // Medianoche Minima: probabilidades, S&H, azar por paso y ring
        const auto st = factoryState (preset);
        auto values = st.values;
        values[P::MTexture] = 0.3f;
        Timeline tl; tl.fs = 44100.0;
        const double bpm = 124.0;
        tl.bpmAt = [bpm] (int64_t) { return bpm; };
        const int64_t bar = (int64_t) (4.0 * 60.0 / bpm * tl.fs);
        Timeline from3 = tl; from3.startPpq = 8.0;
        // a) arranque limpio en el compás 3
        Engine a; preset.build (a.bank); applyRouting (a.routing, st);
        const auto ra = run (a, values, from3, 2 * bar, [] (int) { return 512; }, 512);
        // b) suena otra parte del tema, se para el transporte y se arranca en el compás 3
        Engine b; preset.build (b.bank); applyRouting (b.routing, st);
        Timeline warm = tl; warm.startPpq = 64.0;
        run (b, values, warm, 3 * bar, [] (int) { return 512; }, 512);
        std::vector<float> l (512), r (512);
        HostInfo stopped; stopped.valid = true; stopped.playing = false; stopped.bpm = bpm;
        for (int i = 0; i < 8; ++i) { b.setParams (values.data(), (int) values.size()); b.process (stopped, nullptr, 0, l.data(), r.data(), 512); }
        const auto rb = run (b, values, from3, 2 * bar, [] (int) { return 512; }, 512, {}, false);
        double ea = 0, ed = 0;
        for (int64_t i = 0; i < 2 * bar; ++i)
        {
            const float x = ra.out[(size_t) i], y = rb.out[(size_t) i];
            ea += (double) x * x; ed += (double) (x - y) * (x - y);
        }
        const double res = ed > 0 ? 10.0 * std::log10 (ed / ea) : -1000.0;
        std::snprintf (buf, sizeof (buf), "compases 3 y 4 desde cero y despues de tocar otra parte: diferencia %s",
                       ed > 0 ? (std::to_string ((int) std::lround (res)) + " dB").c_str() : "nula");
        all &= report ("azar", res < -120.0, buf);
    }

    // ---- 8) Bajo con sustain: la nota dura lo que el paso (gate) y después suelta ----
    {
        auto measure = [] (float sustain) {
            Engine eng;
            eng.bank.clearAll();
            Step s; s.on = true; s.vel = 100; s.gate = 32; // 4 pasos de largo
            eng.bank.set (0, Bass, 0, s);
            auto v = defaults();
            for (int t = 0; t < kTracks; ++t) v[(size_t) P::track (t, P::Mute)] = t == Bass ? 0.0f : 1.0f;
            v[(size_t) P::track (Bass, P::EnvS)] = sustain;
            v[(size_t) P::track (Bass, P::EnvR)] = 30.0f;
            v[(size_t) P::sound (Bass, 1)] = 0.2f; // decay corto
            Timeline tl;
            const auto r = run (eng, v, tl, (int64_t) (tl.fs * 2.0), [] (int) { return 512; }, 512);
            // energía entre el paso 2 y el 3 (dentro del gate) y entre el paso 6 y el 7 (después del release)
            auto rms = [&r] (double a, double b) {
                double e = 0; const int64_t i0 = (int64_t) (a * 5512.5) + r.latency, i1 = (int64_t) (b * 5512.5) + r.latency;
                for (int64_t i = i0; i < i1; ++i) e += (double) r.out[(size_t) i] * r.out[(size_t) i];
                return 10.0 * std::log10 (e / (double) (i1 - i0) + 1e-20);
            };
            return std::make_pair (rms (2.0, 3.0), rms (6.0, 7.0));
        };
        const auto off = measure (0.0f), on = measure (0.8f);
        std::snprintf (buf, sizeof (buf), "sin sustain: %.0f dB en el paso 3; con sustain: %.0f dB en el paso 3 y %.0f dB después del gate",
                       off.first, on.first, on.second);
        all &= report ("gate", on.first > off.first + 10.0 && on.second < on.first - 30.0, buf);
    }

    std::printf (all ? "HOST OK\n" : "HOST CON FALLAS\n");
    return all ? 0 : 1;
}
