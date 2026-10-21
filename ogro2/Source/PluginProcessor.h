// OGRO 2 — procesador del plugin (instrumento)
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/Engine.h"
#include "engine/Generator.h"
#include "engine/Presets.h"

class Ogro2Processor : public juce::AudioProcessor
{
public:
    Ogro2Processor();
    ~Ogro2Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    // Los presets viven en la interfaz (el host no los pisa al abrir la sesión)
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return JucePlugin_Name; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Presets
    void loadPreset (int index);
    int getPresetIndex() const { return currentPreset.load(); }
    bool saveUserPreset (const juce::File& file);
    bool loadUserPreset (const juce::File& file);
    juce::String userPresetName;

    // Helpers para la interfaz
    juce::RangedAudioParameter* param (int index) const { return params[(size_t) index]; }
    float value (int index) const { return raw[(size_t) index]->load (std::memory_order_relaxed); }
    void setValue (int index, float physical);

    juce::AudioProcessorValueTreeState state;
    ogro2::Engine engine;
    std::atomic<int> selectedTrack { 0 };

private:
    std::unique_ptr<juce::XmlElement> createStateXml();
    void restoreFromXml (const juce::XmlElement& xml);

    std::vector<juce::RangedAudioParameter*> params;
    std::vector<std::atomic<float>*> raw;
    std::vector<float> values;
    std::vector<ogro2::MidiTrigger> midiTriggers;
    std::atomic<int> currentPreset { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Ogro2Processor)
};
