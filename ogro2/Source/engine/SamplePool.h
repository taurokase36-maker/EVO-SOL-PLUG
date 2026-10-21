// OGRO 2 — pool de samples compartido entre todas las instancias del plugin.
//  - Escanea una carpeta (.mse de Orbhitz: CHD, PAD, HIT y SFX; nada de BAS ni VOX).
//  - Carga el audio (int16 en memoria) y lo analiza en un hilo de fondo.
//  - El análisis se guarda en un manifiesto JSON en la misma carpeta, editable a mano.
//  - Las entradas no se borran nunca mientras viva el proceso: el audio puede leerlas sin
//    locks ni riesgo (cada una tiene "ready" atómico).
#pragma once

#include "Mse.h"
#include "SampleAnalysis.h"
#include "VoiceMaps.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <dirent.h>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>

namespace ogro2
{
enum SampleCategory { CatChd = 0, CatPad, CatHit, CatSfx, CatCount };
inline const char* categoryId (int c)
{
    static const char* n[] = { "CHD", "PAD", "HIT", "SFX" };
    return n[clampv (c, 0, CatCount - 1)];
}
inline int categoryOf (const std::string& name)
{
    for (int c = 0; c < CatCount; ++c)
        if (name.rfind (std::string (categoryId (c)) + "-", 0) == 0) return c;
    return -1;
}

struct SampleEntry
{
    std::string name, path;
    int category = CatChd;
    // Afinación (del manifiesto o del análisis)
    int rootMidi = 60;
    int quality = 0;
    bool tonal = false;
    float confidence = 0.0f;
    int fileRoot = 60;
    bool known = false;         // ya tiene análisis (manifiesto o calculado)
    // Audio (lo llena el hilo de carga; se lee solo con ready == true)
    int channels = 2, frames = 0;
    double sampleRate = 44100.0;
    std::vector<int16_t> pcm;
    std::atomic<bool> ready { false };
    std::atomic<bool> failed { false };
    std::string error;
};

class SamplePool
{
public:
    static constexpr int kCapacity = 4096;
    static SamplePool& get()
    {
        static SamplePool pool;
        return pool;
    }
    ~SamplePool()
    {
        {
            std::lock_guard<std::mutex> lk (m);
            quit = true;
        }
        cv.notify_all();
        if (worker.joinable()) worker.join();
    }

    // Hilo de mensajes: escanea la carpeta, agrega lo nuevo y arranca la carga en el fondo.
    // Devuelve cuántos samples válidos tiene la carpeta.
    int scan (const std::string& dir)
    {
        std::lock_guard<std::mutex> scanLock (scanMutex);
        folder = dir;
        readManifest (dir);
        int found = 0;
        if (DIR* d = opendir (dir.c_str()))
        {
            std::vector<std::string> names;
            while (dirent* e = readdir (d))
            {
                const std::string n = e->d_name;
                if (n.size() < 5 || n.substr (n.size() - 4) != ".mse") continue;
                if (categoryOf (n) < 0) continue;   // BAS, VOX y otros: no
                names.push_back (n);
            }
            closedir (d);
            std::sort (names.begin(), names.end());
            for (const auto& n : names)
            {
                ++found;
                const std::string name = n.substr (0, n.size() - 4);
                if (find (name) >= 0) continue;
                const int c = count.load();
                if (c >= kCapacity) break;
                auto e = std::make_unique<SampleEntry>();
                e->name = name;
                e->path = dir + "/" + n;
                e->category = categoryOf (n);
                applyManifest (*e);
                entries[(size_t) c] = std::move (e);
                count.store (c + 1, std::memory_order_release);
                enqueue (c);
            }
        }
        lastFound = found;
        return found;
    }

