// EVO SOL — presets de fábrica. No tocan key, escala, tipo de voz ni referencia,
// así podés cambiar de preset sin perder la tonalidad del tema.
#pragma once

#include "Parameters.h"

namespace evo::presets
{
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

inline const std::vector<Preset>& factory()
{
    using namespace evo::params::id;
    static const std::vector<Preset> list {
        { "Inicial", {} },
        { "Trap Hard Tune", { { retune, 0.0f }, { humanize, 0.0f }, { flex, 0.0f }, { comp, 0.65f }, { warmth, 0.45f },
                              { color, 0.6f }, { air, 4.0f }, { presence, 2.0f }, { doubler, 0.35f }, { delayMix, 0.18f },
                              { delayDiv, 3.0f }, { delayFb, 0.3f }, { reverbMix, 0.18f }, { reverbSize, 0.45f },
                              { reverbDecay, 1.6f }, { smooth, 0.4f }, { deEss, 0.5f } } },
        { "Reggaeton Brillante", { { retune, 12.0f }, { humanize, 0.2f }, { comp, 0.6f }, { warmth, 0.35f }, { air, 5.0f },
                                   { presence, 2.5f }, { doubler, 0.25f }, { delayMix, 0.15f }, { delayDiv, 2.0f },
                                   { delayFb, 0.35f }, { reverbMix, 0.15f }, { smooth, 0.45f }, { deEss, 0.5f } } },
        { "R&B Natural", { { retune, 60.0f }, { humanize, 0.6f }, { flex, 0.4f }, { comp, 0.45f }, { warmth, 0.3f },
                           { color, 0.3f }, { air, 3.0f }, { body, 1.0f }, { reverbMix, 0.25f }, { reverbSize, 0.6f },
                           { reverbDecay, 2.2f }, { delayMix, 0.12f }, { delayDiv, 1.0f } } },
        { "Coros Armonizados", { { retune, 25.0f }, { humanize, 0.3f }, { harm1, 4.0f }, { harm2, 1.0f }, { harmLevel, 0.55f },
                                 { doubler, 0.5f }, { reverbMix, 0.3f }, { width, 1.4f }, { comp, 0.5f } } },
        { "Radio Intro", { { radio, 0.85f }, { delayMix, 0.25f }, { delayDiv, 3.0f }, { reverbMix, 0.1f }, { comp, 0.7f },
                           { retune, 15.0f } } },
        { "Particula Maxima", { { comp, 0.85f }, { warmth, 0.65f }, { color, 0.8f }, { parallel, 0.85f }, { air, 5.0f },
                                { presence, 2.5f }, { body, 1.5f }, { ceiling, -0.5f }, { outGain, 2.0f }, { smooth, 0.5f },
                                { deEss, 0.55f }, { retune, 20.0f } } },
        { "Robot Extremo", { { retune, 0.0f }, { humanize, 0.0f }, { flex, 0.0f }, { vibrato, -1.0f }, { throat, -1.0f },
                             { comp, 0.6f }, { tracking, 0.7f } } },
        { "Balada Aerea", { { retune, 80.0f }, { humanize, 0.7f }, { reverbMix, 0.4f }, { reverbSize, 0.85f },
                            { reverbDecay, 3.5f }, { reverbPre, 60.0f }, { delayMix, 0.2f }, { delayDiv, 2.0f },
                            { delayFb, 0.45f }, { air, 4.5f }, { comp, 0.4f } } },
    };
    return list;
}

// Parámetros que un preset nunca modifica
inline bool isProtected (const juce::String& pid)
{
    using namespace evo::params::id;
    return pid == key || pid == scale || pid == range || pid == detune;
}

inline void apply (juce::AudioProcessorValueTreeState& state, int index)
{
    const auto& list = factory();
    if (index < 0 || index >= (int) list.size()) return;
    const auto& preset = list[(size_t) index];

    for (auto* param : state.processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param);
        if (ranged == nullptr) continue;
        const auto pid = ranged->getParameterID();
        if (isProtected (pid)) continue;

        float norm = ranged->getDefaultValue();
        for (const auto& [name, value] : preset.values)
            if (pid == name) norm = ranged->convertTo0to1 (value);

        ranged->beginChangeGesture();
        ranged->setValueNotifyingHost (norm);
        ranged->endChangeGesture();
    }
}

} // namespace evo::presets
