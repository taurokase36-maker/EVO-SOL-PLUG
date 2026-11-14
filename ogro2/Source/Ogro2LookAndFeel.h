// OGRO 2 — estética: una placa frontal impresa. Una sola chapa pintada verde, rótulos
// serigrafiados en crema, secciones separadas por líneas impresas (sin tarjetas), tapas de
// perilla negras y planas, y un solo acento (naranja) para lo que está encendido, acentuado
// o moviéndose. Nada brilla y nada tiene degradado (ver design/ESTETICA.md).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

namespace ogro2::ui
{
inline juce::String T (const char* utf8) { return juce::String::fromUTF8 (utf8); }

// Rótulos como se imprimen en la placa: primera letra en mayúscula y el resto en minúscula
// ("LEVEL" -> "Level", "HAT C" -> "Hat C", "FIRE FX" -> "Fire FX"). Las siglas y las letras
// sueltas quedan como están; un texto que ya viene con minúsculas no se toca.
inline juce::String legend (const juce::String& s)
{
    bool hasLower = false, hasUpper = false;
    for (auto c : s)
    {
        hasLower |= juce::CharacterFunctions::isLowerCase (c);
        hasUpper |= juce::CharacterFunctions::isUpperCase (c);
    }
    if (hasLower || ! hasUpper) return s;
    static const char* keep[] = { "FX", "MIDI", "LFO", "LFOS", "BPM", "EQ", "DJ", "FM", "VCA", "FET", "SVF", "GR", "OGRO", "EVO", "SOL", "CHD", "ABLETON" };
    juce::String out, word;
    bool first = true;
    auto flush = [&] {
        if (word.isEmpty()) return;
        bool isKept = word.length() == 1 || word.containsAnyOf ("0123456789");
        for (auto* k : keep) isKept |= word == k;
        if (isKept) out << word;
        else if (first) out << word.substring (0, 1) << word.substring (1).toLowerCase();
        else out << word.toLowerCase();
        first = false;
        word.clear();
    };
    for (auto c : s)
    {
        if (juce::CharacterFunctions::isLetterOrDigit (c)) { word << juce::String::charToString (c); continue; }
        flush();
        out << juce::String::charToString (c);
        if (c == 0xb7 || c == ':') first = true;   // después de "·" o ":" empieza otro rótulo
    }
    flush();
    return out;
}

namespace col
{
    // La placa y su tinta
    inline const juce::Colour panel { 0xff3b4730 };     // chapa pintada
    inline const juce::Colour recess { 0xff2c3524 };    // tecla apagada / hueco
    inline const juce::Colour print { 0xffefe7d2 };     // tinta crema: lo que se lee
    inline const juce::Colour print2 { 0xb8efe7d2 };    // valores y explicaciones
    inline const juce::Colour print3 { 0x80efe7d2 };    // lo apagado
    inline const juce::Colour rule { 0x57efe7d2 };      // líneas impresas
    inline const juce::Colour faint { 0x29efe7d2 };     // pistas de anillos y barras vacías
    inline const juce::Colour window { 0xff14180f };    // ventana oscura (pantallas)
    inline const juce::Colour cap { 0xff171a12 };       // tapa de perilla
    inline const juce::Colour hot { 0xfff39a2b };       // el único acento: encendido, acento, movimiento
    inline const juce::Colour onPrint { 0xff2a3322 };   // texto sobre crema
    inline const juce::Colour onHot { 0xff24180a };     // texto sobre naranja
}

// Tipografías incrustadas (licencia OFL, ver Resources/fonts)
namespace fonts
{
    inline juce::Typeface::Ptr load (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }
    inline juce::Typeface::Ptr title() { static auto t = load (BinaryData::MichromaRegular_ttf, BinaryData::MichromaRegular_ttfSize); return t; }
    inline juce::Typeface::Ptr bold() { static auto t = load (BinaryData::SpaceGroteskBold_ttf, BinaryData::SpaceGroteskBold_ttfSize); return t; }
    inline juce::Typeface::Ptr medium() { static auto t = load (BinaryData::SpaceGroteskMedium_ttf, BinaryData::SpaceGroteskMedium_ttfSize); return t; }
    inline juce::Typeface::Ptr lcd() { static auto t = load (BinaryData::VT323Regular_ttf, BinaryData::VT323Regular_ttfSize); return t; }
}

inline juce::Font titleFont (float size) { return juce::Font (juce::FontOptions (fonts::title()).withHeight (size)); }
inline juce::Font labelFont (float size) { return juce::Font (juce::FontOptions (fonts::bold()).withHeight (size)); }
inline juce::Font textFont (float size) { return juce::Font (juce::FontOptions (fonts::medium()).withHeight (size)); }
inline juce::Font lcdFont (float size) { return juce::Font (juce::FontOptions (fonts::lcd()).withHeight (size)); }

// Rótulo impreso (sin espaciado entre letras)
inline void drawLegend (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, juce::Font f, juce::Justification j)
{
    g.setFont (f);
    g.drawText (legend (s), r, j, false);
}

// ---------------------------------------------------------------------------
// Lo que se pinta a mano (lo usan el editor y los widgets). Todo plano.
// ---------------------------------------------------------------------------
namespace paint
{
    // Línea impresa de 1 px
    inline void rule (juce::Graphics& g, float x1, float y1, float x2, float y2, juce::Colour c = col::rule)
    {
        g.setColour (c);
        if (std::abs (y1 - y2) < 0.01f) g.fillRect (juce::jmin (x1, x2), y1 - 0.5f, std::abs (x2 - x1), 1.0f);
        else g.fillRect (x1 - 0.5f, juce::jmin (y1, y2), 1.0f, std::abs (y2 - y1));
    }

