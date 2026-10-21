// OGRO 2 — lector de los archivos .mse de Orbhitz (C++ puro, sin JUCE).
//
// Formato (todos los enteros little-endian):
//   "MSE " + metadatos + la interfaz del plugin original (PNG + XML con imágenes) que se ignora
//   después de "</player>": "<nombre>.wav\0" + 6 x u32 (canales, sampleRate, bytes por muestra,
//   0x7000, numFrames, 0) + PCM int16 intercalado + u32 nota raíz, tecla baja, tecla alta,
//   velocidad máxima, puntos de loop/fin y dos floats.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace ogro2
{
struct MseSample
{
    std::string name;            // nombre del archivo sin extensión ("CHD-Classics")
    int channels = 2;
    int sampleRate = 44100;
    int frames = 0;
    int fileRoot = 60;           // nota raíz que dice el archivo (no siempre es confiable)
    std::vector<int16_t> pcm;    // intercalado
};

namespace mse
{
inline uint32_t u32 (const std::vector<uint8_t>& d, size_t at)
{
    return (uint32_t) d[at] | ((uint32_t) d[at + 1] << 8) | ((uint32_t) d[at + 2] << 16) | ((uint32_t) d[at + 3] << 24);
}
inline size_t find (const std::vector<uint8_t>& d, const char* what, size_t from)
{
    const size_t n = std::strlen (what);
    if (d.size() < n) return std::string::npos;
    for (size_t i = from; i + n <= d.size(); ++i)
        if (d[i] == (uint8_t) what[0] && std::memcmp (&d[i], what, n) == 0) return i;
    return std::string::npos;
}
inline std::string baseName (const std::string& path)
{
    const auto slash = path.find_last_of ("/\\");
    std::string b = slash == std::string::npos ? path : path.substr (slash + 1);
    const auto dot = b.find_last_of ('.');
    return dot == std::string::npos ? b : b.substr (0, dot);
}
} // namespace mse

// Lee un .mse desde memoria. Devuelve false (con el motivo en "error") si no lo entiende.
inline bool parseMse (const std::vector<uint8_t>& d, const std::string& name, MseSample& out, std::string& error)
{
    using namespace mse;
    if (d.size() < 64 || std::memcmp (d.data(), "MSE ", 4) != 0) { error = "no es un archivo MSE"; return false; }
    const size_t player = find (d, "</player>", 4);
    if (player == std::string::npos) { error = "no tiene la sección del sample"; return false; }
    const size_t wav = find (d, ".wav", player);
    if (wav == std::string::npos || wav + 5 + 24 > d.size() || d[wav + 4] != 0) { error = "no tiene la zona del sample"; return false; }
    const size_t h = wav + 5;
    const uint32_t ch = u32 (d, h), sr = u32 (d, h + 4), bps = u32 (d, h + 8), frames = u32 (d, h + 16);
    if (ch < 1 || ch > 2 || bps != 2 || sr < 8000 || sr > 192000 || frames == 0) { error = "formato de audio no soportado"; return false; }
    const size_t pcmAt = h + 24, bytes = (size_t) frames * ch * 2;
    if (pcmAt + bytes > d.size()) { error = "el archivo está cortado"; return false; }
    out.name = name;
    out.channels = (int) ch;
    out.sampleRate = (int) sr;
    out.frames = (int) frames;
    out.pcm.resize ((size_t) frames * ch);
    for (size_t i = 0; i < out.pcm.size(); ++i)
        out.pcm[i] = (int16_t) ((uint16_t) d[pcmAt + 2 * i] | ((uint16_t) d[pcmAt + 2 * i + 1] << 8));
    out.fileRoot = pcmAt + bytes + 4 <= d.size() ? (int) u32 (d, pcmAt + bytes) : 60;
    if (out.fileRoot < 0 || out.fileRoot > 127) out.fileRoot = 60;
    return true;
}

inline bool readMse (const std::string& path, MseSample& out, std::string& error)
{
    FILE* f = std::fopen (path.c_str(), "rb");
    if (f == nullptr) { error = "no se puede abrir"; return false; }
    std::vector<uint8_t> d;
    std::fseek (f, 0, SEEK_END);
    const long size = std::ftell (f);
    std::fseek (f, 0, SEEK_SET);
    if (size > 0 && size < (200L << 20))
    {
        d.resize ((size_t) size);
        if (std::fread (d.data(), 1, d.size(), f) != d.size()) d.clear();
    }
    std::fclose (f);
    if (d.empty()) { error = "archivo vacío o ilegible"; return false; }
    return parseMse (d, mse::baseName (path), out, error);
}

// Escribe un .mse mínimo (para los tests: el plugin solo lee lo que está después de </player>)
inline std::vector<uint8_t> makeMse (const std::string& name, int channels, int sampleRate, const std::vector<int16_t>& pcm, int root)
{
    std::vector<uint8_t> d;
    auto put = [&d] (const void* p, size_t n) { const auto* b = (const uint8_t*) p; d.insert (d.end(), b, b + n); };
    auto put32 = [&put] (uint32_t v) { uint8_t b[4] = { (uint8_t) v, (uint8_t) (v >> 8), (uint8_t) (v >> 16), (uint8_t) (v >> 24) }; put (b, 4); };
    put ("MSE ", 4);
    for (int i = 0; i < 60; ++i) d.push_back ((uint8_t) i);
    const std::string xml = "<?xml version=\"1.0\"?><player w=\"1\"><img data=\"AAAA\"/></player>";
    put (xml.data(), xml.size());
    const std::string wav = name + ".wav";
    put (wav.c_str(), wav.size() + 1);
    const uint32_t frames = (uint32_t) (pcm.size() / (size_t) channels);
    put32 ((uint32_t) channels); put32 ((uint32_t) sampleRate); put32 (2); put32 (0x7000); put32 (frames); put32 (0);
    for (int16_t s : pcm) { const uint16_t u = (uint16_t) s; uint8_t b[2] = { (uint8_t) u, (uint8_t) (u >> 8) }; put (b, 2); }
    put32 ((uint32_t) root); put32 (24); put32 (120); put32 (127); put32 (frames - 1); put32 (frames - 1);
    const float f1 = 100.0f, f2 = 1000.0f;
    put (&f1, 4); put (&f2, 4);
    return d;
}

} // namespace ogro2
