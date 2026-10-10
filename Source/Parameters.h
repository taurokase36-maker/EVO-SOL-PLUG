// EVO SOL — definición de parámetros (automatizables desde Ableton)
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/VocalEngine.h"

namespace evo::params
{
// IDs estables (no cambiar: las sesiones guardadas dependen de ellos)
namespace id
{
    inline constexpr const char* inGain = "inGain";
    inline constexpr const char* outGain = "outGain";
    inline constexpr const char* ceiling = "ceiling";
    inline constexpr const char* riderOn = "riderOn";
    inline constexpr const char* riderTarget = "riderTarget";
    inline constexpr const char* riderRange = "riderRange";
    inline constexpr const char* smooth = "smooth";
    inline constexpr const char* deEss = "deEss";
    inline constexpr const char* tuneOn = "tuneOn";
    inline constexpr const char* key = "key";
    inline constexpr const char* scale = "scale";
    inline constexpr const char* retune = "retune";
    inline constexpr const char* humanize = "humanize";
    inline constexpr const char* flex = "flex";
    inline constexpr const char* vibrato = "vibrato";
    inline constexpr const char* throat = "throat";
    inline constexpr const char* transpose = "transpose";
    inline constexpr const char* detune = "detune";
    inline constexpr const char* tracking = "tracking";
    inline constexpr const char* range = "range";
    inline constexpr const char* harm1 = "harm1";
    inline constexpr const char* harm2 = "harm2";
    inline constexpr const char* harmLevel = "harmLevel";
    inline constexpr const char* coreOn = "coreOn";
    inline constexpr const char* hpf = "hpf";
    inline constexpr const char* body = "body";
    inline constexpr const char* presence = "presence";
    inline constexpr const char* air = "air";
    inline constexpr const char* comp = "comp";
    inline constexpr const char* color = "color";
    inline constexpr const char* warmth = "warmth";
    inline constexpr const char* parallel = "parallel";
    inline constexpr const char* doubler = "doubler";
    inline constexpr const char* delayMix = "delayMix";
    inline constexpr const char* delayDiv = "delayDiv";
    inline constexpr const char* delayFb = "delayFb";
    inline constexpr const char* delayTone = "delayTone";
    inline constexpr const char* reverbMix = "reverbMix";
    inline constexpr const char* reverbSize = "reverbSize";
    inline constexpr const char* reverbDecay = "reverbDecay";
    inline constexpr const char* reverbPre = "reverbPre";
    inline constexpr const char* duck = "duck";
    inline constexpr const char* radio = "radio";
    inline constexpr const char* width = "width";
}

inline const juce::StringArray& delayDivisionNames()
{
    static const juce::StringArray names { "1/2", "1/4", "1/4 puntillo", "1/8", "1/8 puntillo", "1/8 tresillo", "1/16" };
    return names;
}
inline float delayDivisionBeats (int idx)
{
    static const float beats[] = { 2.0f, 1.0f, 1.5f, 0.5f, 0.75f, 1.0f / 3.0f, 0.25f };
    return beats[juce::jlimit (0, 6, idx)];
}

inline juce::StringArray toStringArray (const char* const* names, int count)
{
    juce::StringArray a;
    for (int i = 0; i < count; ++i) a.add (juce::String::fromUTF8 (names[i]));
    return a;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    using APF = AudioParameterFloat;
    using APB = AudioParameterBool;
    using APC = AudioParameterChoice;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto pct = [] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + " %"; };
    auto db1 = [] (float v, int) { return String (v, 1) + " dB"; };
    auto bip = [] (float v, int) { return (v > 0 ? "+" : "") + String (juce::roundToInt (v * 100.0f)) + " %"; };

    auto F = [&] (const char* pid, const char* name, NormalisableRange<float> r, float def,
                  std::function<String (float, int)> toText)
    {
        p.push_back (std::make_unique<APF> (ParameterID { pid, 1 }, name, r, def,
                                            AudioParameterFloatAttributes().withStringFromValueFunction (std::move (toText))));
    };

    // Entrada / salida
    F (id::inGain, "Entrada", { -24.0f, 24.0f, 0.1f }, 0.0f, db1);
    F (id::outGain, "Salida", { -24.0f, 12.0f, 0.1f }, 0.0f, db1);
    F (id::ceiling, "Techo", { -6.0f, 0.0f, 0.1f }, -1.0f, db1);

