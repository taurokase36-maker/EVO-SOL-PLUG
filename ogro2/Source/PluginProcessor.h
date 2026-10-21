// OGRO 2 — procesador del plugin (instrumento)
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/Engine.h"
#include "engine/Generator.h"
#include "engine/Presets.h"

class Ogro2Processor : public juce::AudioProcessor
{
public:
    // Versión del formato del estado guardado (sesiones y presets). Subirla solo con migración.
    static constexpr int kStateVersion = 1;
    // Salidas separadas: la principal + una por pista (10) + 2 reservadas para versiones 2.x
    static constexpr int kAuxOutputs = 12;

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
    bool importV1Preset (const juce::File& file); // .ogro de OGRO 1.0
    juce::String getUserPresetName() const;
    static juce::File presetFolder();

    // Ruteo de MOD y MOTION (no son parámetros del host: se guardan por ID en el estado)
    void setModSource (int slot, int src);
    void setModDest (int slot, int param);
    void setLaneDest (int lane, int param);

    // Avisa al host que cambió algo que no es un parámetro (patrones, ruteo), para que
    // Ableton marque la sesión como modificada
    void notifyStateChanged();

    // Helpers para la interfaz
    juce::RangedAudioParameter* param (int index) const { return params[(size_t) index]; }
    float value (int index) const { return raw[(size_t) index] != nullptr ? raw[(size_t) index]->load (std::memory_order_relaxed) : 0.0f; }
    void setValue (int index, float physical);

    juce::AudioProcessorValueTreeState state;
    ogro2::Engine engine;
    std::atomic<int> selectedTrack { 0 };

private:
    static BusesProperties createBuses();
    std::unique_ptr<juce::XmlElement> createStateXml();
    void restoreFromXml (const juce::XmlElement& xml);
    void applyValues (const std::vector<float>& values);
    void applyV1 (const juce::XmlElement& xml, const juce::String& name);
    void setUserPresetName (const juce::String& n);

    std::vector<juce::RangedAudioParameter*> params;
    std::vector<std::atomic<float>*> raw;
    std::vector<float> values;
    std::vector<ogro2::MidiTrigger> midiTriggers;
    std::atomic<int> currentPreset { 0 };
    juce::String userPresetName;
    mutable juce::SpinLock nameLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Ogro2Processor)
};
