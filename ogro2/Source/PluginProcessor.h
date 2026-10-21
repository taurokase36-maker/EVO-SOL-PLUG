// OGRO 2 — procesador del plugin (instrumento)
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "engine/Engine.h"
#include "engine/Generator.h"
#include "engine/Presets.h"

class Ogro2Processor : public juce::AudioProcessor, private juce::Timer
{
public:
    // Versión del formato del estado guardado (sesiones y presets). Subirla solo con migración.
    // 1: fase 1 · 2: fase 3 (tonalidad, melodía, texturas; migración con ParamDef::compat)
    static constexpr int kStateVersion = 2;
    // Salidas separadas: la principal + una por pista (10) + 2 reservadas para versiones 2.x
    static constexpr int kAuxOutputs = 12;

    Ogro2Processor();
    ~Ogro2Processor() override { stopTimer(); alive->store (false); }

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

    // Deshacer / rehacer del secuenciador (patrones, p-locks, cadena y carriles; no los
    // parámetros, que los deshace Ableton). Se llama a pushUndo() antes de cada edición.
    void pushUndo();
    bool undo();
    bool redo();
    bool canUndo() const { return ! undoStack.empty(); }
    bool canRedo() const { return ! redoStack.empty(); }
    // El largo del patrón (para la cadena y el cambio "al final") sigue a la pista más larga
    void syncPatternLength (int pattern);

    // ---- Samples (fase 4) ----
    // slot: 0 = TEXTURAS, 1 = ACORDES. El estado guarda el nombre, no la ruta.
    void setSampleName (int slot, const juce::String& name);
    juce::String getSampleName (int slot) const;
    // "" si está todo bien; si no, el aviso para la interfaz ("Falta ...", "Cargando...")
    juce::String sampleStatus (int slot) const;
    void rollSampleDice (int slot);   // DADO: uno al azar de la categoría elegida
    static juce::File defaultSampleFolder();
    static juce::File orbhitzSource();   // la librería en Descargas, si está
    juce::File getSampleFolder() const;
    void setSampleFolder (const juce::File& dir);
    // Copia CHD, PAD, HIT y SFX de Orbhitz a Música > OGRO Samples > Orbhitz (en el fondo)
    void copyOrbhitz (std::function<void (bool, juce::String)> done);
    std::atomic<bool> copying { false };

    // ---- Fase 5: escenas A/B (MORPH) y dado de sonido con candados ----
    void storeScene (int which);          // guarda las perillas actuales en la escena A (0) o B (1)
    void recallScene (int which);         // vuelve las perillas a la escena
    bool hasScene (int which) const { return engine.hasSnapshot (which); }
    // Candados: perilla k (0..7 de sonido, 8.. de la tira) de la pista t no se mueve con el dado
    bool isLocked (int track, int knob) const { return (soundLocks[juce::jlimit (0, ogro2::kTracks - 1, track)].load() >> knob) & 1u; }
    void toggleLock (int track, int knob);
    // Dado: mueve al azar las perillas sin candado de la pista. amount 0..1 (1 = todo al azar)
    void rollSoundDice (int track, float amount);

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
    std::atomic<int> seqPage { 0 };      // vista del secuenciador: pasos 1-16 o 17-32

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
    std::string bankText() const;
    void timerCallback() override;
    void refreshSamples();
    std::unique_ptr<juce::PropertiesFile> settings;
    juce::String sampleNames[2];
    int lastPoolSize = -1, lastCat = -1;
    mutable juce::SpinLock sampleLock;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    std::atomic<uint32_t> soundLocks[ogro2::kTracks] {};
    std::vector<std::string> undoStack, redoStack;
    std::atomic<int> currentPreset { 0 };
    juce::String userPresetName;
    mutable juce::SpinLock nameLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Ogro2Processor)
};