    int size() const { return count.load (std::memory_order_acquire); }
    const SampleEntry* entry (int i) const { return i >= 0 && i < size() ? entries[(size_t) i].get() : nullptr; }
    int find (const std::string& name) const
    {
        const int n = size();
        for (int i = 0; i < n; ++i) if (entries[(size_t) i]->name == name) return i;
        return -1;
    }
    int loadedCount() const
    {
        int c = 0;
        for (int i = 0; i < size(); ++i) c += entries[(size_t) i]->ready.load() ? 1 : 0;
        return c;
    }
    bool busy() const { return pending.load() > 0; }
    std::string currentFolder() const { std::lock_guard<std::mutex> lk (scanMutex); return folder; }
    int samplesInFolder() const { return lastFound; }
    // Cambia la afinación a mano (también se guarda en el manifiesto)
    void setTuning (int i, int rootMidi, bool tonal)
    {
        if (i < 0 || i >= size()) return;
        auto& e = *entries[(size_t) i];
        e.rootMidi = rootMidi; e.tonal = tonal; e.known = true;
        saveManifest();
    }

    // ---- Manifiesto JSON ----
    static std::string manifestPath (const std::string& dir) { return dir + "/ogro2_samples.json"; }
    void saveManifest()
    {
        std::string dir;
        { std::lock_guard<std::mutex> lk (scanMutex); dir = folder; }
        if (dir.empty()) return;
        static const char* pcs[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        std::ostringstream o;
        o << "{\n  \"version\": 1,\n";
        o << "  \"ayuda\": \"Analisis de afinacion de OGRO 2. Podes corregir raiz (nota MIDI, 60 = C4), calidad y tonal (false = no se transpone). OGRO 2 respeta lo que pongas.\",\n";
        o << "  \"samples\": [\n";
        const int n = size();
        bool first = true;
        for (int i = 0; i < n; ++i)
        {
            const auto& e = *entries[(size_t) i];
            if (! e.known || e.path.rfind (dir + "/", 0) != 0) continue;
            char buf[64];
            std::snprintf (buf, sizeof (buf), "%.2f", (double) e.confidence);
            o << (first ? "" : ",\n") << "    { \"nombre\": \"" << e.name << "\", \"categoria\": \"" << categoryId (e.category)
              << "\", \"raiz\": " << e.rootMidi << ", \"nota\": \"" << pcs[((e.rootMidi % 12) + 12) % 12] << (e.rootMidi / 12 - 1)
              << "\", \"calidad\": \"" << analysisQualities()[(size_t) clampv (e.quality, 0, (int) analysisQualities().size() - 1)]
              << "\", \"tonal\": " << (e.tonal ? "true" : "false") << ", \"confianza\": " << buf << ", \"raiz_archivo\": " << e.fileRoot << " }";
            first = false;
        }
        o << "\n  ]\n}\n";
        if (FILE* f = std::fopen (manifestPath (dir).c_str(), "wb"))
        {
            const auto s = o.str();
            std::fwrite (s.data(), 1, s.size(), f);
            std::fclose (f);
        }
    }

private:
    SamplePool() : entries ((size_t) kCapacity) { worker = std::thread ([this] { run(); }); }

    struct ManifestRow { int root = 60, quality = 0, fileRoot = 60; bool tonal = false; float conf = 0.0f; };
    static std::string field (const std::string& obj, const std::string& key)
    {
        const auto k = obj.find ("\"" + key + "\"");
        if (k == std::string::npos) return {};
        auto c = obj.find (':', k);
        if (c == std::string::npos) return {};
        ++c;
        while (c < obj.size() && (obj[c] == ' ' || obj[c] == '\t' || obj[c] == '\n' || obj[c] == '\r')) ++c;
        if (c < obj.size() && obj[c] == '"')
        {
            const auto e = obj.find ('"', c + 1);
            return e == std::string::npos ? std::string() : obj.substr (c + 1, e - c - 1);
        }
        auto e = c;
        while (e < obj.size() && obj[e] != ',' && obj[e] != '}' && obj[e] != '\n') ++e;
        auto v = obj.substr (c, e - c);
        while (! v.empty() && (v.back() == ' ' || v.back() == '\r' || v.back() == '\t')) v.pop_back();
        return v;
    }
    void readManifest (const std::string& dir)
    {
        manifest.clear();
        FILE* f = std::fopen (manifestPath (dir).c_str(), "rb");
        if (f == nullptr) return;
        std::string text;
        char buf[8192];
        size_t n;
        while ((n = std::fread (buf, 1, sizeof (buf), f)) > 0) text.append (buf, n);
        std::fclose (f);
        size_t pos = text.find ("\"samples\"");
        while (pos != std::string::npos)
        {
            const auto a = text.find ('{', pos);
            if (a == std::string::npos) break;
            const auto b = text.find ('}', a);
            if (b == std::string::npos) break;
            const auto obj = text.substr (a, b - a + 1);
            const auto name = field (obj, "nombre");
            if (! name.empty())
            {
                ManifestRow r;
                r.root = clampv (std::atoi (field (obj, "raiz").c_str()), 0, 127);
                r.quality = qualityIndex (field (obj, "calidad"));
                r.tonal = field (obj, "tonal") == "true";
                r.conf = (float) std::atof (field (obj, "confianza").c_str());
                const auto fr = field (obj, "raiz_archivo");
                r.fileRoot = fr.empty() ? 60 : std::atoi (fr.c_str());
                manifest.push_back ({ name, r });
            }
            pos = b + 1;
        }
    }
    void applyManifest (SampleEntry& e)
    {
        for (const auto& [name, r] : manifest)
            if (name == e.name)
            {
                e.rootMidi = r.root; e.quality = r.quality; e.tonal = r.tonal; e.confidence = r.conf; e.fileRoot = r.fileRoot;
                e.known = true;
                return;
            }
    }
    void enqueue (int i)
    {
        {
            std::lock_guard<std::mutex> lk (m);
            queue.push_back (i);
            pending.fetch_add (1);
        }
        cv.notify_one();
    }
    void run()
    {
        bool analysedSomething = false;
        for (;;)
        {
            int i = -1;
            {
                std::unique_lock<std::mutex> lk (m);
                cv.wait (lk, [this] { return quit || ! queue.empty(); });
                if (quit) return;
                i = queue.front();
                queue.pop_front();
            }
            auto& e = *entries[(size_t) i];
            MseSample s;
            std::string err;
            if (readMse (e.path, s, err))
            {
                e.channels = s.channels;
                e.frames = s.frames;
                e.sampleRate = (double) s.sampleRate;
                e.pcm = std::move (s.pcm);
                if (! e.known)
                {
                    e.fileRoot = s.fileRoot;
                    std::vector<float> mono ((size_t) e.frames);
                    for (int k = 0; k < e.frames; ++k)
                    {
                        float acc = 0.0f;
                        for (int c = 0; c < e.channels; ++c) acc += e.pcm[(size_t) (k * e.channels + c)];
                        mono[(size_t) k] = acc / (float) e.channels / 32768.0f;
                    }
                    const auto a = analyseSample (mono, e.sampleRate, s.fileRoot);
                    e.rootMidi = a.rootMidi;
                    e.quality = a.quality;
                    e.confidence = a.confidence;
                    // Los acordes siempre se transponen; los SFX solo si la altura es muy clara
                    e.tonal = e.category == CatChd ? true : (e.category == CatSfx ? a.tonal && a.confidence > 0.5f : a.tonal);
                    e.known = true;
                    analysedSomething = true;
                }
                e.ready.store (true, std::memory_order_release);
            }
            else
            {
                e.error = err;
                e.failed.store (true);
            }
            if (pending.fetch_sub (1) == 1 && analysedSomething)
            {
                saveManifest();
                analysedSomething = false;
            }
        }
    }

    std::vector<std::unique_ptr<SampleEntry>> entries;   // capacidad fija: nunca se mueve
    std::atomic<int> count { 0 }, pending { 0 };
    std::vector<std::pair<std::string, ManifestRow>> manifest;
    mutable std::mutex scanMutex;
    std::string folder;
    int lastFound = 0;
    std::mutex m;
    std::condition_variable cv;
    std::deque<int> queue;
    bool quit = false;
    std::thread worker;
};

} // namespace ogro2
