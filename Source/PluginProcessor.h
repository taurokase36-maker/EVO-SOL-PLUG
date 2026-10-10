// EVO SOL — procesador del plugin
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "Presets.h"

class EvoSolProcessor : public juce::AudioProcessor
{
public:
    EvoSolProcessor();
    ~EvoSolProcessor() override = default;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EVO SOL"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    // Los presets viven en la interfaz (así el host nunca pisa tus ajustes al abrir la sesión)
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "EVO SOL"; }
    void changeProgramName (int, const juce::String&) override {}

    void loadPreset (int index);
    int getPresetIndex() const { return currentPreset; }
    juce::String getPresetName (int index) const;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- Asistente inteligente (llamar desde la interfaz) ---------------
    void startAnalysis();
    void stopAnalysis();
    bool isAnalysing() const { return analysing.load(); }
    float analysisSeconds() const { return liveSeconds.load(); }
    // Key detectada en vivo mientras escucha
    bool liveKey (int& key, bool& minor, float& confidence) const;
    // Sugerencia final (válida después de detener el análisis)
    bool getSuggestion (evo::VocalAssistant::Suggestion& out);
    void applySuggestion (const evo::VocalAssistant::Suggestion& s);

    juce::AudioProcessorValueTreeState state;
    evo::VocalEngine engine;

private:
    void setParam (const char* pid, float value);

    evo::params::ParamReader reader { state };
    evo::EngineParams engineParams;
    std::vector<float> scratchR = std::vector<float> (512, 0.0f);
    int currentPreset = 0;

    std::atomic<bool> analysing { false }, requestStart { false }, requestStop { false };
    std::atomic<float> liveSeconds { 0.0f };
    std::atomic<int> liveKeyIndex { -1 };
    std::atomic<bool> liveKeyMinor { false };
    std::atomic<float> liveKeyConf { 0.0f };
    int analysisCounter = 0;

    juce::SpinLock suggestionLock;
    evo::VocalAssistant::Suggestion pendingSuggestion;
    bool suggestionReady = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EvoSolProcessor)
};
