// OGRO 2 — interfaz principal
#pragma once

#include "Widgets.h"

namespace ogro2::ui
{
// Página que se muestra dentro de la ventana violeta
class Page : public juce::Component
{
public:
    virtual void tick() {}
    virtual void trackChanged (int) {}
    virtual juce::String title() const = 0;
};

class Content : public juce::Component, private juce::Timer
{
public:
    static constexpr int kW = 1280, kH = 820;
    explicit Content (Ogro2Processor&);
    ~Content() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    enum PageId { PageTrack = 0, PageFx, PageMod, PageMotion, PageRing, PageMaster, PageCount };

    void timerCallback() override;
    void selectTrack (int t);
    void showPage (int i);
    void refreshPreset();
    void showPresetMenu();
    void showFileMenu();
    void paintLogo (juce::Graphics&, juce::Rectangle<float>);

    Ogro2Processor& proc;
    SeqContext ctx;

    // Cabecera
    juce::TextButton prevPreset { T ("‹") }, nextPreset { T ("›") }, fileButton;
    PresetDisplay presetDisplay;
    juce::OwnedArray<juce::TextButton> patternButtons;
    PlayOrb playOrb;
    Lcd bpmLcd;
    std::unique_ptr<Choice> rootChoice;
    std::unique_ptr<juce::FileChooser> chooser;

    // Pistas
    juce::OwnedArray<TrackRow> rows;

    // Ventana violeta: páginas
    juce::OwnedArray<Page> pages;
    juce::TextButton backButton;
    int currentPage = PageTrack;

    // Columna derecha: macros, paneles y master
    juce::OwnedArray<MacroKnob> macros;
    juce::OwnedArray<juce::TextButton> panelButtons;
    MasterDisc disc;
    std::unique_ptr<Knob> ceiling, volume;

    // Groove y secuenciador
    std::unique_ptr<Knob> swing;
    juce::TextButton lenMinus { T ("−") }, lenPlus { "+" }, inventButton, clearButton, copyButton, pasteButton;
    juce::OwnedArray<juce::TextButton> modeButtons;
    StepLane lane;
    StepKeys keys;
    int clipboardPattern = -1;

    // Zonas (coordenadas de diseño, 1280 x 820)
    juce::Rectangle<int> chassis, header, tube, trackCard, window, macroCard, masterCard, grooveCard, seqCard;
    int lastTrigger[kTracks] {};
    float flash[kTracks] {};
    uint32_t lastRevision = 0;
    int blink = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Content)
};
} // namespace ogro2::ui

class Ogro2Editor : public juce::AudioProcessorEditor
{
public:
    explicit Ogro2Editor (Ogro2Processor&);
    ~Ogro2Editor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ogro2::ui::Ogro2LookAndFeel lnf;
    ogro2::ui::Content content;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Ogro2Editor)
};
