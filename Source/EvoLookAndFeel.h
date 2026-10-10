// EVO SOL — estética: paleta "atardecer" (sol naranja, ámbar, rosa, violeta)
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace evo::ui
{
inline juce::String T (const char* utf8) { return juce::String::fromUTF8 (utf8); }


namespace colours
{
    inline const juce::Colour bg { 0xff0b0a0f };
    inline const juce::Colour bgTop { 0xff17121c };
    inline const juce::Colour panel { 0xff141219 };
    inline const juce::Colour panelEdge { 0xff2a2533 };
    inline const juce::Colour knobFace { 0xff1d1a24 };
    inline const juce::Colour track { 0xff2c2735 };
    inline const juce::Colour text { 0xffefe7da };
    inline const juce::Colour textDim { 0xff8e869a };
    inline const juce::Colour sun { 0xffff7a1f };
    inline const juce::Colour amber { 0xffffb53f };
    inline const juce::Colour gold { 0xffffe08a };
    inline const juce::Colour rose { 0xffff5c8a };
    inline const juce::Colour violet { 0xff9d7bff };
    inline const juce::Colour good { 0xff7be3a6 };
}

inline juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

class EvoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    EvoLookAndFeel()
    {
        using namespace colours;
        setColour (juce::ResizableWindow::backgroundColourId, bg);
        setColour (juce::ComboBox::backgroundColourId, knobFace);
        setColour (juce::ComboBox::outlineColourId, panelEdge);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::ComboBox::arrowColourId, amber);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1a1720));
        setColour (juce::PopupMenu::textColourId, text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, sun.withAlpha (0.85f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
        setColour (juce::TextButton::buttonColourId, knobFace);
        setColour (juce::TextButton::buttonOnColourId, sun);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        setColour (juce::Label::textColourId, text);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::backgroundColourId, knobFace);
        setColour (juce::TextEditor::textColourId, text);
        setColour (juce::TextEditor::highlightColourId, sun.withAlpha (0.4f));
        setColour (juce::CaretComponent::caretColourId, amber);
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff1f1b27));
        setColour (juce::TooltipWindow::textColourId, text);
        setColour (juce::TooltipWindow::outlineColourId, panelEdge);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        using namespace juce;
        const auto accent = slider.findColour (Slider::rotarySliderFillColourId);
        const bool bipolar = (bool) slider.getProperties().getWithDefault ("bipolar", false);
        const auto bounds = Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
        const float radius = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float arcW = jmax (2.5f, radius * 0.11f);
        const float arcR = radius - arcW * 0.5f;

        // Pista
        Path trackPath;
        trackPath.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (colours::track);
        g.strokePath (trackPath, PathStrokeType (arcW, PathStrokeType::curved, PathStrokeType::rounded));

        // Arco de valor (desde el centro si es bipolar)
        const float angle = startAngle + pos * (endAngle - startAngle);
        const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
        if (std::abs (angle - from) > 0.01f)
        {
            Path valuePath;
            valuePath.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, jmin (from, angle), jmax (from, angle), true);
            ColourGradient grad (accent.brighter (0.35f), centre.x - radius, centre.y + radius,
                                 accent, centre.x + radius, centre.y - radius, false);
            g.setGradientFill (grad);
            g.strokePath (valuePath, PathStrokeType (arcW, PathStrokeType::curved, PathStrokeType::rounded));
        }

        // Cara del knob
        const float faceR = arcR - arcW * 1.3f;
        ColourGradient face (colours::knobFace.brighter (0.25f), centre.x, centre.y - faceR,
                             colours::knobFace.darker (0.4f), centre.x, centre.y + faceR, false);
        g.setGradientFill (face);
        g.fillEllipse (centre.x - faceR, centre.y - faceR, faceR * 2.0f, faceR * 2.0f);
        g.setColour (Colours::black.withAlpha (0.5f));
        g.drawEllipse (centre.x - faceR, centre.y - faceR, faceR * 2.0f, faceR * 2.0f, 1.0f);

        // Indicador
        const float inner = faceR * 0.35f, outer = faceR * 0.86f;
        const Point<float> a (centre.x + inner * std::sin (angle), centre.y - inner * std::cos (angle));
        const Point<float> b (centre.x + outer * std::sin (angle), centre.y - outer * std::cos (angle));
        g.setColour (slider.isEnabled() ? colours::text : colours::textDim);
        g.drawLine ({ a, b }, jmax (2.0f, radius * 0.07f));
        if (slider.isMouseOverOrDragging())
        {
            g.setColour (accent.withAlpha (0.18f));
            g.fillEllipse (centre.x - faceR, centre.y - faceR, faceR * 2.0f, faceR * 2.0f);
        }
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool) override
    {
        using namespace juce;
        const auto accent = b.findColour (ToggleButton::tickColourId);
        auto r = b.getLocalBounds().toFloat();
        auto pill = r.removeFromLeft (jmin (38.0f, r.getWidth())).withSizeKeepingCentre (34.0f, 18.0f);
        const bool on = b.getToggleState();
        g.setColour (on ? accent : colours::track);
        g.fillRoundedRectangle (pill, 9.0f);
        if (highlighted) { g.setColour (Colours::white.withAlpha (0.08f)); g.fillRoundedRectangle (pill, 9.0f); }
        const float d = 14.0f;
        const float cx = on ? pill.getRight() - d * 0.5f - 2.0f : pill.getX() + d * 0.5f + 2.0f;
        g.setColour (on ? Colours::black.withAlpha (0.85f) : colours::textDim);
        g.fillEllipse (cx - d * 0.5f, pill.getCentreY() - d * 0.5f, d, d);
        if (b.getButtonText().isNotEmpty())
        {
            g.setColour (on ? colours::text : colours::textDim);
            g.setFont (font (12.0f, true));
            g.drawText (b.getButtonText(), r.withTrimmedLeft (6.0f), Justification::centredLeft);
        }
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace juce;
        auto r = Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
        g.setColour (box.findColour (ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (box.hasKeyboardFocus (true) ? colours::amber.withAlpha (0.7f) : colours::panelEdge);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
        Path arrow;
        const float ax = (float) width - 14.0f, ay = (float) height * 0.5f;
        arrow.addTriangle (ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour (box.findColour (ComboBox::arrowColourId));
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.0f, true); }
    juce::Font getPopupMenuFont() override { return font (14.0f); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 1, box.getWidth() - 26, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgColour,
                               bool highlighted, bool down) override
    {
        using namespace juce;
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        Colour c = on ? b.findColour (TextButton::buttonOnColourId) : bgColour;
        if (! b.isEnabled()) c = c.withMultipliedAlpha (0.4f);
        if (down) c = c.darker (0.2f);
        else if (highlighted) c = c.brighter (0.12f);
        if (on)
        {
            ColourGradient grad (c.brighter (0.25f), r.getX(), r.getY(), c, r.getRight(), r.getBottom(), false);
            g.setGradientFill (grad);
        }
        else
            g.setColour (c);
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (on ? c.brighter (0.4f) : colours::panelEdge.brighter (0.15f));
        g.drawRoundedRectangle (r, 7.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return font (juce::jmin (13.5f, h * 0.45f), true); }
};

} // namespace evo::ui