    // Grupo de controles: el título y una línea impresa hasta el borde derecho. Con "to", la
    // línea termina en flecha: es el camino de la señal (SENDS -> FX), la firma de la placa.
    inline void group (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, const juce::String& to = {})
    {
        const auto f = labelFont (13.0f);
        const auto text = legend (title);
        const float tw = title.isEmpty() ? 0.0f : (float) juce::GlyphArrangement::getStringWidthInt (f, text) + 10.0f;
        const float y = r.getY() + 10.0f;
        g.setColour (col::print);
        g.setFont (f);
        g.drawText (text, juce::Rectangle<float> (r.getX(), r.getY(), tw, 20.0f), juce::Justification::centredLeft, false);
        float x2 = r.getRight();
        if (to.isNotEmpty())
        {
            const auto tf = textFont (12.0f);
            const float lw = (float) juce::GlyphArrangement::getStringWidthInt (tf, to) + 6.0f;
            g.setColour (col::print2);
            g.setFont (tf);
            g.drawText (to, juce::Rectangle<float> (r.getRight() - lw, r.getY(), lw, 20.0f), juce::Justification::centredRight, false);
            x2 = r.getRight() - lw - 4.0f;
            juce::Path head;
            head.addTriangle (x2, y, x2 - 7.0f, y - 4.0f, x2 - 7.0f, y + 4.0f);
            g.setColour (col::print3);
            g.fillPath (head);
            x2 -= 7.0f;
        }
        rule (g, r.getX() + tw, y, x2, y);
    }

    // Ventana oscura donde van las cifras (preset, tempo, pasos, listas)
    inline void window (juce::Graphics& g, juce::Rectangle<float> r, float radius = 3.0f)
    {
        g.setColour (col::window);
        g.fillRoundedRectangle (r, radius);
    }

    // Cifra en una ventana
    inline void readout (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, juce::Font f,
                         juce::Justification j, juce::Colour c = col::print)
    {
        g.setFont (f);
        g.setColour (c);
        g.drawText (s, r, j, false);
    }

    // Perilla: tapa negra plana con indicador crema y anillo de valor impreso alrededor
    struct KnobStyle
    {
        bool bipolar = false;
        float modFrom = 0.0f, modTo = 0.0f;   // arco naranja (0..1): cuánto la mueve una modulación
    };

    inline void knob (juce::Graphics& g, juce::Rectangle<float> area, float pos, const KnobStyle& st, bool enabled = true)
    {
        using namespace juce;
        const float D = jmin (area.getWidth(), area.getHeight());
        const auto c = area.getCentre();
        const float R = D * 0.5f;
        const float startA = MathConstants<float>::pi * 1.25f, endA = MathConstants<float>::pi * 2.75f;
        const float ringW = jmax (2.5f, R * 0.11f), ringR = R - ringW * 0.5f - 1.0f;

        Path trk;
        trk.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, startA, endA, true);
        g.setColour (col::faint);
        g.strokePath (trk, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::butt));

