// OGRO 2 — estética: aparato de plástico crema con grilla, pantallas de fósforo
// verde, ventana de plástico violeta translúcido, perillas cromadas, cobre y
// destellos rojo/amarillo (ver design/ESTETICA.md y design/maqueta/).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

namespace ogro2::ui
{
inline juce::String T (const char* utf8) { return juce::String::fromUTF8 (utf8); }

// Mayúsculas que no dependen del idioma del sistema (towupper no siempre convierte "á" o "ñ")
inline juce::String upper (const juce::String& s)
{
    juce::String out;
    for (auto c : s)
    {
        switch (c)
        {
            case 0xe1: c = 0xc1; break; case 0xe9: c = 0xc9; break; case 0xed: c = 0xcd; break;
            case 0xf3: c = 0xd3; break; case 0xfa: c = 0xda; break; case 0xf1: c = 0xd1; break;
            case 0xfc: c = 0xdc; break;
            default: if (c >= 'a' && c <= 'z') c = (juce::juce_wchar) (c - 'a' + 'A');
        }
        out += juce::String::charToString (c);
    }
    return out;
}

namespace col
{
    // Aparato
    inline const juce::Colour black { 0xff07060a };
    inline const juce::Colour chassis { 0xffece8de };
    inline const juce::Colour grid { 0x0d281e3c };
    inline const juce::Colour card { 0xffe3ddd0 };
    inline const juce::Colour ink { 0xff1e1a24 };
    inline const juce::Colour label { 0xff2a2531 };
    inline const juce::Colour caption { 0xff5a5262 };
    inline const juce::Colour dim { 0xff6a6272 };
    // Ventana violeta
    inline const juce::Colour violetTop { 0xf0542cb8 };
    inline const juce::Colour violetBottom { 0xf52c106c };
    inline const juce::Colour violetEdge { 0x80c8aeff };
    inline const juce::Colour violetCard { 0x6b12062e };
    inline const juce::Colour violetText { 0xfff3edff };
    inline const juce::Colour violetValue { 0xffcdbeff };
    inline const juce::Colour violetRing { 0xffc9aeff };
    inline const juce::Colour violet { 0xff6b3ff0 };
    inline const juce::Colour violetDeep { 0xff3a1c8a };
    // Fósforo
    inline const juce::Colour lcd { 0xff121807 };
    inline const juce::Colour phosphor { 0xffd8f55a };
    inline const juce::Colour phosphorDim { 0xff93ad36 };
    inline const juce::Colour phosphorOff { 0xff33401a };
    // Teclas, LEDs y acentos
    inline const juce::Colour keyCream { 0xfff4f1ea };
    inline const juce::Colour keyBlue { 0xffafc6df };
    inline const juce::Colour orange { 0xfff08a3c };
    inline const juce::Colour red { 0xffe5391c };
    inline const juce::Colour yellow { 0xfff6c42f };
    inline const juce::Colour led { 0xffe53b1e };
    inline const juce::Colour ledGreen { 0xffc9e84a };
    inline const juce::Colour ledOff { 0xffb9b3a5 };
    inline const juce::Colour copperHi { 0xfff8d0a3 };
    inline const juce::Colour copper { 0xffb6622c };
    inline const juce::Colour copperLo { 0xff7a3913 };
    inline const juce::Colour discRing { 0xffa57cff };

