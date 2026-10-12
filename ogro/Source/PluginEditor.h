// OGRO — interfaz principal
#pragma once

#include "Widgets.h"

namespace ogro::ui
{
class Page : public juce::Component
{
public:
    virtual void tick() {}
    virtual void trackChanged (int) {}

protected:
    static void box (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour accent = col::orange)
    {
        auto f = r.toFloat();
        g.setColour (col::panel);
        g.fillRoundedRectangle (f, 5.0f);
        g.setColour (col::line);
        g.drawRoundedRectangle (f.reduced (0.5f), 5.0f, 1.0f);
        g.setColour (accent);
        g.fillRect (f.getX() + 10.0f, f.getY() + 8.0f, 3.0f, 12.0f);
        g.setColour (col::cream);
        g.setFont (font (11.5f, true));
        g.drawText (title, juce::Rectangle<float> (f.getX() + 18.0f, f.getY() + 5.0f, f.getWidth() - 24.0f, 18.0f), juce::Justification::centredLeft);
    }
};

class Content : public juce::Component, private juce::Timer
{
public:
    static constexpr int kW = 1280, kH = 820;
    explicit Content (OgroProcessor&);
    ~Content() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void selectTrack (int t);
    void showPage (int i);
    void refreshPresetBox();
    void paintLogo (juce::Graphics&, juce::Rectangle<float>);

    OgroProcessor& proc;
    SeqContext ctx;

    // Cabecera
    juce::ComboBox presetBox;
    juce::TextButton prevPreset { "<" }, nextPreset { ">" }, saveButton, loadButton;
    juce::OwnedArray<juce::TextButton> patternButtons;
    juce::TextButton playButton;
    Lcd lcd;
    std::unique_ptr<Knob> swing, rootKnob;
    std::unique_ptr<Choice> rootChoice;
    std::unique_ptr<juce::FileChooser> chooser;

    // Pistas y macros
    juce::OwnedArray<TrackRow> rows;
    juce::OwnedArray<Knob> macros;
    Meter outL { "L", false, 48.0f }, outR { "R", false, 48.0f };

    // Páginas
    juce::OwnedArray<juce::TextButton> tabs;
    juce::OwnedArray<Page> pages;
    int currentPage = 0;

    // Secuenciador
    StepLane lane;
    StepKeys keys;
    juce::OwnedArray<juce::TextButton> modeButtons;
    juce::TextButton lenMinus { "-" }, lenPlus { "+" }, inventButton, clearButton, copyButton, pasteButton;
    int clipboardPattern = -1;
    juce::Rectangle<int> headerArea, trackArea, macroArea, pageArea, seqArea, seqTools;
    int lastTrigger[kTracks] {};
    float flash[kTracks] {};
    uint32_t lastRevision = 0;
    int blink = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Content)
};
} // namespace ogro::ui

class OgroEditor : public juce::AudioProcessorEditor
{
public:
    explicit OgroEditor (OgroProcessor&);
    ~OgroEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ogro::ui::OgroLookAndFeel lnf;
    ogro::ui::Content content;
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OgroEditor)
};