        const float angle = startA + jlimit (0.0f, 1.0f, pos) * (endA - startA);
        const float from = st.bipolar ? (startA + endA) * 0.5f : startA;
        if (std::abs (angle - from) > 0.005f)
        {
            Path arc;
            arc.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, jmin (from, angle), jmax (from, angle), true);
            g.setColour (enabled ? col::print : col::print3);
            g.strokePath (arc, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::butt));
        }
        if (st.bipolar)
        {
            g.setColour (col::print);
            g.fillRect (c.x - 0.75f, c.y - R, 1.5f, ringW + 2.0f);
        }
        if (std::abs (st.modTo - st.modFrom) > 0.002f)
        {
            const float a0 = startA + jlimit (0.0f, 1.0f, st.modFrom) * (endA - startA);
            const float a1 = startA + jlimit (0.0f, 1.0f, st.modTo) * (endA - startA);
            Path m;
            m.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, jmin (a0, a1), jmax (a0, a1), true);
            g.setColour (col::hot);
            g.strokePath (m, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::butt));
        }
        const float capR = ringR - ringW * 0.5f - jmax (3.0f, R * 0.12f);
        g.setColour (col::cap);
        g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
        const float i0 = capR * 0.38f, i1 = capR * 0.86f;
        g.setColour (enabled ? col::print : col::print3);
        g.drawLine (c.x + i0 * std::sin (angle), c.y - i0 * std::cos (angle), c.x + i1 * std::sin (angle), c.y - i1 * std::cos (angle),
                    jmax (2.0f, capR * 0.14f));
    }

    // Tecla de la placa. Off: contorno impreso. On: llena en crema. Hot: llena en naranja.
    enum class Key { Off, On, Hot };
    inline void key (juce::Graphics& g, juce::Rectangle<float> r, Key k, bool over = false, float radius = 3.0f)
    {
        if (k == Key::Off)
        {
            if (over) { g.setColour (col::faint); g.fillRoundedRectangle (r, radius); }
            g.setColour (col::rule);
            g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
            return;
        }
        g.setColour (k == Key::Hot ? col::hot : col::print);
        g.fillRoundedRectangle (r, radius);
    }

    // Luz: naranja encendida, apenas marcada apagada
    inline void led (juce::Graphics& g, juce::Rectangle<float> r, bool lit)
    {
        g.setColour (lit ? col::hot : col::faint);
        g.fillRect (r);
    }
}

// ---------------------------------------------------------------------------
// Nombres de la estética anterior, llevados a la placa (los widgets los siguen usando)
// ---------------------------------------------------------------------------
namespace col
{
    inline const juce::Colour black = panel, chassis = panel, card = panel, ink = print, label = print, caption = print2, dim = print3;
    inline const juce::Colour violetText = print, violetValue = print2, violetRing = print, violet = print, violetDeep = recess, violetEdge = rule;
    inline const juce::Colour lcd = window, phosphor = print, phosphorDim = print3, phosphorOff = faint;
    inline const juce::Colour keyCream = recess, keyBlue = recess, orange = hot, red = hot, yellow = print, led = hot, ledGreen = hot, ledOff = faint;
    inline const juce::Colour discRing = print;
    inline juce::Colour track (int) { return print; }
    inline juce::Colour stepKey (int) { return recess; }
}
inline juce::String upper (const juce::String& s) { return legend (s); }
inline void drawSpaced (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, juce::Font f, float, juce::Justification j) { drawLegend (g, s, r, f, j); }
namespace paint
{
    inline void card (juce::Graphics&, juce::Rectangle<float>, float = 0.0f) {}
    inline void violetCard (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title = {})
    {
        if (title.isNotEmpty()) group (g, r.reduced (10.0f, 0.0f).translated (0.0f, 4.0f), title, legend (title) == "Sends" ? "FX" : juce::String());
    }
    inline void lcd (juce::Graphics& g, juce::Rectangle<float> r, float = 3.0f) { window (g, r); }
    inline void glowText (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, juce::Font f, juce::Justification j, juce::Colour c = col::print) { readout (g, s, r, f, j, c); }
    inline void copperTrace (juce::Graphics&, juce::Point<float>, juce::Point<float>, float = 0.0f) {}
    // Tecla de paso: apagada = hueco, encendida = crema, o el color que se pida (acento / fill)
    inline void key (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, bool down, bool, float = 3.0f)
    {
        g.setColour (down && c == col::recess ? col::print : c);
        g.fillRoundedRectangle (r, 3.0f);
        if (! down) { g.setColour (juce::Colours::black.withAlpha (0.3f)); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f); }
    }
    inline void led (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour, bool lit, juce::Colour = {}) { led (g, r, lit); }
}