    inline juce::Colour track (int t)
    {
        static const juce::uint32 c[] = { 0xffe0533b, 0xfff08a3c, 0xffe8b730, 0xff7fa8d1, 0xff5e8cc0,
                                          0xff4fb08c, 0xffb48cff, 0xffe58fd0, 0xff5ec4e8, 0xffb59e6a };
        return juce::Colour (c[juce::jlimit (0, 9, t)]);
    }
    // Teclas de pasos en grupos de 4, como un teclado de calculadora
    inline juce::Colour stepKey (int s) { return (s / 4) % 2 == 0 ? keyBlue : keyCream; }
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
// Compatibilidad con el código de la 1.0
inline juce::Font font (float size, bool bold = false) { return bold ? labelFont (size) : textFont (size); }

// Texto con espaciado entre letras (rótulos en mayúscula)
inline void drawSpaced (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, juce::Font f, float tracking,
                        juce::Justification j)
{
    f.setExtraKerningFactor (tracking);
    g.setFont (f);
    g.drawText (s, r, j, false);
}

// ---------------------------------------------------------------------------
// Materiales que se pintan a mano (los usan el editor y los widgets)
// ---------------------------------------------------------------------------
namespace paint
{
    // Panel hundido del aparato
    inline void card (juce::Graphics& g, juce::Rectangle<float> r, float radius = 14.0f)
    {
        g.setColour (col::card);
        g.fillRoundedRectangle (r, radius);
        g.setColour (juce::Colours::black.withAlpha (0.10f));
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawLine (r.getX() + radius, r.getBottom() + 0.5f, r.getRight() - radius, r.getBottom() + 0.5f, 1.0f);
    }