    // Inteligencia
    p.push_back (std::make_unique<APB> (ParameterID { id::riderOn, 1 }, "Auto-ganancia", true));
    F (id::riderTarget, "Nivel objetivo", { -30.0f, -10.0f, 0.1f }, -20.0f, db1);
    F (id::riderRange, "Rango auto-ganancia", { 0.0f, 18.0f, 0.1f }, 9.0f, db1);
    F (id::smooth, "Suavizado IA", { 0.0f, 1.0f, 0.001f }, 0.35f, pct);
    F (id::deEss, "De-esser IA", { 0.0f, 1.0f, 0.001f }, 0.4f, pct);

    // Afinación
    p.push_back (std::make_unique<APB> (ParameterID { id::tuneOn, 1 }, "Afinacion", true));
    p.push_back (std::make_unique<APC> (ParameterID { id::key, 1 }, "Key", toStringArray (noteNames(), 12), 0));
    p.push_back (std::make_unique<APC> (ParameterID { id::scale, 1 }, "Escala",
                                        toStringArray (scaleNames(), (int) ScaleType::NumScales), 0));
    {
        NormalisableRange<float> r { 0.0f, 400.0f, 0.1f };
        r.setSkewForCentre (40.0f);
        F (id::retune, "Velocidad", r, 20.0f, [] (float v, int) { return v < 0.5f ? String ("Robot") : String (v, 0) + " ms"; });
    }
    F (id::humanize, "Humanizar", { 0.0f, 1.0f, 0.001f }, 0.0f, pct);
    F (id::flex, "Flex-Tune", { 0.0f, 1.0f, 0.001f }, 0.0f, pct);
    F (id::vibrato, "Vibrato natural", { -1.0f, 1.0f, 0.001f }, 0.0f, bip);
    F (id::throat, "Garganta", { -4.0f, 4.0f, 0.01f }, 0.0f, [] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " st"; });
    F (id::transpose, "Transponer", { -12.0f, 12.0f, 1.0f }, 0.0f, [] (float v, int) { return (v > 0 ? "+" : "") + String (roundToInt (v)) + " st"; });
    F (id::detune, "Referencia", { -100.0f, 100.0f, 0.1f }, 0.0f,
       [] (float v, int) { return String (440.0 * std::pow (2.0, v / 1200.0), 1) + " Hz"; });
    F (id::tracking, "Tracking", { 0.0f, 1.0f, 0.001f }, 0.5f, pct);
    p.push_back (std::make_unique<APC> (ParameterID { id::range, 1 }, "Voz", toStringArray (voiceRangeNames(), 3), 1));

    // Armonías
    p.push_back (std::make_unique<APC> (ParameterID { id::harm1, 1 }, "Armonia 1",
                                        toStringArray (harmonyNames(), (int) HarmonyInterval::Num), 0));
    p.push_back (std::make_unique<APC> (ParameterID { id::harm2, 1 }, "Armonia 2",
                                        toStringArray (harmonyNames(), (int) HarmonyInterval::Num), 0));
    F (id::harmLevel, "Nivel armonias", { 0.0f, 1.0f, 0.001f }, 0.5f, pct);

    // Núcleo analógico
    p.push_back (std::make_unique<APB> (ParameterID { id::coreOn, 1 }, "Nucleo", true));
    {
        NormalisableRange<float> r { 20.0f, 300.0f, 1.0f };
        r.setSkewForCentre (90.0f);
        F (id::hpf, "Low-cut", r, 80.0f, [] (float v, int) { return String (roundToInt (v)) + " Hz"; });
    }
    F (id::body, "Cuerpo", { -6.0f, 6.0f, 0.1f }, 0.0f, db1);
    F (id::presence, "Presencia", { -6.0f, 6.0f, 0.1f }, 1.5f, db1);
    F (id::air, "Aire", { 0.0f, 10.0f, 0.1f }, 3.0f, db1);
    F (id::comp, "Compresion", { 0.0f, 1.0f, 0.001f }, 0.5f, pct);
    F (id::color, "Color", { 0.0f, 1.0f, 0.001f }, 0.5f, pct);
    F (id::warmth, "Calor", { 0.0f, 1.0f, 0.001f }, 0.35f, pct);
    F (id::parallel, "Mezcla", { 0.0f, 1.0f, 0.001f }, 1.0f, pct);

    // Ear candy
    F (id::doubler, "Doble", { 0.0f, 1.0f, 0.001f }, 0.0f, pct);
    F (id::delayMix, "Delay", { 0.0f, 1.0f, 0.001f }, 0.0f, pct);
    p.push_back (std::make_unique<APC> (ParameterID { id::delayDiv, 1 }, "Delay tiempo", delayDivisionNames(), 1));
    F (id::delayFb, "Feedback", { 0.0f, 0.9f, 0.001f }, 0.35f, pct);
    F (id::delayTone, "Tono delay", { 0.0f, 1.0f, 0.001f }, 0.5f, pct);
    F (id::reverbMix, "Reverb", { 0.0f, 1.0f, 0.001f }, 0.15f, pct);
    F (id::reverbSize, "Tamano", { 0.0f, 1.0f, 0.001f }, 0.5f, pct);
    F (id::reverbDecay, "Decay", { 0.3f, 6.0f, 0.01f }, 1.8f, [] (float v, int) { return String (v, 1) + " s"; });
    F (id::reverbPre, "Pre-delay", { 0.0f, 150.0f, 0.1f }, 25.0f, [] (float v, int) { return String (roundToInt (v)) + " ms"; });
    F (id::duck, "Ducking", { 0.0f, 1.0f, 0.001f }, 0.6f, pct);
    F (id::radio, "Radio", { 0.0f, 1.0f, 0.001f }, 0.0f, pct);
    F (id::width, "Ancho", { 0.0f, 2.0f, 0.001f }, 1.0f, pct);

    return { p.begin(), p.end() };
}

