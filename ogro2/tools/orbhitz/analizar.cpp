// OGRO 2 — analiza una carpeta de samples .mse (Orbhitz) y escribe el manifiesto de afinación
// (ogro2_samples.json) en la misma carpeta. Es lo mismo que hace el plugin la primera vez que
// abre la carpeta; sirve para revisar el resultado en una tabla antes de corregirlo a mano.
// Uso: clang++ -std=c++17 -O2 analizar.cpp -o analizar && ./analizar "~/Music/OGRO Samples/Orbhitz"
#include "../../Source/engine/SamplePool.h"
#include <chrono>
#include <cstdio>
#include <thread>

int main (int argc, char** argv)
{
    using namespace ogro2;
    if (argc < 2) { std::printf ("Uso: analizar <carpeta con .mse>\n"); return 1; }
    auto& pool = SamplePool::get();
    const int found = pool.scan (argv[1]);
    std::printf ("%d samples (CHD, PAD, HIT, SFX). Analizando...\n", found);
    while (pool.busy()) std::this_thread::sleep_for (std::chrono::milliseconds (50));
    pool.saveManifest();
    static const char* pcs[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < pool.size(); ++i)
    {
        const auto* e = pool.entry (i);
        std::printf ("%-16s %s  raiz %-3s%d (archivo %3d)  %-5s  %s  confianza %.2f\n", e->name.c_str(), categoryId (e->category),
                     pcs[e->rootMidi % 12], e->rootMidi / 12 - 1, e->fileRoot, analysisQualities()[(size_t) e->quality].c_str(),
                     e->tonal ? "en tono " : "sin altura", (double) e->confidence);
    }
    std::printf ("Manifiesto: %s\n", SamplePool::manifestPath (argv[1]).c_str());
    return 0;
}
