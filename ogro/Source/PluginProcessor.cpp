#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace ogro;

static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> out;
    for (const auto& d : paramDefs())
    {
        const juce::ParameterID pid { d.id, 1 };
        const auto name = juce::String::fromUTF8 (d.name.c_str());
        auto toText = [fn = d.text] (float v, int) { return juce::String::fromUTF8 (fn (v).c_str()); };
        switch (d.kind)
        {
            case Kind::Float:
            {
                juce::NormalisableRange<float> range;
                if (d.log)
                {
                    range = juce::NormalisableRange<float> (
                        d.min, d.max,
                        [] (float a, float b, float n) { return a * std::pow (b / a, n); },
                        [] (float a, float b, float v) { return std::log (juce::jmax (v, a) / a) / std::log (b / a); });
                }
                else
                    range = juce::NormalisableRange<float> (d.min, d.max);
                out.push_back (std::make_unique<juce::AudioParameterFloat> (
                    pid, name, range, d.def, juce::AudioParameterFloatAttributes().withStringFromValueFunction (toText)));
                break;
            }
            case Kind::Choice:
            {
                juce::StringArray choices;
                for (const auto& c : d.choices) choices.add (juce::String::fromUTF8 (c.c_str()));
                out.push_back (std::make_unique<juce::AudioParameterChoice> (pid, name, choices, (int) d.def));
                break;
            }
            case Kind::Bool:
                out.push_back (std::make_unique<juce::AudioParameterBool> (pid, name, d.def > 0.5f));
                break;
        }
    }
    return { out.begin(), out.end() };
}

OgroProcessor::OgroProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Salida", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "OGRO", createLayout())
{
    const auto& defs = paramDefs();
    params.resize (defs.size());
    raw.resize (defs.size());
    values.resize (defs.size());
    for (size_t i = 0; i < defs.size(); ++i)
    {
        params[i] = state.getParameter (defs[i].id);
        raw[i] = state.getRawParameterValue (defs[i].id);
        jassert (params[i] != nullptr && raw[i] != nullptr);
    }
    midiTriggers.reserve (512);
    factoryPresets()[0].build (engine.bank);
    engine.prepare (44100.0, 512);
    setLatencySamples (engine.getLatencySamples());
}

bool OgroProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void OgroProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, juce::jmax (32, samplesPerBlock));
    setLatencySamples (engine.getLatencySamples());
}

void OgroProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0) return;

    for (size_t i = 0; i < raw.size(); ++i) values[i] = raw[i]->load (std::memory_order_relaxed);
    engine.setParams (values.data(), (int) values.size());

    HostInfo host;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            host.valid = true;
            host.playing = pos->getIsPlaying();
            if (auto bpm = pos->getBpm()) host.bpm = *bpm;
            else host.bpm = values[P::Tempo];
            if (auto ppq = pos->getPpqPosition()) host.ppq = *ppq;
            else host.playing = false;
        }

    // MIDI: C1..G1 (36..43) disparan las pistas 1..8
    midiTriggers.clear();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn() && m.getNoteNumber() >= 36 && m.getNoteNumber() < 36 + kTracks)
            midiTriggers.push_back ({ meta.samplePosition, m.getNoteNumber() - 36, m.getFloatVelocity() });
    }
    midi.clear();

    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    if (r != nullptr)
        engine.process (host, midiTriggers.data(), (int) midiTriggers.size(), l, r, n);
    else
    {
        // Salida mono: renderizamos en estéreo en tramos y sumamos
        float right[512];
        for (int start = 0; start < n; start += 512)
        {
            const int len = juce::jmin (512, n - start);
            engine.process (host, midiTriggers.data(), (int) midiTriggers.size(), l + start, right, len);
            for (int i = 0; i < len; ++i) l[start + i] = 0.5f * (l[start + i] + right[i]);
            if (host.playing) host.ppq += (double) len * host.bpm / 60.0 / getSampleRate();
            for (auto& t : midiTriggers) t.sample -= len;
        }
    }
}

void OgroProcessor::setValue (int index, float physical)
{
    if (auto* p = params[(size_t) index])
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (physical));
        p->endChangeGesture();
    }
}

void OgroProcessor::loadPreset (int index)
{
    const auto& list = factoryPresets();
    if (index < 0 || index >= (int) list.size()) return;
    const auto& preset = list[(size_t) index];
    const auto& defs = paramDefs();
    std::vector<float> v (defs.size());
    for (size_t i = 0; i < defs.size(); ++i) v[i] = defs[i].def;
    for (const auto& [id, val] : preset.values)
    {
        const int idx = paramIndex (id);
        if (idx >= 0) v[(size_t) idx] = val;
    }
    for (size_t i = 0; i < defs.size(); ++i) setValue ((int) i, v[i]);
    preset.build (engine.bank);
    currentPreset = index;
    userPresetName.clear();
}

std::unique_ptr<juce::XmlElement> OgroProcessor::createStateXml()
{
    auto copy = state.copyState();
    copy.setProperty ("preset", currentPreset.load(), nullptr);
    copy.setProperty ("userPreset", userPresetName, nullptr);
    copy.setProperty ("patterns", juce::String (engine.bank.serialise()), nullptr);
    return copy.createXml();
}

void OgroProcessor::restoreFromXml (const juce::XmlElement& xml)
{
    if (! xml.hasTagName (state.state.getType())) return;
    auto tree = juce::ValueTree::fromXml (xml);
    currentPreset = (int) tree.getProperty ("preset", 0);
    userPresetName = tree.getProperty ("userPreset", "").toString();
    const auto pat = tree.getProperty ("patterns", "").toString();
    if (pat.isNotEmpty()) engine.bank.deserialise (pat.toStdString());
    tree.removeProperty ("patterns", nullptr);
    state.replaceState (tree);
}

void OgroProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createStateXml())
        copyXmlToBinary (*xml, destData);
}

void OgroProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        restoreFromXml (*xml);
}

bool OgroProcessor::saveUserPreset (const juce::File& file)
{
    auto xml = createStateXml();
    if (! xml) return false;
    xml->setAttribute ("userPreset", file.getFileNameWithoutExtension());
    if (! xml->writeTo (file)) return false;
    userPresetName = file.getFileNameWithoutExtension();
    return true;
}

bool OgroProcessor::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (! xml) return false;
    restoreFromXml (*xml);
    userPresetName = file.getFileNameWithoutExtension();
    return true;
}

juce::AudioProcessorEditor* OgroProcessor::createEditor() { return new OgroEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new OgroProcessor(); }