    // Ventana de plástico violeta translúcido con el circuito apenas visible
    inline void violetWindow (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const float rad = 18.0f;
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (juce::Colour (0xff4620b4).withAlpha (0.06f * (float) i));
            g.fillRoundedRectangle (r.translated (0, 4.0f * (float) i).expanded (2.0f * (float) i), rad + 2.0f * (float) i);
        }
        juce::ColourGradient grad (col::violetTop, r.getX(), r.getY(), col::violetBottom, r.getRight(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, rad);
        g.saveState();
        g.reduceClipRegion (r.toNearestInt());
        juce::Path circuit;
        const float x0 = r.getX(), y0 = r.getY(), w = r.getWidth(), h = r.getHeight();
        circuit.startNewSubPath (x0, y0 + 64); circuit.lineTo (x0 + w * 0.26f, y0 + 64); circuit.lineTo (x0 + w * 0.26f, y0 + 112);
        circuit.lineTo (x0 + w * 0.51f, y0 + 112); circuit.lineTo (x0 + w * 0.51f, y0 + 40); circuit.lineTo (x0 + w, y0 + 40);
        circuit.startNewSubPath (x0 + 40, y0 + h); circuit.lineTo (x0 + 40, y0 + h - 82); circuit.lineTo (x0 + w * 0.39f, y0 + h - 82);
        circuit.lineTo (x0 + w * 0.39f, y0 + h - 40); circuit.lineTo (x0 + w * 0.8f, y0 + h - 40); circuit.lineTo (x0 + w * 0.8f, y0 + h - 120);
        circuit.lineTo (x0 + w, y0 + h - 120);
        circuit.startNewSubPath (x0 + w * 0.68f, y0); circuit.lineTo (x0 + w * 0.68f, y0 + 150); circuit.lineTo (x0 + w * 0.94f, y0 + 150);
        circuit.lineTo (x0 + w * 0.94f, y0 + 230);
        g.setColour (juce::Colour (0xffd2c1ff).withAlpha (0.14f));
        g.strokePath (circuit, juce::PathStrokeType (2.0f));
        for (auto p : { juce::Point<float> (x0 + w * 0.51f, y0 + 40), juce::Point<float> (x0 + w * 0.39f, y0 + h - 82),
                        juce::Point<float> (x0 + w * 0.94f, y0 + 230) })
            g.fillEllipse (p.x - 5, p.y - 5, 10, 10);
        g.restoreState();
        g.setColour (col::violetEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.drawLine (r.getX() + rad, r.getY() + 1.5f, r.getRight() - rad, r.getY() + 1.5f, 1.0f);
    }

    // Tarjeta dentro de la ventana violeta
    inline void violetCard (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title = {})
    {
        g.setColour (col::violetCard);
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (col::violetEdge.withAlpha (0.18f));
        g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
        if (title.isNotEmpty())
        {
            g.setColour (col::violetText);
            drawSpaced (g, upper (title), { r.getX() + 12.0f, r.getY() + 9.0f, r.getWidth() - 24.0f, 16.0f },
                        labelFont (13.0f), 0.08f, juce::Justification::centredLeft);
        }
    }

    // Pantalla de fósforo hundida
    inline void lcd (juce::Graphics& g, juce::Rectangle<float> r, float radius = 8.0f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.fillRoundedRectangle (r.translated (0, 1.0f), radius);
        g.setColour (col::lcd);
        g.fillRoundedRectangle (r, radius);
        juce::ColourGradient inner (juce::Colours::black.withAlpha (0.55f), r.getX(), r.getY(),
                                    juce::Colours::transparentBlack, r.getX(), r.getY() + juce::jmin (14.0f, r.getHeight() * 0.5f), false);
        g.setGradientFill (inner);
        g.fillRoundedRectangle (r, radius);
        g.setColour (col::phosphor.withAlpha (0.10f));
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    }

    // Texto de fósforo con brillo
    inline void glowText (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, juce::Font f,
                          juce::Justification j, juce::Colour c = col::phosphor)
    {
        g.setFont (f);
        g.setColour (c.withAlpha (0.22f));
        for (auto o : { juce::Point<float> (-1, 0), juce::Point<float> (1, 0), juce::Point<float> (0, -1), juce::Point<float> (0, 1) })
            g.drawText (s, r.translated (o.x, o.y), j, false);
        g.setColour (c);
        g.drawText (s, r, j, false);
    }

    // Caño de cobre con conexiones de bronce
    inline void copperTube (juce::Graphics& g, juce::Rectangle<float> r)
    {
        auto brass = [&g] (juce::Rectangle<float> b) {
            juce::ColourGradient gb (juce::Colour (0xfff4e6ae), b.getX(), b.getY(), juce::Colour (0xff7b6224), b.getX(), b.getBottom(), false);
            gb.addColour (0.55, juce::Colour (0xffba983f));
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (b.translated (0, 1.0f), 3.0f);
            g.setGradientFill (gb);
            g.fillRoundedRectangle (b, 3.0f);
        };
        auto tube = r.withTrimmedLeft (14.0f).withTrimmedRight (14.0f).withSizeKeepingCentre (r.getWidth() - 28.0f, r.getHeight() * 0.62f);
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillRoundedRectangle (tube.translated (0, 2.0f), tube.getHeight() * 0.5f);
        juce::ColourGradient gc (col::copperHi, 0, tube.getY(), juce::Colour (0xffda8c50), 0, tube.getBottom(), false);
        gc.addColour (0.22, juce::Colour (0xffe2945a));
        gc.addColour (0.55, col::copper);
        gc.addColour (0.82, col::copperLo);
        g.setGradientFill (gc);
        g.fillRoundedRectangle (tube, tube.getHeight() * 0.5f);
        brass (r.withWidth (14.0f));
        brass (r.withX (r.getRight() - 14.0f).withWidth (14.0f));
    }

    // Traza de cobre fina (camino de la señal)
    inline void copperTrace (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, float thickness = 4.0f)
    {
        juce::Path p;
        p.startNewSubPath (a);
        p.lineTo (b);
        juce::ColourGradient gc (col::copperHi, 0, a.y - thickness, col::copperLo, 0, a.y + thickness, false);
        gc.addColour (0.5, col::copper);
        g.setGradientFill (gc);
        g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Destello de 4 puntas
    inline juce::Path star (juce::Point<float> c, float outer, float inner)
    {
        juce::Path p;
        p.startNewSubPath (c.x, c.y - outer);
        p.lineTo (c.x + inner, c.y - inner); p.lineTo (c.x + outer, c.y); p.lineTo (c.x + inner, c.y + inner);
        p.lineTo (c.x, c.y + outer); p.lineTo (c.x - inner, c.y + inner); p.lineTo (c.x - outer, c.y);
        p.lineTo (c.x - inner, c.y - inner); p.closeSubPath();
        return p;
    }

    // Perilla cromada con anillo de valor (y arco de modulación opcional)
    struct KnobStyle
    {
        juce::Colour ring = col::violetRing, track = juce::Colours::white.withAlpha (0.13f);
        bool bipolar = false, glow = true;
        float modFrom = 0.0f, modTo = 0.0f; // arco externo (0..1) que muestra una macro o modulación
        juce::Colour modColour = juce::Colour (0xffff5a3c);
    };

    inline void chromeKnob (juce::Graphics& g, juce::Rectangle<float> area, float pos, const KnobStyle& st)
    {
        using namespace juce;
        const float D = jmin (area.getWidth(), area.getHeight());
        const auto c = area.getCentre();
        const float R = D * 0.5f;
        const float startA = MathConstants<float>::pi * 1.25f, endA = MathConstants<float>::pi * 2.75f;
        const float ringR = R * 0.86f, ringW = jmax (2.5f, R * 0.13f);

        // Pista del anillo
        Path trk;
        trk.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, startA, endA, true);
        g.setColour (st.track);
        g.strokePath (trk, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::butt));
        // Valor
        const float angle = startA + jlimit (0.0f, 1.0f, pos) * (endA - startA);
        const float from = st.bipolar ? (startA + endA) * 0.5f : startA;
        if (std::abs (angle - from) > 0.005f)
        {
            Path arc;
            arc.addCentredArc (c.x, c.y, ringR, ringR, 0.0f, jmin (from, angle), jmax (from, angle), true);
            if (st.glow)
                for (int i = 3; i >= 1; --i)
                {
                    g.setColour (st.ring.withAlpha (0.09f));
                    g.strokePath (arc, PathStrokeType (ringW + 3.0f * (float) i, PathStrokeType::curved, PathStrokeType::butt));
                }
            g.setColour (st.ring);
            g.strokePath (arc, PathStrokeType (ringW, PathStrokeType::curved, PathStrokeType::butt));
        }
        if (st.bipolar)
        {
            g.setColour (col::ink.withAlpha (0.75f));
            g.fillRect (c.x - 1.0f, c.y - R + 0.5f, 2.0f, ringW * 0.7f);
        }
        // Arco de modulación (afuera del anillo)
        if (std::abs (st.modTo - st.modFrom) > 0.002f)
        {
            const float a0 = startA + jlimit (0.0f, 1.0f, st.modFrom) * (endA - startA);
            const float a1 = startA + jlimit (0.0f, 1.0f, st.modTo) * (endA - startA);
            Path m;
            const float mr = R * 1.02f;
            m.addCentredArc (c.x, c.y, mr, mr, 0.0f, jmin (a0, a1), jmax (a0, a1), true);
            g.setColour (st.modColour.withAlpha (0.35f));
            g.strokePath (m, PathStrokeType (4.5f));
            g.setColour (st.modColour);
            g.strokePath (m, PathStrokeType (2.2f));
        }
        // Sombra
        const float capR = R * 0.64f;
        g.setColour (Colours::black.withAlpha (0.28f));
        g.fillEllipse (c.x - capR, c.y - capR + capR * 0.16f, capR * 2, capR * 2);
        g.setColour (Colours::black.withAlpha (0.18f));
        g.fillEllipse (c.x - capR * 1.04f, c.y - capR + capR * 0.26f, capR * 2.08f, capR * 2.08f);
        // Cromo
        ColourGradient chrome (Colours::white, c.x - capR * 0.32f, c.y - capR * 0.44f,
                               Colour (0xff2b2b35), c.x + capR * 0.9f, c.y + capR * 0.9f, true);
        chrome.addColour (0.13, Colour (0xffefeff4));
        chrome.addColour (0.36, Colour (0xffbbbbc6));
        chrome.addColour (0.64, Colour (0xff6c6c7a));
        g.setGradientFill (chrome);
        g.fillEllipse (c.x - capR, c.y - capR, capR * 2, capR * 2);
        g.setColour (Colours::black.withAlpha (0.35f));
        g.drawEllipse (c.x - capR, c.y - capR, capR * 2, capR * 2, 0.8f);
        // Indicador
        const float i0 = capR * 0.18f, i1 = capR * 0.82f;
        Path ind;
        ind.startNewSubPath (c.x + i0 * std::sin (angle), c.y - i0 * std::cos (angle));
        ind.lineTo (c.x + i1 * std::sin (angle), c.y - i1 * std::cos (angle));
        g.setColour (Colour (0xff17131d));
        g.strokePath (ind, PathStrokeType (jmax (2.0f, capR * 0.12f), PathStrokeType::curved, PathStrokeType::rounded));
    }

