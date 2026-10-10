#include "PluginProcessor.h"
#include "PluginEditor.h"

EvoSolProcessor::EvoSolProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Entrada", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Salida", juce::AudioChannelSet::stereo(), true)),
      state (*this, nullptr, "PARAMS", evo::params::createLayout())
{
    // Latencia inicial (44.1 kHz); se recalcula en prepareToPlay con la frecuencia real
    engine.prepare (44100.0, 512);
    setLatencySamples (engine.getLatencySamples());
}

bool EvoSolProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const bool inOk = in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    const bool outOk = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    return inOk && outOk;
}

void EvoSolProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, juce::jmax (64, samplesPerBlock));
    scratchR.assign ((size_t) juce::jmax (64, samplesPerBlock), 0.0f);
    reader.fill (engineParams);
    engine.setParams (engineParams);
    setLatencySamples (engine.getLatencySamples());
}

void EvoSolProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0) return;

    // Pedidos del asistente
    if (requestStart.exchange (false))
    {
        engine.startLearning();
        analysing = true;
        analysisCounter = 0;
        liveSeconds = 0.0f;
        liveKeyIndex = -1;
    }
    if (requestStop.exchange (false) && analysing.load())
    {
        engine.stopLearning();
        const auto s = engine.suggestion();
        {
            const juce::SpinLock::ScopedTryLockType lock (suggestionLock);
            if (lock.isLocked()) { pendingSuggestion = s; suggestionReady = true; }
        }
        analysing = false;
    }

    reader.fill (engineParams);
    engineParams.pitch.learnKey = analysing.load();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                engineParams.bpm = *bpm;
    engine.setParams (engineParams);

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const float* inL = buffer.getReadPointer (0);
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : nullptr;

    if (numOut >= 2)
    {
        engine.process (inL, inR, buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);
    }
    else
    {
        // Salida mono: sumamos el estéreo interno (buffer temporal preasignado)
        auto* outL = buffer.getWritePointer (0);
        const int chunk = (int) scratchR.size();
        for (int start = 0; start < numSamples; start += chunk)
        {
            const int n = juce::jmin (chunk, numSamples - start);
            engine.process (inL + start, inR ? inR + start : nullptr, outL + start, scratchR.data(), n);
            for (int i = 0; i < n; ++i) outL[start + i] = 0.5f * (outL[start + i] + scratchR[(size_t) i]);
        }
    }

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // Actualización en vivo del análisis (~4 veces por segundo)
    if (analysing.load())
    {
        analysisCounter += numSamples;
        if (analysisCounter >= (int) (getSampleRate() * 0.25))
        {
            analysisCounter = 0;
            const auto k = engine.keyResult();
            liveKeyIndex = k.totalSeconds > 0.5f ? k.key : -1;
            liveKeyMinor = k.minor;
            liveKeyConf = k.confidence;
            liveSeconds = k.totalSeconds;
        }
    }
}

void EvoSolProcessor::startAnalysis()
{
    {
        const juce::SpinLock::ScopedLockType lock (suggestionLock);
        suggestionReady = false;
    }
    requestStart = true;
}

void EvoSolProcessor::stopAnalysis() { requestStop = true; }

bool EvoSolProcessor::liveKey (int& key, bool& minor, float& confidence) const
{
    key = liveKeyIndex.load();
    minor = liveKeyMinor.load();
    confidence = liveKeyConf.load();
    return key >= 0;
}

bool EvoSolProcessor::getSuggestion (evo::VocalAssistant::Suggestion& out)
{
    const juce::SpinLock::ScopedLockType lock (suggestionLock);
    if (! suggestionReady) return false;
    out = pendingSuggestion;
    return true;
}

void EvoSolProcessor::setParam (const char* pid, float value)
{
    if (auto* p = state.getParameter (pid))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void EvoSolProcessor::applySuggestion (const evo::VocalAssistant::Suggestion& s)
{
    using namespace evo::params::id;
    if (! s.valid) return;
    setParam (key, (float) s.key);
    setParam (scale, (float) (s.minor ? (int) evo::ScaleType::NaturalMinor : (int) evo::ScaleType::Major));
    setParam (range, (float) s.range);
    setParam (hpf, s.hpfHz);
    setParam (body, s.bodyDb);
    setParam (presence, s.presenceDb);
    setParam (air, s.airDb);
    setParam (deEss, s.deEss);
    setParam (inGain, s.inputGainDb);
}

void EvoSolProcessor::loadPreset (int index)
{
    if (index < 0 || index >= (int) evo::presets::factory().size()) return;
    currentPreset = index;
    evo::presets::apply (state, index);
}

juce::String EvoSolProcessor::getPresetName (int index) const
{
    const auto& list = evo::presets::factory();
    return index >= 0 && index < (int) list.size() ? juce::String (list[(size_t) index].name) : juce::String();
}

void EvoSolProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto copy = state.copyState();
    copy.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = copy.createXml())
        copyXmlToBinary (*xml, destData);
}

void EvoSolProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (state.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentPreset = tree.getProperty ("preset", 0);
            state.replaceState (tree);
        }
}

juce::AudioProcessorEditor* EvoSolProcessor::createEditor()
{
    return new EvoSolEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EvoSolProcessor();
}