// Lectura rápida de todos los parámetros desde el hilo de audio
class ParamReader
{
public:
    explicit ParamReader (juce::AudioProcessorValueTreeState& s) : state (s) {}

    float get (const char* pid) const
    {
        if (auto* v = state.getRawParameterValue (pid)) return v->load (std::memory_order_relaxed);
        jassertfalse;
        return 0.0f;
    }

    void fill (EngineParams& e) const
    {
        e.inGainDb = get (id::inGain);
        e.outGainDb = get (id::outGain);
        e.ceilingDb = get (id::ceiling);
        e.riderOn = get (id::riderOn) > 0.5f;
        e.riderTargetDb = get (id::riderTarget);
        e.riderRangeDb = get (id::riderRange);
        e.smooth = get (id::smooth);
        e.deEss = get (id::deEss);

        auto& p = e.pitch;
        p.correctionOn = get (id::tuneOn) > 0.5f;
        p.key = juce::roundToInt (get (id::key));
        p.scale = (ScaleType) juce::roundToInt (get (id::scale));
        p.retuneMs = get (id::retune);
        p.humanize = get (id::humanize);
        p.flexTune = get (id::flex);
        p.vibrato = get (id::vibrato);
        p.throatSemis = get (id::throat);
        p.transposeSemis = (float) juce::roundToInt (get (id::transpose));
        p.detuneCents = get (id::detune);
        p.tracking = get (id::tracking);
        p.range = (VoiceRange) juce::roundToInt (get (id::range));
        p.harmony[0] = (HarmonyInterval) juce::roundToInt (get (id::harm1));
        p.harmony[1] = (HarmonyInterval) juce::roundToInt (get (id::harm2));
        e.harmLevel = get (id::harmLevel);

        e.coreOn = get (id::coreOn) > 0.5f;
        e.hpfHz = get (id::hpf);
        e.bodyDb = get (id::body);
        e.presenceDb = get (id::presence);
        e.airDb = get (id::air);
        e.comp = get (id::comp);
        e.color = get (id::color);
        e.warmth = get (id::warmth);
        e.parallel = get (id::parallel);

        e.doubler = get (id::doubler);
        e.delayMix = get (id::delayMix);
        e.delayBeats = delayDivisionBeats (juce::roundToInt (get (id::delayDiv)));
        e.delayFeedback = get (id::delayFb);
        e.delayTone = get (id::delayTone);
        e.reverbMix = get (id::reverbMix);
        e.reverbSize = get (id::reverbSize);
        e.reverbDecay = get (id::reverbDecay);
        e.reverbPreMs = get (id::reverbPre);
        e.duck = get (id::duck);
        e.radio = get (id::radio);
        e.width = get (id::width);
    }

private:
    juce::AudioProcessorValueTreeState& state;
};

} // namespace evo::params