// ---------------------------------------------------------------------------
// LookAndFeel: botones, listas, menús y deslizadores de la placa
// ---------------------------------------------------------------------------
// Un botón "hot" (acción principal de su zona) va lleno en naranja
inline void makeHot (juce::Button& b) { b.getProperties().set ("hot", true); }
// Una pestaña: sin contorno, subrayada cuando está elegida
inline void makeTab (juce::Button& b) { b.getProperties().set ("tab", true); }

class Ogro2LookAndFeel : public juce::LookAndFeel_V4
{
public:
    Ogro2LookAndFeel()
    {
        using namespace col;
        setColour (juce::ResizableWindow::backgroundColourId, panel);
        setColour (juce::ComboBox::backgroundColourId, window);
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::textColourId, print);
        setColour (juce::ComboBox::arrowColourId, print3);
        setColour (juce::PopupMenu::backgroundColourId, window);
        setColour (juce::PopupMenu::textColourId, print);
        setColour (juce::PopupMenu::headerTextColourId, print3);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, print);
        setColour (juce::PopupMenu::highlightedTextColourId, onPrint);
        setColour (juce::TextButton::textColourOffId, print);
        setColour (juce::TextButton::textColourOnId, onPrint);
        setColour (juce::Label::textColourId, print);
        setColour (juce::TooltipWindow::backgroundColourId, window);
        setColour (juce::TooltipWindow::textColourId, print);
        setColour (juce::TooltipWindow::outlineColourId, rule);
        setColour (juce::Slider::trackColourId, print);
        setColour (juce::Slider::backgroundColourId, faint);
        setColour (juce::Slider::thumbColourId, print);
        setColour (juce::Slider::textBoxTextColourId, print2);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ScrollBar::thumbColourId, print3);
        setColour (juce::BubbleComponent::backgroundColourId, panel);
        setColour (juce::BubbleComponent::outlineColourId, rule);
        setColour (juce::AlertWindow::backgroundColourId, panel);
        setColour (juce::AlertWindow::textColourId, print);
        setColour (juce::AlertWindow::outlineColourId, rule);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider& s) override
    {
        paint::KnobStyle st;
        st.bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
        st.modFrom = (float) (double) s.getProperties().getWithDefault ("modFrom", 0.0);
        st.modTo = (float) (double) s.getProperties().getWithDefault ("modTo", 0.0);
        paint::knob (g, juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h), pos, st, s.isEnabled());
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle, juce::Slider& s) override
    {
        using namespace juce;
        auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).withSizeKeepingCentre ((float) w, 4.0f);
        g.setColour (col::faint);
        g.fillRect (r);
        const float mid = r.getCentreX();
        const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
        const auto ink = s.isEnabled() ? col::print : col::print3;
        g.setColour (ink);
        if (bipolar) g.fillRect (Rectangle<float>::leftTopRightBottom (jmin (mid, pos), r.getY(), jmax (mid, pos), r.getBottom()));
        else g.fillRect (Rectangle<float>::leftTopRightBottom (r.getX(), r.getY(), pos, r.getBottom()));
        g.setColour (col::cap);
        g.fillRect (pos - 4.0f, r.getCentreY() - 8.0f, 8.0f, 16.0f);
        g.setColour (ink);
        g.fillRect (pos - 1.0f, r.getCentreY() - 6.0f, 2.0f, 12.0f);
    }

    // Listas: una ventana oscura con la cifra y una flechita
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        paint::window (g, juce::Rectangle<float> (0, 0, (float) w, (float) h));
        juce::Path p;
        const float ax = (float) w - 11.0f, ay = (float) h * 0.5f;
        p.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.fillPath (p);
    }
    juce::Font getComboBoxFont (juce::ComboBox& b) override
    {
        const float maxF = (float) (double) b.getProperties().getWithDefault ("maxFont", 24.0);
        return lcdFont (juce::jlimit (16.0f, maxF, (float) b.getHeight() * 0.8f));
    }
    juce::Font getPopupMenuFont() override { return textFont (15.0f); }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 0, box.getWidth() - 22, box.getHeight());
        label.setFont (getComboBoxFont (box));
        label.setColour (juce::Label::textColourId, col::print);
    }
    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
        g.setColour (col::rule);
        g.drawRect (0, 0, w, h, 1);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat();
        const bool on = b.getToggleState();
        const float alpha = b.isEnabled() ? 1.0f : 0.4f;
        if ((bool) b.getProperties().getWithDefault ("tab", false))
        {
            if (over && ! on) { g.setColour (col::faint); g.fillRect (r.withTrimmedBottom (2.0f)); }
            g.setColour ((on ? col::print : col::faint).withMultipliedAlpha (alpha));
            g.fillRect (r.removeFromBottom (2.0f));
            return;
        }
        const bool hot = (bool) b.getProperties().getWithDefault ("hot", false);
        g.setOpacity (alpha);
        g.beginTransparencyLayer (alpha);
        paint::key (g, down ? r.reduced (1.0f) : r, hot ? paint::Key::Hot : (on ? paint::Key::On : paint::Key::Off), over);
        g.endTransparencyLayer();
    }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return labelFont (juce::jmin (13.0f, (float) h * 0.5f)); }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const bool tab = (bool) b.getProperties().getWithDefault ("tab", false);
        const bool hot = (bool) b.getProperties().getWithDefault ("hot", false);
        const bool on = b.getToggleState();
        juce::Colour c = hot ? col::onHot : (on && ! tab ? col::onPrint : (tab && ! on ? col::print3 : col::print));
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.setColour (c.withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.4f));
        g.drawFittedText (legend (b.getButtonText()), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1, 0.8f);
    }

    // Toggle: tecla con luz. Las chiquitas (M, S) se llenan enteras al encenderse.
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat();
        if (down) r = r.reduced (1.0f);
        const bool on = b.getToggleState();
        const bool small = r.getWidth() < 40.0f;
        if (small)
        {
            const bool hot = (bool) b.getProperties().getWithDefault ("hot", false);
            paint::key (g, r, on ? (hot ? paint::Key::Hot : paint::Key::On) : paint::Key::Off, over, 2.0f);
            g.setColour (on ? (hot ? col::onHot : col::onPrint) : col::print2);
            g.setFont (labelFont (juce::jmin (12.0f, r.getHeight() * 0.6f)));
            g.drawText (b.getButtonText(), r, juce::Justification::centred);
            return;
        }
        paint::key (g, r, paint::Key::Off, over);
        paint::led (g, { r.getX() + 9.0f, r.getCentreY() - 3.0f, 10.0f, 6.0f }, on);
        g.setColour (on ? col::print : col::print2);
        g.setFont (labelFont (juce::jmin (13.0f, r.getHeight() * 0.5f)));
        g.drawFittedText (legend (b.getButtonText()), r.withTrimmedLeft (26.0f).withTrimmedRight (6.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h) override
    {
        g.fillAll (findColour (juce::TooltipWindow::backgroundColourId));
        g.setColour (col::rule);
        g.drawRect (0, 0, w, h, 1);
        g.setColour (col::print);
        g.setFont (textFont (14.0f));
        g.drawFittedText (text, juce::Rectangle<int> (w, h).reduced (8, 4), juce::Justification::centredLeft, 4);
    }
    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent) override
    {
        const int w = juce::jmin (360, juce::GlyphArrangement::getStringWidthInt (textFont (14.0f), text) + 20);
        const int lines = juce::jmax (1, juce::GlyphArrangement::getStringWidthInt (textFont (14.0f), text) / 340 + 1);
        const int h = 10 + 18 * lines;
        return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 24,
                                     pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 6, w, h)
            .constrainedWithin (parent);
    }
};

} // namespace ogro2::ui
