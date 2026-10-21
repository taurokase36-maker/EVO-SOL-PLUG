// OGRO 2 — estética inspirada en las cajas de ritmo analógicas de los 80:
// placa crema arriba, cuerpo oscuro, teclas de pasos por colores y LEDs rojos.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ogro2::ui
{
inline juce::String T (const char* utf8) { return juce::String::fromUTF8 (utf8); }

namespace col
{
    inline const juce::Colour plate { 0xffd9d0bc };
    inline const juce::Colour plateShade { 0xffc4b99f };
    inline const juce::Colour ink { 0xff1c1a17 };
    inline const juce::Colour body { 0xff23211e };
    inline const juce::Colour bodyLight { 0xff2e2b27 };
    inline const juce::Colour panel { 0xff1a1917 };
    inline const juce::Colour line { 0xff4b463d };
    inline const juce::Colour cream { 0xffeee5d0 };
    inline const juce::Colour dim { 0xff9b927f };
    inline const juce::Colour red { 0xffd9432c };
    inline const juce::Colour orange { 0xffee8a2b };
    inline const juce::Colour yellow { 0xfff1c23a };
    inline const juce::Colour keyCream { 0xffe9e1cb };
    inline const juce::Colour led { 0xffff3a24 };
    inline const juce::Colour ledOff { 0xff4a1e18 };
    inline const juce::Colour lcd { 0xff120f0d };
    inline const juce::Colour lcdText { 0xffff5a36 };

    inline juce::Colour track (int t)
    {
        static const juce::uint32 c[] = { 0xffd9432c, 0xffee8a2b, 0xfff1c23a, 0xffe9e1cb,
                                          0xffbfb59c, 0xff7fb8a4, 0xff6f9fd8, 0xffb48bd6 };
        return juce::Colour (c[juce::jlimit (0, 7, t)]);
    }
    // Colores de las teclas de pasos (grupos de 4)
    inline juce::Colour stepKey (int s)
    {
        static const juce::uint32 c[] = { 0xffd9432c, 0xffee8a2b, 0xfff1c23a, 0xffe9e1cb };
        return juce::Colour (c[(s / 4) % 4]);
    }
}

inline juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

class Ogro2LookAndFeel : public juce::LookAndFeel_V4
{
public:
    Ogro2LookAndFeel()
    {
        using namespace col;
        setColour (juce::ResizableWindow::backgroundColourId, body);
        setColour (juce::ComboBox::backgroundColourId, lcd);
        setColour (juce::ComboBox::outlineColourId, line);
        setColour (juce::ComboBox::textColourId, cream);
        setColour (juce::ComboBox::arrowColourId, orange);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1e1c19));
        setColour (juce::PopupMenu::textColourId, cream);
        setColour (juce::PopupMenu::headerTextColourId, orange);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, orange);
        setColour (juce::PopupMenu::highlightedTextColourId, ink);
        setColour (juce::TextButton::buttonColourId, bodyLight);
        setColour (juce::TextButton::buttonOnColourId, orange);
        setColour (juce::TextButton::textColourOffId, cream);
        setColour (juce::TextButton::textColourOnId, ink);
        setColour (juce::Label::textColourId, cream);
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff1e1c19));
        setColour (juce::TooltipWindow::textColourId, cream);
        setColour (juce::TooltipWindow::outlineColourId, line);
        setColour (juce::Slider::trackColourId, orange);
        setColour (juce::Slider::backgroundColourId, lcd);
        setColour (juce::Slider::thumbColourId, cream);
    }

    // Perilla vintage: falda negra con marcas, tapa de color y línea indicadora
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startA, float endA, juce::Slider& s) override
    {
        using namespace juce;
        const auto cap = s.findColour (Slider::rotarySliderFillColourId);
        const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
        auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
        const float R = jmin (r.getWidth(), r.getHeight()) * 0.5f;
        const auto c = r.getCentre();

        // Marcas de escala
        g.setColour (col::dim.withAlpha (0.8f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startA + (endA - startA) * (float) i / 10.0f;
            const float r0 = R * 0.86f, r1 = R * (i % 5 == 0 ? 1.0f : 0.95f);
            g.drawLine (c.x + r0 * std::sin (a), c.y - r0 * std::cos (a), c.x + r1 * std::sin (a), c.y - r1 * std::cos (a),
                        i % 5 == 0 ? 1.4f : 1.0f);
        }
        // Arco de valor fino
        const float angle = startA + pos * (endA - startA);
        const float from = bipolar ? (startA + endA) * 0.5f : startA;
        if (std::abs (angle - from) > 0.01f)
        {
            Path arc;
            arc.addCentredArc (c.x, c.y, R * 0.8f, R * 0.8f, 0.0f, jmin (from, angle), jmax (from, angle), true);
            g.setColour (cap.withAlpha (0.9f));
            g.strokePath (arc, PathStrokeType (2.0f));
        }
        // Falda
        const float skirt = R * 0.72f;
        ColourGradient sg (Colour (0xff3a3833), c.x, c.y - skirt, Colour (0xff0d0c0b), c.x, c.y + skirt, false);
        g.setGradientFill (sg);
        g.fillEllipse (c.x - skirt, c.y - skirt, skirt * 2, skirt * 2);
        // Tapa
        const float capR = skirt * 0.68f;
        ColourGradient cg (cap.brighter (0.35f), c.x - capR * 0.4f, c.y - capR * 0.6f, cap.darker (0.45f), c.x + capR, c.y + capR, true);
        g.setGradientFill (cg);
        g.fillEllipse (c.x - capR, c.y - capR, capR * 2, capR * 2);
        g.setColour (Colours::black.withAlpha (0.45f));
        g.drawEllipse (c.x - capR, c.y - capR, capR * 2, capR * 2, 1.0f);
        // Indicador
        const float i0 = capR * 0.15f, i1 = skirt * 0.96f;
        g.setColour (cap.getPerceivedBrightness() > 0.7f ? col::ink : col::cream);
        g.drawLine (c.x + i0 * std::sin (angle), c.y - i0 * std::cos (angle), c.x + i1 * std::sin (angle), c.y - i1 * std::cos (angle),
                    jmax (2.0f, R * 0.08f));
        if (s.isMouseOverOrDragging())
        {
            g.setColour (Colours::white.withAlpha (0.07f));
            g.fillEllipse (c.x - skirt, c.y - skirt, skirt * 2, skirt * 2);
        }
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle, juce::Slider& s) override
    {
        using namespace juce;
        auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (1.0f, (float) h * 0.25f);
        g.setColour (col::lcd);
        g.fillRoundedRectangle (r, 2.0f);
        const float mid = r.getCentreX();
        const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
        const auto c = s.findColour (Slider::trackColourId);
        g.setColour (c);
        if (bipolar)
            g.fillRect (Rectangle<float>::leftTopRightBottom (jmin (mid, pos), r.getY() + 1, jmax (mid, pos), r.getBottom() - 1));
        else
            g.fillRect (Rectangle<float>::leftTopRightBottom (r.getX() + 1, r.getY() + 1, pos, r.getBottom() - 1));
        g.setColour (col::cream);
        g.fillRect (pos - 1.0f, r.getY() - 2.0f, 2.0f, r.getHeight() + 4.0f);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace juce;
        auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (box.findColour (ComboBox::backgroundColourId));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (box.hasKeyboardFocus (true) ? col::orange : col::line);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        Path p;
        const float ax = (float) w - 11.0f, ay = (float) h * 0.5f;
        p.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
        g.setColour (box.findColour (ComboBox::arrowColourId));
        g.fillPath (p);
    }
    juce::Font getComboBoxFont (juce::ComboBox& b) override { return font (juce::jmin (13.0f, (float) b.getHeight() * 0.55f), true); }
    juce::Font getPopupMenuFont() override { return font (14.0f); }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (6, 1, box.getWidth() - 20, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    // Botones de goma rectangulares (con LED cuando están activos)
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool over, bool down) override
    {
        using namespace juce;
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();
        Colour c = on ? b.findColour (TextButton::buttonOnColourId) : bg;
        if (! b.isEnabled()) c = c.withMultipliedAlpha (0.4f);
        if (down) c = c.darker (0.25f); else if (over) c = c.brighter (0.1f);
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (r.translated (0, 1.5f), 3.0f);
        ColourGradient grad (c.brighter (0.15f), r.getX(), r.getY(), c.darker (0.2f), r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (Colours::black.withAlpha (0.4f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return font (juce::jmin (12.5f, (float) h * 0.5f), true); }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        using namespace juce;
        auto r = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState();
        const auto accent = b.findColour (ToggleButton::tickColourId);
        auto key = r.reduced (1.0f);
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (key.translated (0, 1.5f), 3.0f);
        g.setColour (over ? col::bodyLight.brighter (0.1f) : col::bodyLight);
        g.fillRoundedRectangle (key, 3.0f);
        if (key.getWidth() < 40.0f)
        {
            // Tecla chica (M / S): se ilumina entera
            if (on) { g.setColour (accent); g.fillRoundedRectangle (key, 3.0f); }
            g.setColour (on ? col::ink : col::dim);
            g.setFont (font (jmin (11.5f, key.getHeight() * 0.55f), true));
            g.drawText (b.getButtonText(), key, Justification::centred);
            return;
        }
        // LED
        const float d = jmin (8.0f, key.getHeight() * 0.4f);
        auto ledR = Rectangle<float> (key.getX() + 6.0f, key.getCentreY() - d * 0.5f, d, d);
        if (on)
        {
            g.setColour (accent.withAlpha (0.35f));
            g.fillEllipse (ledR.expanded (3.0f));
        }
        g.setColour (on ? accent : accent.darker (0.85f).withAlpha (0.8f));
        g.fillEllipse (ledR);
        g.setColour (on ? col::cream : col::dim);
        g.setFont (font (jmin (11.5f, key.getHeight() * 0.5f), true));
        g.drawText (b.getButtonText(), key.withTrimmedLeft (d + 10.0f).withTrimmedRight (4.0f), Justification::centredLeft);
    }
};

} // namespace ogro2::ui
