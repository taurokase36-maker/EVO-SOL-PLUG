// OGRO 2 — secuenciador: pasos (hasta 32 por pista) x pistas x 8 patrones, p-locks,
// carriles de automatización dibujables y reloj sincronizado al host.
// Todos los datos son atómicos: la interfaz edita mientras el audio lee.
#pragma once

#include "VoiceMaps.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace ogro2
{
constexpr int kSteps = 32;      // capacidad por pista
constexpr int kPatterns = 8;
constexpr int kLanes = 4;
constexpr int kLanePoints = 64;
constexpr int kMaxRatchet = 8;
constexpr int kMaxLocks = 8;    // p-locks por paso

// Un paso empaquetado en 64 bits. Los 26 bits bajos son idénticos al formato de la 1.0.
struct Step
{
    bool on = false;
    int vel = 100;      // 1..127
    int note = 0;       // -24..+24 semitonos (o grados con MODO TONAL)
    int prob = 100;     // 0..100 %
    int ratchet = 1;    // 1..8
    bool slide = false; // ligado (bajo / acordes)
    int gate = 0;       // 0 = automático; 1..63 = largo en octavos de paso
    int nudge = 0;      // -50..+50 % de un paso (microtiming)
    int cond = 0;       // condición de disparo (0 = siempre)
    int octave = 0;     // -3..+3
    int chordType = 0;  // 0 = el de la pista; 1..31 = tipo + 1

    uint64_t pack() const
    {
        return (uint64_t) (on ? 1 : 0)
             | ((uint64_t) clampv (vel, 0, 127) << 1)
             | ((uint64_t) clampv (note + 64, 0, 127) << 8)
             | ((uint64_t) clampv (prob, 0, 100) << 15)
             | ((uint64_t) (clampv (ratchet, 1, kMaxRatchet) - 1) << 22)
             | ((uint64_t) (slide ? 1 : 0) << 25)
             | ((uint64_t) clampv (gate, 0, 63) << 26)
             | ((uint64_t) (clampv (nudge, -50, 50) & 0x7f) << 32)
             | ((uint64_t) clampv (cond, 0, 31) << 39)
             | ((uint64_t) (clampv (octave, -3, 3) & 0x7) << 44)
             | ((uint64_t) clampv (chordType, 0, 31) << 47);
    }
    static Step unpack (uint64_t v)
    {
        Step s;
        s.on = (v & 1u) != 0;
        s.vel = (int) ((v >> 1) & 127u);
        s.note = (int) ((v >> 8) & 127u) - 64;
        s.prob = std::min (100, (int) ((v >> 15) & 127u));
        s.ratchet = (int) ((v >> 22) & 7u) + 1;
        s.slide = ((v >> 25) & 1u) != 0;
        s.gate = (int) ((v >> 26) & 63u);
        const int n = (int) ((v >> 32) & 0x7fu);
        s.nudge = n >= 64 ? n - 128 : n;
        s.cond = (int) ((v >> 39) & 31u);
        const int o = (int) ((v >> 44) & 7u);
        s.octave = o >= 4 ? o - 8 : o;
        s.chordType = (int) ((v >> 47) & 31u);
        return s;
    }
};

// Condiciones de disparo por paso (estilo Elektron). El índice se guarda: la lista solo crece al final.
enum TrigCond { CondAlways = 0, CondFill, CondNotFill, CondPre, CondNotPre, CondNei, CondNotNei, CondFirst, CondNotFirst,
                CondRatioBase, CondCount = CondRatioBase + 17 };
// A:B (suena en la vuelta A de cada B): 1:2 2:2 1:3 2:3 3:3 1:4..4:4 1:8..8:8
inline bool condRatio (int c, int& a, int& b)
{
    static const int tab[17][2] = { { 1, 2 }, { 2, 2 }, { 1, 3 }, { 2, 3 }, { 3, 3 }, { 1, 4 }, { 2, 4 }, { 3, 4 }, { 4, 4 },
                                    { 1, 8 }, { 2, 8 }, { 3, 8 }, { 4, 8 }, { 5, 8 }, { 6, 8 }, { 7, 8 }, { 8, 8 } };
    const int i = c - CondRatioBase;
    if (i < 0 || i >= 17) return false;
    a = tab[i][0]; b = tab[i][1];
    return true;
}
inline std::string condLabel (int c)
{
    static const char* fixed[] = { "", "FILL", "!FILL", "PRE", "!PRE", "VEC", "!VEC", "1a", "!1a" };
    int a = 0, b = 0;
    if (c >= 0 && c < CondRatioBase) return fixed[c];
    if (condRatio (c, a, b)) return std::to_string (a) + ":" + std::to_string (b);
    return "";
}
inline std::string condName (int c)
{
    static const char* fixed[] = { "Siempre", "Solo en el fill", "Nunca en el fill", "Si el paso anterior con condición sonó",
                                   "Si el paso anterior con condición no sonó", "Si la pista de arriba sonó", "Si la pista de arriba no sonó",
                                   "Solo la primera vuelta", "Todas las vueltas menos la primera" };
    int a = 0, b = 0;
    if (c >= 0 && c < CondRatioBase) return fixed[c];
    if (condRatio (c, a, b)) return "Vuelta " + std::to_string (a) + " de cada " + std::to_string (b);
    return "";
}

// P-lock: valor propio de una perilla en un paso (índice de parámetro + valor normalizado)
struct Lock
{
    int param = -1;      // índice interno de parámetro (-1 = vacío)
    float value = 0.0f;  // 0..1
    uint32_t pack() const { return param < 0 ? 0xFFFFFFFFu : ((uint32_t) param << 16) | (uint32_t) std::lround (clampv (value, 0.0f, 1.0f) * 65535.0f); }
    static Lock unpack (uint32_t v)
    {
        Lock l;
        if (v == 0xFFFFFFFFu) return l;
        l.param = (int) (v >> 16);
        l.value = (float) (v & 0xFFFFu) / 65535.0f;
        return l;
    }
};

class PatternBank
{
public:
    PatternBank() { clearAll(); }

    void clearAll()
    {
        chainLen.store (0);
        for (int p = 0; p < kPatterns; ++p) clearPattern (p);
        for (int l = 0; l < kLanes; ++l)
            for (int i = 0; i < kLanePoints; ++i) lanes[l][i].store (0.5f);
        ++revision;
    }
    void clearPattern (int p)
    {
        patLength[idx (p)].store (16);
        for (int t = 0; t < kTracks; ++t) clearTrack (p, t);
        ++revision;
    }
    void clearTrack (int p, int t)
    {
        length[idx (p)][tr (t)].store (16);
        for (int s = 0; s < kSteps; ++s)
        {
            steps[idx (p)][tr (t)][s].store (Step().pack());
            for (int k = 0; k < kMaxLocks; ++k) locks[idx (p)][tr (t)][s][k].store (0xFFFFFFFFu);
        }
        ++revision;
    }

    Step get (int p, int t, int s) const { return Step::unpack (steps[idx (p)][tr (t)][st (s)].load (std::memory_order_relaxed)); }
    void set (int p, int t, int s, const Step& v) { steps[idx (p)][tr (t)][st (s)].store (v.pack(), std::memory_order_relaxed); ++revision; }
    int getLength (int p, int t) const { return length[idx (p)][tr (t)].load (std::memory_order_relaxed); }
    void setLength (int p, int t, int len) { length[idx (p)][tr (t)].store (clampv (len, 1, kSteps)); ++revision; }
    int getPatternLength (int p) const { return patLength[idx (p)].load (std::memory_order_relaxed); }
    void setPatternLength (int p, int len) { patLength[idx (p)].store (clampv (len, 1, kSteps)); ++revision; }

    // P-locks
    Lock getLock (int p, int t, int s, int k) const { return Lock::unpack (locks[idx (p)][tr (t)][st (s)][clampv (k, 0, kMaxLocks - 1)].load (std::memory_order_relaxed)); }
    int lockCount (int p, int t, int s) const
    {
        int n = 0;
        for (int k = 0; k < kMaxLocks; ++k) n += getLock (p, t, s, k).param >= 0 ? 1 : 0;
        return n;
    }
    // Busca el lock de un parámetro en un paso (-1 si no hay)
    float lockValue (int p, int t, int s, int param) const
    {
        for (int k = 0; k < kMaxLocks; ++k)
        {
            const auto l = getLock (p, t, s, k);
            if (l.param == param) return l.value;
        }
        return -1.0f;
    }
    // Pone o cambia un lock; devuelve false si el paso ya tiene 8
    bool setLock (int p, int t, int s, int param, float value)
    {
        int freeSlot = -1;
        for (int k = 0; k < kMaxLocks; ++k)
        {
            const auto l = getLock (p, t, s, k);
            if (l.param == param) { writeLock (p, t, s, k, { param, value }); return true; }
            if (l.param < 0 && freeSlot < 0) freeSlot = k;
        }
        if (freeSlot < 0) return false;
        writeLock (p, t, s, freeSlot, { param, value });
        return true;
    }
    void clearLock (int p, int t, int s, int param)
    {
        for (int k = 0; k < kMaxLocks; ++k)
            if (getLock (p, t, s, k).param == param) writeLock (p, t, s, k, {});
    }
    void clearLocks (int p, int t, int s) { for (int k = 0; k < kMaxLocks; ++k) writeLock (p, t, s, k, {}); }

    float lane (int l, int i) const { return lanes[clampv (l, 0, kLanes - 1)][clampv (i, 0, kLanePoints - 1)].load (std::memory_order_relaxed); }
    void setLane (int l, int i, float v) { lanes[clampv (l, 0, kLanes - 1)][clampv (i, 0, kLanePoints - 1)].store (clampv (v, 0.0f, 1.0f)); ++revision; }

    void copyTrack (int fromP, int fromT, int toP, int toT)
    {
        setLength (toP, toT, getLength (fromP, fromT));
        for (int s = 0; s < kSteps; ++s)
        {
            set (toP, toT, s, get (fromP, fromT, s));
            for (int k = 0; k < kMaxLocks; ++k) writeLock (toP, toT, s, k, getLock (fromP, fromT, s, k));
        }
    }
    void copyPattern (int from, int to)
    {
        setPatternLength (to, getPatternLength (from));
        for (int t = 0; t < kTracks; ++t) copyTrack (from, t, to, t);
    }

    // ---- Guardado (formato de texto con versión; se ignora lo que no se entiende) ----
    // paramId: índice -> ID de texto, para guardar los p-locks por ID
    std::string serialise (const std::function<std::string (int)>& paramId = {}) const
    {
        std::ostringstream o;
        o << "OGRO2PAT 1\n";
        const uint64_t def = Step().pack();
        char buf[64];
        for (int p = 0; p < kPatterns; ++p)
        {
            o << "L " << p << " " << getPatternLength (p) << "\n";
            for (int t = 0; t < kTracks; ++t)
            {
                o << "T " << p << " " << trackId (t) << " " << getLength (p, t);
                for (int s = 0; s < kSteps; ++s)
                {
                    const uint64_t v = steps[p][t][s].load();
                    if (v == def) continue;
                    std::snprintf (buf, sizeof (buf), " %d=%llx", s, (unsigned long long) v);
                    o << buf;
                }
                o << "\n";
                if (! paramId) continue;
                for (int s = 0; s < kSteps; ++s)
                    for (int k = 0; k < kMaxLocks; ++k)
                    {
                        const auto l = getLock (p, t, s, k);
                        if (l.param < 0) continue;
                        const auto id = paramId (l.param);
                        if (id.empty()) continue;
                        std::snprintf (buf, sizeof (buf), "%.5f", (double) l.value);
                        o << "K " << p << " " << trackId (t) << " " << s << " " << id << " " << buf << "\n";
                    }
            }
        }
        if (chainLength() > 0)
        {
            o << "C";
            for (int i = 0; i < chainLength(); ++i) o << " " << chainPattern (i) << "x" << chainRepeats (i);
            o << "\n";
        }
        for (int l = 0; l < kLanes; ++l)
        {
            o << "M " << l;
            for (int i = 0; i < kLanePoints; ++i) o << (i == 0 ? " " : ",") << (int) std::lround (lane (l, i) * 1000.0f);
            o << "\n";
        }
        return o.str();
    }

    bool deserialise (const std::string& text, const std::function<int (const std::string&)>& paramIndex = {})
    {
        if (text.rfind ("OGRO2PAT", 0) != 0) return deserialiseV1 (text);
        clearAll();
        std::istringstream in (text);
        std::string line;
        std::getline (in, line);
        while (std::getline (in, line))
        {
            std::istringstream ls (line);
            std::string kind;
            ls >> kind;
            if (kind == "L")
            {
                int p = 0, len = 16;
                ls >> p >> len;
                if (p >= 0 && p < kPatterns) setPatternLength (p, len);
            }
            else if (kind == "T")
            {
                int p = 0, len = 16;
                std::string tid;
                ls >> p >> tid >> len;
                const int t = trackIndex (tid);
                if (p < 0 || p >= kPatterns || t < 0) continue;
                setLength (p, t, len);
                std::string tok;
                while (ls >> tok)
                {
                    const auto eq = tok.find ('=');
                    if (eq == std::string::npos) continue;
                    const int s = std::atoi (tok.substr (0, eq).c_str());
                    if (s < 0 || s >= kSteps) continue;
                    steps[p][t][s].store ((uint64_t) std::strtoull (tok.substr (eq + 1).c_str(), nullptr, 16));
                }
            }
            else if (kind == "K" && paramIndex)
            {
                int p = 0, s = 0;
                std::string tid, pid;
                double v = 0.0;
                ls >> p >> tid >> s >> pid >> v;
                const int t = trackIndex (tid), param = paramIndex (pid);
                if (p >= 0 && p < kPatterns && t >= 0 && s >= 0 && s < kSteps && param >= 0) setLock (p, t, s, param, (float) v);
            }
            else if (kind == "C")
            {
                std::vector<std::pair<int, int>> entries;
                std::string tok;
                while (ls >> tok)
                {
                    const auto x = tok.find ('x');
                    if (x == std::string::npos) continue;
                    entries.push_back ({ std::atoi (tok.substr (0, x).c_str()), std::atoi (tok.substr (x + 1).c_str()) });
                }
                setChain (entries);
            }
            else if (kind == "M")
            {
                int l = 0;
                std::string vals;
                ls >> l >> vals;
                if (l < 0 || l >= kLanes) continue;
                std::istringstream vs (vals);
                std::string num;
                for (int i = 0; i < kLanePoints && std::getline (vs, num, ','); ++i) setLane (l, i, (float) std::atoi (num.c_str()) / 1000.0f);
            }
        }
        ++revision;
        return true;
    }

    // Formato de OGRO 1.0: 8 patrones x 8 pistas x 16 pasos de 32 bits + carriles
    bool deserialiseV1 (const std::string& text)
    {
        const auto bar = text.find ('|');
        if (bar == std::string::npos) return false;
        clearAll();
        size_t pos = 0;
        for (int p = 0; p < 8; ++p)
            for (int t = 0; t < 8; ++t)
            {
                const auto colon = text.find (':', pos);
                if (colon == std::string::npos || colon > bar) return false;
                setLength (p, t, (int) std::strtol (text.substr (pos, colon - pos).c_str(), nullptr, 16));
                pos = colon + 1;
                for (int s = 0; s < 16; ++s)
                {
                    const auto comma = text.find (',', pos);
                    if (comma == std::string::npos) return false;
                    steps[p][t][s].store ((uint64_t) (uint32_t) std::strtoul (text.substr (pos, comma - pos).c_str(), nullptr, 16));
                    pos = comma + 1;
                }
                pos = text.find (';', pos) + 1;
            }
        pos = bar + 1;
        for (int l = 0; l < kLanes; ++l)
            for (int i = 0; i < kLanePoints; ++i)
            {
                const auto comma = text.find (',', pos);
                if (comma == std::string::npos) return false;
                setLane (l, i, (float) std::atoi (text.substr (pos, comma - pos).c_str()) / 1000.0f);
                pos = comma + 1;
            }
        ++revision;
        return true;
    }

    static int trackIndex (const std::string& id)
    {
        for (int t = 0; t < kTracks; ++t) if (id == trackId (t)) return t;
        return -1;
    }

    // ---- Cadena de patrones (song mode): hasta 64 entradas de patrón x repeticiones ----
    static constexpr int kChainMax = 64;
    int chainLength() const { return chainLen.load (std::memory_order_relaxed); }
    int chainPattern (int i) const { return chainPat[clampv (i, 0, kChainMax - 1)].load (std::memory_order_relaxed); }
    int chainRepeats (int i) const { return chainRep[clampv (i, 0, kChainMax - 1)].load (std::memory_order_relaxed); }
    void setChain (const std::vector<std::pair<int, int>>& entries)
    {
        const int n = std::min ((int) entries.size(), kChainMax);
        chainLen.store (0);
        for (int i = 0; i < n; ++i)
        {
            chainPat[i].store (clampv (entries[(size_t) i].first, 0, kPatterns - 1));
            chainRep[i].store (clampv (entries[(size_t) i].second, 1, 64));
        }
        chainLen.store (n);
        ++revision;
    }
    std::vector<std::pair<int, int>> getChain() const
    {
        std::vector<std::pair<int, int>> v;
        for (int i = 0; i < chainLength(); ++i) v.push_back ({ chainPattern (i), chainRepeats (i) });
        return v;
    }

    std::atomic<uint32_t> revision { 0 };

private:
    std::atomic<int> chainLen { 0 };
    std::atomic<int> chainPat[kChainMax] {}, chainRep[kChainMax] {};
    void writeLock (int p, int t, int s, int k, const Lock& l)
    {
        locks[idx (p)][tr (t)][st (s)][clampv (k, 0, kMaxLocks - 1)].store (l.pack(), std::memory_order_relaxed);
        ++revision;
    }
    static int idx (int p) { return clampv (p, 0, kPatterns - 1); }
    static int tr (int t) { return clampv (t, 0, kTracks - 1); }
    static int st (int s) { return clampv (s, 0, kSteps - 1); }

    std::atomic<uint64_t> steps[kPatterns][kTracks][kSteps];
    std::atomic<uint32_t> locks[kPatterns][kTracks][kSteps][kMaxLocks];
    std::atomic<int> length[kPatterns][kTracks];
    std::atomic<int> patLength[kPatterns];
    std::atomic<float> lanes[kLanes][kLanePoints];
};

// Información de transporte por bloque
struct Transport
{
    bool playing = false;
    double ppq = 0.0;   // posición en negras al inicio del bloque
    double bpm = 120.0;
    bool looping = false;
    double loopStart = 0.0, loopEnd = 0.0;
};

struct TriggerEvent
{
    int sample = 0;
    int track = 0;
    Hit hit;
    double gateBeats = 0.0; // largo de la nota en negras (0 = sin gate)
    int64_t absStep = 0;    // semicorchea absoluta desde el inicio del tema
    int sub = 0;            // subgolpe del ratchet
    int stepIndex = 0;      // paso dentro de la pista
    int pattern = 0;
    bool release = false;   // fin de nota (MIDI note-off)
    bool locks = false;     // el paso tiene p-locks (se leen al disparar)
    int bfxBar = -1;        // >= 0: arranca la ventana de FX de fin de compás de ese compás
    double bfxBeats = 0.0;  // largo de la ventana en negras
};

// Calcula los disparos que caen dentro de un bloque
class StepClock
{
public:
    enum PatChange { ChangeAtBar = 0, ChangeAtPatternEnd, ChangeImmediate };

    void prepare (double sampleRate) { fs = sampleRate; reset(); }
    void reset()
    {
        lastEnd = -1.0; curPattern = -1; patternStartK = 0; playing = -1;
        for (auto& s : lastStep) s = -1;
        for (auto& d : decisionStep) d = INT64_MIN;
        for (auto& c : lastCond) c = false;
    }

    void setSwing (float s) { swing = clampv (s, 0.5f, 0.75f); }
    void setTrackSwing (int t, bool own, float amt) { trackSwingOwn[clampv (t, 0, kTracks - 1)] = own; trackSwing[clampv (t, 0, kTracks - 1)] = clampv (amt, 0.5f, 0.75f); }
    void setGroove (int type, float amount, float humanizeMs) { groove = type; grooveAmt = clampv (amount, 0.0f, 1.0f); humanize = std::max (0.0f, humanizeMs); }
    void setPatternMode (int change, bool continuousPoly, bool chainOn) { patChange = change; polyContinuous = continuousPoly; chain = chainOn; }
    void setFill (int everyBars, int fillPattern) { fillEvery = everyBars; fillPat = fillPattern; }
    void setBarFx (int everyBars, int lenSteps) { bfxEvery = everyBars; bfxLen = clampv (lenSteps, 1, 4); }
    void requestPattern (int p) { wantedPattern = clampv (p, 0, kPatterns - 1); }
    // Azar: fijo = cada decisión sale de (semilla, pista, paso); vivo = generador con estado (como la 1.0)
    void setRandom (bool fixed, uint32_t s) { fixedRandom = fixed; seed = s; }
    int pattern() const { return playing >= 0 ? playing : (curPattern < 0 ? wantedPattern : curPattern); }
    int stepOf (int track) const { return lastStep[clampv (track, 0, kTracks - 1)]; }
    // true si en el último bloque el transporte arrancó o saltó (no en los saltos del loop)
    bool relocated() const { return jumped; }
    bool fillActive() const { return lastFill; }

    // Llena "events" (ordenados) y devuelve cuántos hay
    int process (const Transport& tr, int numSamples, const PatternBank& bank, uint32_t& rngState,
                 TriggerEvent* events, int maxEvents)
    {
        jumped = false;
        if (! tr.playing || tr.bpm <= 0.0)
        {
            lastEnd = -1.0;
            return 0;
        }
        jumped = lastEnd < 0.0;
        const double ppqPerSample = tr.bpm / 60.0 / fs;
        bpm = tr.bpm;
        const double hostStart = tr.ppq;
        double start = hostStart, end = hostStart + ppqPerSample * numSamples;
        // Continuidad: seguimos desde donde terminó el bloque anterior para no saltear ni
        // repetir pasos. Con tempo fijo se encadena igual que la 1.0; si el tempo cambia y la
        // posición del host se corre más de 2 muestras, el final del bloque se ancla al host.
        if (lastEnd >= 0.0)
        {
            const double drift = std::fabs (hostStart - lastEnd);
            if (drift < ppqPerSample * 2.0) { start = lastEnd; end = start + ppqPerSample * numSamples; }
            else if (drift < 0.02) start = lastEnd;
            else jumped = true;
        }
        if (jumped)
        {
            // Saltó o arrancó: todo se ancla a la posición del tema (sale igual en cada render)
            for (auto& d : decisionStep) d = INT64_MIN;
            for (auto& c : lastCond) c = false;
            patternStartK = 0;
        }
        if (curPattern < 0) { curPattern = wantedPattern; patternStartK = 0; }

        int count = 0;
        // Loop de Ableton: si el fin del loop cae adentro del bloque, partimos la ventana
        if (tr.looping && tr.loopEnd > tr.loopStart + 1e-6 && start < tr.loopEnd && end > tr.loopEnd + ppqPerSample * 0.5)
        {
            const double cut = tr.loopEnd;
            count = window (start, cut, start, ppqPerSample, numSamples, 0, bank, rngState, events, maxEvents, count);
            const int cutSample = clampv ((int) std::ceil ((cut - start) / ppqPerSample), 0, numSamples - 1);
            const double restart = tr.loopStart;
            const double restartEnd = restart + (end - cut);
            count = window (restart, restartEnd, restart - (double) cutSample * ppqPerSample, ppqPerSample, numSamples, cutSample,
                            bank, rngState, events, maxEvents, count);
            lastEnd = restartEnd;
        }
        else
        {
            count = window (start, end, start, ppqPerSample, numSamples, 0, bank, rngState, events, maxEvents, count);
            lastEnd = end;
        }
        // Orden por muestra (inserción: pocos eventos)
        for (int i = 1; i < count; ++i)
        {
            TriggerEvent v = events[i];
            int j = i - 1;
            while (j >= 0 && events[j].sample > v.sample) { events[j + 1] = events[j]; --j; }
            events[j + 1] = v;
        }
        return count;
    }

private:
    // Qué patrón suena en la semicorchea k, desde qué paso empezó y en qué vuelta va
    struct Slot { int pattern; int64_t startK; bool fill; };

    Slot slotFor (int64_t k, const PatternBank& bank) const
    {
        Slot sl { curPattern, patternStartK, false };
        // Cadena: sale solo de la posición en el tema (repetible y apta para loops)
        const int n = bank.chainLength();
        if (chain && n > 0)
        {
            int64_t total = 0;
            for (int i = 0; i < n; ++i) total += (int64_t) bank.getPatternLength (bank.chainPattern (i)) * bank.chainRepeats (i);
            if (total > 0)
            {
                int64_t pos = k % total, acc = 0;
                for (int i = 0; i < n; ++i)
                {
                    const int p = bank.chainPattern (i);
                    const int64_t len = (int64_t) bank.getPatternLength (p) * bank.chainRepeats (i);
                    if (pos < acc + len)
                    {
                        const int64_t into = pos - acc;
                        sl.pattern = p;
                        sl.startK = k - into;
                        break;
                    }
                    acc += len;
                }
            }
        }
        // Fill: el último compás de cada grupo de N compases
        if (fillEvery > 0)
        {
            const int64_t bar = k / 16;
            if (bar % fillEvery == fillEvery - 1)
            {
                sl.fill = true;
                if (fillPat > 0) { sl.pattern = fillPat - 1; sl.startK = bar * 16; }
            }
        }
        return sl;
    }

    // Corrimiento de la plantilla de groove (en fracción de paso) y escala de velocidad
    void grooveFor (int64_t k, double& offset, float& velScale) const
    {
        static const float loose[16] = { 0.0f, 0.06f, -0.03f, 0.09f, 0.0f, 0.04f, -0.05f, 0.07f, -0.02f, 0.05f, -0.04f, 0.08f, 0.01f, 0.03f, -0.06f, 0.10f };
        static const float looseVel[16] = { 1.0f, 0.85f, 0.95f, 0.8f, 1.0f, 0.88f, 0.92f, 0.82f, 0.98f, 0.86f, 0.94f, 0.8f, 1.0f, 0.84f, 0.9f, 0.85f };
        static const float push[16] = { 0.0f, 0.04f, 0.0f, 0.08f, -0.03f, 0.05f, 0.0f, 0.09f, 0.0f, 0.04f, 0.0f, 0.08f, -0.03f, 0.05f, 0.0f, 0.10f };
        static const float mpcVel[16] = { 1.0f, 0.9f, 0.96f, 0.88f, 1.0f, 0.9f, 0.96f, 0.88f, 1.0f, 0.9f, 0.96f, 0.88f, 1.0f, 0.9f, 0.96f, 0.88f };
        const int s16 = (int) (k % 16);
        offset = 0.0; velScale = 1.0f;
        switch (groove)
        {
            case 1: velScale = 1.0f + (mpcVel[s16] - 1.0f) * grooveAmt; break;
            case 3: offset = loose[s16] * grooveAmt; velScale = 1.0f + (looseVel[s16] - 1.0f) * grooveAmt; break;
            case 4: offset = push[s16] * grooveAmt; break;
            default: break;
        }
    }

    // Recorre las semicorcheas de [start, end). "origin" es la posición que corresponde a la muestra 0.
    int window (double start, double end, double origin, double ppqPerSample, int numSamples, int minSample,
                const PatternBank& bank, uint32_t& rngState, TriggerEvent* events, int maxEvents, int count)
    {
        if (end <= start) return count;
        const double stepLen = 0.25;
        // El swing corre los pasos hasta media semicorchea y el nudge ±media: miramos uno antes y uno después
        const int64_t first = (int64_t) std::floor (start / stepLen) - 1;
        const int64_t last = (int64_t) std::floor (end / stepLen) + 1;
        for (int64_t k = first; k <= last; ++k)
        {
            if (k < 0) continue;
            const bool odd = (k & 1) != 0;
            const double kpos = (double) k * stepLen;
            // Cambio de patrón (modo manual)
            if (! chain || bank.chainLength() == 0)
            {
                const double tq = odd ? kpos + (swing - 0.5) * 2.0 * stepLen : kpos;
                if (tq >= start && tq < end && wantedPattern != curPattern)
                {
                    const int len = std::max (1, bank.getPatternLength (curPattern));
                    const bool atBar = k % 16 == 0;
                    const bool atEnd = relative (k, patternStartK) % len == 0;
                    if ((patChange == ChangeAtBar && atBar) || (patChange == ChangeAtPatternEnd && atEnd) || patChange == ChangeImmediate)
                    {
                        curPattern = wantedPattern;
                        if (patChange != ChangeImmediate) patternStartK = k;
                    }
                }
            }
            // Ventana de FX de fin de compás: en los últimos pasos de cada N compases
            if (bfxEvery > 0 && k % 16 == 16 - bfxLen && kpos >= start && kpos < end && count < maxEvents)
            {
                const int64_t bar = k / 16;
                if (bar % bfxEvery == bfxEvery - 1)
                {
                    TriggerEvent& e = events[count++];
                    e = TriggerEvent();
                    e.track = -1;
                    e.sample = clampv ((int) ((kpos - origin) / ppqPerSample), minSample, numSamples - 1);
                    e.bfxBar = (int) bar;
                    e.bfxBeats = bfxLen * stepLen;
                    e.absStep = k;
                }
            }
            const Slot sl = slotFor (k, bank);
            if (kpos >= start && kpos < end) { lastFill = sl.fill; playing = sl.pattern; }
            const int64_t relK = relative (k, sl.startK);
            double gOffset = 0.0; float gVel = 1.0f;
            grooveFor (k, gOffset, gVel);

            for (int t = 0; t < kTracks; ++t)
            {
                const int len = bank.getLength (sl.pattern, t);
                const int64_t pos = polyContinuous ? k : relK;
                const int s = (int) (pos % len);
                const Step st = bank.get (sl.pattern, t, s);
                float sw = trackSwingOwn[t] ? trackSwing[t] : swing;
                if (groove == 2) sw = 0.5f + std::round ((sw - 0.5f) * 24.0f) / 24.0f;   // 909: el swing va en saltos
                const double swingOffset = odd ? (sw - 0.5) * 2.0 * stepLen : 0.0;
                const double t0base = (double) k * stepLen + swingOffset;
                const double dur = odd ? stepLen * (1.0 - (sw - 0.5) * 2.0) : stepLen * (1.0 + (sw - 0.5) * 2.0);
                double t0 = t0base + (double) st.nudge / 100.0 * stepLen;
                if (gOffset != 0.0) t0 += gOffset * stepLen;
                if (humanize > 0.0f)
                    t0 += (double) hashBipolar (hash32 (seed, (uint32_t) t, (uint32_t) k, 0x48554Du)) * humanize * 0.001 * bpm / 60.0;
                if (t0base >= start && t0base < end) lastStep[t] = s;
                if (! st.on) continue;
                for (int r = 0; r < st.ratchet; ++r)
                {
                    const double tr0 = t0 + dur * (double) r / (double) st.ratchet;
                    if (tr0 < start || tr0 >= end) continue;
                    if ((st.prob < 100 || st.cond != CondAlways) && ! passes (t, k, st, sl.fill, (int) (relK / len), rngState)) break;
                    if (count >= maxEvents) break;
                    TriggerEvent& e = events[count++];
                    e = TriggerEvent();
                    e.sample = clampv ((int) ((tr0 - origin) / ppqPerSample), minSample, numSamples - 1);
                    e.track = t;
                    e.hit.vel = (float) st.vel / 127.0f * (r > 0 ? 0.82f : 1.0f);
                    if (gVel != 1.0f) e.hit.vel *= gVel;
                    e.hit.note = (float) st.note;   // semitonos o grado (lo convierte el motor)
                    e.hit.octave = st.octave;
                    e.hit.chordType = st.chordType - 1;
                    e.hit.slide = st.slide && r == 0;
                    e.hit.accent = st.vel >= 118;
                    e.absStep = k;
                    e.sub = r;
                    e.stepIndex = s;
                    e.pattern = sl.pattern;
                    e.locks = bank.lockCount (sl.pattern, t, s) > 0;
                    if (fixedRandom) { e.hit.fixedSeed = true; e.hit.seed = hash32 (seed, (uint32_t) t, (uint32_t) k, (uint32_t) r + 77u); }
                    // Gate: largo del paso (o el que se eligió) y, si el paso siguiente está ligado, hasta él
                    const double subDur = dur / (double) st.ratchet;
                    double gate = st.gate > 0 ? stepLen * (double) st.gate / 8.0 : subDur;
                    if (r == st.ratchet - 1)
                    {
                        const Step next = bank.get (sl.pattern, t, (int) ((pos + 1) % len));
                        if (next.on && next.slide) gate = std::max (gate, dur - (tr0 - t0) + 0.02);
                    }
                    e.gateBeats = gate;
                }
            }
        }
        return count;
    }

    // Posición relativa al inicio del patrón (si el loop volvió antes de ese inicio, la del tema)
    static int64_t relative (int64_t k, int64_t startK) { return k >= startK ? k - startK : k; }

    bool evalCond (int t, const Step& st, bool fill, int iteration) const
    {
        int a = 0, b = 0;
        switch (st.cond)
        {
            case CondAlways: return true;
            case CondFill: return fill;
            case CondNotFill: return ! fill;
            case CondPre: return lastCond[t];
            case CondNotPre: return ! lastCond[t];
            case CondNei: return t > 0 && lastCond[t - 1];
            case CondNotNei: return ! (t > 0 && lastCond[t - 1]);
            case CondFirst: return iteration == 0;
            case CondNotFirst: return iteration > 0;
            default: if (condRatio (st.cond, a, b)) return iteration % b == a - 1;
        }
        return true;
    }

    // Una sola decisión por cada vez que pasa el paso: vale para todos sus subgolpes
    bool passes (int t, int64_t k, const Step& st, bool fill, int iteration, uint32_t& rngState)
    {
        if (decisionStep[t] == k) return decisionPass[t];
        bool pass = true;
        if (st.cond != CondAlways)
        {
            pass = evalCond (t, st, fill, iteration);
            if (st.cond != CondPre && st.cond != CondNotPre && st.cond != CondNei && st.cond != CondNotNei) lastCond[t] = pass;
        }
        if (pass && st.prob < 100)
        {
            if (fixedRandom) pass = (int) (hash32 (seed, (uint32_t) t, (uint32_t) k, 0x50524F42u) % 100u) < st.prob;
            else
            {
                rngState ^= rngState << 13; rngState ^= rngState >> 17; rngState ^= rngState << 5;
                pass = (int) (rngState % 100u) < st.prob;
            }
            if (st.cond == CondAlways) lastCond[t] = pass;
        }
        decisionStep[t] = k;
        decisionPass[t] = pass;
        return pass;
    }

    double fs = 44100.0, lastEnd = -1.0, bpm = 120.0;
    bool jumped = false, lastFill = false;
    float swing = 0.5f;
    bool trackSwingOwn[kTracks] {};
    float trackSwing[kTracks] { 0.54f, 0.54f, 0.54f, 0.54f, 0.54f, 0.54f, 0.54f, 0.54f, 0.54f, 0.54f };
    int groove = 0;
    float grooveAmt = 1.0f, humanize = 0.0f;
    int patChange = ChangeAtBar;
    bool polyContinuous = false, chain = false;
    int fillEvery = 0, fillPat = 0, bfxEvery = 0, bfxLen = 2;
    bool fixedRandom = true;
    uint32_t seed = 1;
    int wantedPattern = 0, curPattern = -1, playing = -1;
    int64_t patternStartK = 0;
    int lastStep[kTracks] { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    int64_t decisionStep[kTracks] {};
    bool decisionPass[kTracks] {};
    bool lastCond[kTracks] {};
};

} // namespace ogro2