    // Tecla tipo calculadora
    inline void key (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, bool down, bool over, float radius = 7.0f)
    {
        if (! down)
        {
            g.setColour (juce::Colours::black.withAlpha (0.22f));
            g.fillRoundedRectangle (r.translated (0, 2.0f), radius);
        }
        auto k = down ? r.translated (0, 1.5f) : r;
        juce::Colour base = over ? c.brighter (0.06f) : c;
        juce::ColourGradient grad (base.brighter (0.08f), 0, k.getY(), base.darker (0.06f), 0, k.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (k, radius);
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.drawLine (k.getX() + radius, k.getY() + 1.0f, k.getRight() - radius, k.getY() + 1.0f, 1.0f);
    }

    // LED rectangular
    inline void led (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour on, bool lit, juce::Colour off = col::ledOff)
    {
        if (lit)
        {
            g.setColour (on.withAlpha (0.3f));
            g.fillRoundedRectangle (r.expanded (3.0f), 4.0f);
        }
        g.setColour (lit ? on : off);
        g.fillRoundedRectangle (r, 2.0f);
    }
}

// ---------------------------------------------------------------------------
// LookAndFeel: botones, combos y menús con la estética nueva
// ---------------------------------------------------------------------------
class Ogro2LookAndFeel : public juce::LookAndFeel_V4
{
public:
    Ogro2LookAndFeel()
    {
        using namespace col;
        setColour (juce::ResizableWindow::backgroundColourId, black);
        setColour (juce::ComboBox::backgroundColourId, lcd);
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::textColourId, phosphor);
        setColour (juce::ComboBox::arrowColourId, phosphorDim);
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1b1424));
        setColour (juce::PopupMenu::textColourId, violetText);
        setColour (juce::PopupMenu::headerTextColourId, violetRing);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, violet);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextButton::buttonColourId, keyCream);
        setColour (juce::TextButton::buttonOnColourId, ink);
        setColour (juce::TextButton::textColourOffId, ink);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::Label::textColourId, ink);
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff1b1424));
        setColour (juce::TooltipWindow::textColourId, violetText);
        setColour (juce::TooltipWindow::outlineColourId, violetEdge);
        setColour (juce::Slider::trackColourId, violet);
        setColour (juce::Slider::backgroundColourId, lcd);
        setColour (juce::Slider::thumbColourId, violetText);
        setColour (juce::Slider::rotarySliderFillColourId, violetRing);
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::white.withAlpha (0.13f));
        setColour (juce::ScrollBar::thumbColourId, violetRing);
    }

    // Las perillas se dibujan con paint::chromeKnob; el color del anillo y la pista
    // salen de los colores del slider, y "bipolar" de sus propiedades.
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider& s) override
    {
        paint::KnobStyle st;
        st.ring = s.findColour (juce::Slider::rotarySliderFillColourId);
        st.track = s.findColour (juce::Slider::rotarySliderOutlineColourId);
        st.bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
        st.glow = (bool) s.getProperties().getWithDefault ("glow", true);
        st.modFrom = (float) (double) s.getProperties().getWithDefault ("modFrom", 0.0);
        st.modTo = (float) (double) s.getProperties().getWithDefault ("modTo", 0.0);
        if (s.getProperties().contains ("modColour"))
            st.modColour = juce::Colour ((juce::uint32) (juce::int64) s.getProperties()["modColour"]);
        paint::chromeKnob (g, juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h), pos, st);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                           juce::Slider::SliderStyle, juce::Slider& s) override
    {
        using namespace juce;
        auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).withSizeKeepingCentre ((float) w, 8.0f);
        g.setColour (Colours::black.withAlpha (0.28f));
        g.fillRoundedRectangle (r, 4.0f);
        const float mid = r.getCentreX();
        const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
        const auto c = s.findColour (Slider::trackColourId);
        g.setColour (c);
        if (bipolar)
            g.fillRoundedRectangle (Rectangle<float>::leftTopRightBottom (jmin (mid, pos), r.getY(), jmax (mid, pos), r.getBottom()), 4.0f);
        else
            g.fillRoundedRectangle (Rectangle<float>::leftTopRightBottom (r.getX(), r.getY(), pos, r.getBottom()), 4.0f);
        g.setColour (Colours::white);
        g.fillEllipse (pos - 7.0f, r.getCentreY() - 7.0f, 14.0f, 14.0f);
        g.setColour (Colours::black.withAlpha (0.3f));
        g.drawEllipse (pos - 7.0f, r.getCentreY() - 7.0f, 14.0f, 14.0f, 1.0f);
    }

    // Combos como pantallas de fósforo
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
        paint::lcd (g, r, 6.0f);
        juce::Path p;
        const float ax = (float) w - 11.0f, ay = (float) h * 0.5f;
        p.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.fillPath (p);
    }
    juce::Font getComboBoxFont (juce::ComboBox& b) override { return lcdFont (juce::jlimit (16.0f, 26.0f, (float) b.getHeight() * 0.8f)); }
    juce::Font getPopupMenuFont() override { return textFont (15.0f); }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 0, box.getWidth() - 22, box.getHeight());
        label.setFont (getComboBoxFont (box));
        label.setColour (juce::Label::textColourId, col::phosphor);
    }
    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
        g.setColour (col::violetEdge);
        g.drawRect (0, 0, w, h, 1);
    }

    // Teclas tipo calculadora
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f, 1.0f).withTrimmedBottom (2.0f);
        const bool on = b.getToggleState();
        juce::Colour c = on ? b.findColour (juce::TextButton::buttonOnColourId) : bg;
        if (! b.isEnabled()) c = c.withMultipliedAlpha (0.45f);
        if ((bool) b.getProperties().getWithDefault ("flat", false) && ! on)
        {
            if (over) { g.setColour (juce::Colours::black.withAlpha (0.06f)); g.fillRoundedRectangle (r, 7.0f); }
            return;
        }
        paint::key (g, r, c, down || on, over, juce::jmin (7.0f, r.getHeight() * 0.3f));
    }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return labelFont (juce::jmin (13.0f, (float) h * 0.48f)); }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down) override
    {
        auto f = getTextButtonFont (b, b.getHeight());
        f.setExtraKerningFactor (0.05f);
        g.setFont (f);
        g.setColour (b.findColour (b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)
                         .withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.5f));
        auto r = b.getLocalBounds().withTrimmedBottom (3);
        if (down || b.getToggleState()) r.translate (0, 1);
        g.drawFittedText (b.getButtonText(), r.reduced (4, 0), juce::Justification::centred, 1, 0.8f);
    }

    // Toggle: tecla con LED
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f, 1.0f).withTrimmedBottom (2.0f);
        const bool on = b.getToggleState();
        const auto accent = b.findColour (juce::ToggleButton::tickColourId);
        const bool small = r.getWidth() < 40.0f;
        const auto bg = small && on ? accent : juce::Colour ((juce::uint32) (juce::int64) b.getProperties().getWithDefault ("keyColour", (juce::int64) col::keyCream.getARGB()));
        paint::key (g, r, bg, down || on, over, juce::jmin (6.0f, r.getHeight() * 0.3f));
        auto k = (down || on) ? r.translated (0, 1.5f) : r;
        if (small)
        {
            g.setColour (on ? juce::Colours::white : col::ink);
            g.setFont (labelFont (juce::jmin (12.0f, k.getHeight() * 0.6f)));
            g.drawText (b.getButtonText(), k, juce::Justification::centred);
            return;
        }
        auto ledR = juce::Rectangle<float> (k.getX() + 9.0f, k.getCentreY() - 3.5f, 12.0f, 7.0f);
        paint::led (g, ledR, accent, on);
        g.setColour (col::ink);
        auto f = labelFont (juce::jmin (13.0f, k.getHeight() * 0.48f));
        f.setExtraKerningFactor (0.05f);
        g.setFont (f);
        g.drawFittedText (b.getButtonText(), k.withTrimmedLeft (28.0f).withTrimmedRight (6.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h) override
    {
        g.fillAll (findColour (juce::TooltipWindow::backgroundColourId));
        g.setColour (col::violetEdge);
        g.drawRect (0, 0, w, h, 1);
        g.setColour (col::violetText);
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
