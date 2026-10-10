// EVO SOL — interfaz
#pragma once

#include "EvoLookAndFeel.h"
#include "PluginProcessor.h"

namespace evo::ui
{
// Knob con nombre arriba y valor abajo
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& title,
          juce::Colour accent, bool bipolar = false);
    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider slider;

private:
    juce::String title;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Combo enlazado a un parámetro de elección
class Choice : public juce::Component
{
public:
    Choice (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& title);
    void paint (juce::Graphics&) override;
    void resized() override;

    juce::ComboBox box;

private:
    juce::String title;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

// Interruptor ON/OFF enlazado a un parámetro booleano
class Switch : public juce::Component
{
public:
    Switch (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& text, juce::Colour accent);
    void resized() override { button.setBounds (getLocalBounds()); }
    juce::ToggleButton button;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

// Barra de medición (nivel o reducción de ganancia)
class Meter : public juce::Component
{
public:
    enum class Mode { Level, Reduction, Bipolar };
    Meter (const juce::String& label, Mode mode, juce::Colour colour, float rangeDb);
    void setValue (float db);
    void paint (juce::Graphics&) override;

private:
    juce::String label;
    Mode mode;
    juce::Colour colour;
    float rangeDb, value = -100.0f, shown = -100.0f;
};

// Gráfico de afinación (pitch de entrada vs. corregido) estilo editor de pitch
class PitchGraph : public juce::Component
{
public:
    void push (bool voiced, float inMidi, float outMidi, int target);
    void setScale (int key, int scale) { currentKey = key; currentScale = scale; }
    void paint (juce::Graphics&) override;

private:
    static constexpr int kHistory = 180;
    std::array<float, kHistory> inHist {}, outHist {};
    std::array<bool, kHistory> voicedHist {};
    int head = 0;
    float centre = 60.0f;
    int lastTarget = -1;
    float lastDeviation = 0.0f;
    bool lastVoiced = false;
    int currentKey = 0, currentScale = 0;
};

// Logo del sol
class SunLogo : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void setPulse (float p) { pulse = p; repaint(); }

private:
    float pulse = 0.0f;
};

// Lienzo con todo el layout (se escala completo al redimensionar la ventana)
class Content : public juce::Component, private juce::Timer
{
public:
    static constexpr int kWidth = 1120, kHeight = 760;
    explicit Content (EvoSolProcessor&);
    ~Content() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void layoutKnobRow (std::initializer_list<Knob*> knobs, juce::Rectangle<int> area);
    void drawPanel (juce::Graphics&, juce::Rectangle<int> r, const juce::String& title,
                    const juce::String& subtitle, juce::Colour accent);
    void updateAssistant();

    EvoSolProcessor& proc;
    juce::AudioProcessorValueTreeState& st;

    SunLogo logo;
    juce::ComboBox presetBox;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" };

    // Cabecera
    Knob inGain, outGain, ceiling;
    Meter inMeter, outMeterL, outMeterR;

    // Afinación
    Switch tuneOn;
    Choice key, scale, range;
    PitchGraph pitchGraph;
    Knob retune, humanize, flex, vibrato, throat, transpose, detune, tracking;

    // Inteligencia
    Switch riderOn;
    Knob riderTarget, riderRange, smooth, deEss;
    juce::TextButton analyseButton, applyButton;
    juce::Label assistantStatus;
    Meter riderMeter, smoothMeter, deEssMeter;

    // Núcleo
    Switch coreOn;
    Knob hpf, body, presence, air, comp, color, warmth, parallel;
    Meter fetMeter, optoMeter, limMeter;

    // Ear candy
    Choice harm1, harm2, delayDiv;
    Knob harmLevel, doubler, radio, width, duck;
    Knob delayMix, delayFb, delayTone, reverbMix, reverbSize, reverbDecay, reverbPre;

    juce::Rectangle<int> headerArea, tuneArea, aiArea, coreArea, fxArea;
    evo::VocalAssistant::Suggestion lastSuggestion;
    bool hasSuggestion = false;
    float sunPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Content)
};
} // namespace evo::ui

class EvoSolEditor : public juce::AudioProcessorEditor
{
public:
    explicit EvoSolEditor (EvoSolProcessor&);
    ~EvoSolEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    evo::ui::EvoLookAndFeel lnf;
    evo::ui::Content content;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EvoSolEditor)
};
