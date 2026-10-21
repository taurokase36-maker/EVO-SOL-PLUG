// OGRO 2 — componentes de interfaz
#pragma once

#include "Ogro2LookAndFeel.h"
#include "PluginProcessor.h"

namespace ogro2::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

enum class Theme { Violet, Cream };

// ---------------------------------------------------------------------------
// Slider rotativo con ajuste fino: Shift (o Cmd) al arrastrar = 5 veces más preciso
// ---------------------------------------------------------------------------
class FineSlider : public juce::Slider
{
public:
    FineSlider() : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox)
    {
        setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        setMouseDragSensitivity (coarse);
    }
    void setTravel (int pixels) { coarse = pixels; setMouseDragSensitivity (coarse); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        setMouseDragSensitivity (e.mods.isShiftDown() || e.mods.isCommandDown() ? coarse * 5 : coarse);
        juce::Slider::mouseDown (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const int want = e.mods.isShiftDown() || e.mods.isCommandDown() ? coarse * 5 : coarse;
        if (getMouseDragSensitivity() != want) setMouseDragSensitivity (want);
        juce::Slider::mouseDrag (e);
    }

private:
    int coarse = 260;
};

// ---------------------------------------------------------------------------
// Perilla cromada con título arriba y valor abajo (se puede re-enlazar)
// ---------------------------------------------------------------------------
class Knob : public juce::Component
{
public:
    Knob (Ogro2Processor& p, Theme th = Theme::Violet) : proc (p), theme (th)
    {
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (slider);
        setRing (theme == Theme::Violet ? col::violetRing : col::violet);
    }
    Knob (Ogro2Processor& p, int paramIndex, const juce::String& t, Theme th = Theme::Violet, bool bipolar = false)
        : Knob (p, th)
    {
        setBipolar (bipolar);
        bind (paramIndex, t);
    }

    void bind (int paramIndex, const juce::String& t)
    {
        attachment.reset();
        title = t;
        index = paramIndex;
        const auto& d = paramDefs()[(size_t) paramIndex];
        attachment = std::make_unique<APVTS::SliderAttachment> (proc.state, juce::String (d.id), slider);
        slider.setDoubleClickReturnValue (true, d.def);
        slider.setTooltip (juce::String::fromUTF8 (d.name.c_str()) + T (" · doble clic: valor inicial · Shift: ajuste fino"));
        repaint();
    }
    void setRing (juce::Colour c)
    {
        slider.setColour (juce::Slider::rotarySliderFillColourId, c);
        slider.setColour (juce::Slider::rotarySliderOutlineColourId,
                          theme == Theme::Violet ? juce::Colours::white.withAlpha (0.13f) : col::ink.withAlpha (0.13f));
        slider.repaint();
    }
    void setBipolar (bool b) { slider.getProperties().set ("bipolar", b); slider.repaint(); }
    void setTitleSize (float s) { titleSize = s; repaint(); }
    void setShowValue (bool v) { showValue = v; resized(); repaint(); }
    void setTravel (int px) { slider.setTravel (px); }
    int paramIndex() const { return index; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const bool v = theme == Theme::Violet;
        g.setColour (v ? col::violetText : col::label);
        if (title.isNotEmpty())
            drawSpaced (g, upper (title), r.removeFromTop (titleSize + 4.0f), labelFont (titleSize), 0.06f, juce::Justification::centred);
        if (showValue)
        {
            g.setColour (v ? col::violetValue : col::caption);
            g.setFont (textFont (titleSize));
            g.drawText (slider.getTextFromValue (slider.getValue()), r.removeFromBottom (titleSize + 4.0f), juce::Justification::centred);
        }
    }
    void resized() override
    {
        auto r = getLocalBounds();
        if (title.isNotEmpty()) r.removeFromTop ((int) titleSize + 6);
        if (showValue) r.removeFromBottom ((int) titleSize + 6);
        const int s = juce::jmin (r.getWidth(), r.getHeight());
        slider.setBounds (r.withSizeKeepingCentre (s, s));
    }

    FineSlider slider;

private:
    Ogro2Processor& proc;
    Theme theme;
    juce::String title;
    int index = -1;
    float titleSize = 13.0f;
    bool showValue = true;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// ---------------------------------------------------------------------------
// Combo de fósforo enlazado a un parámetro de elección
// ---------------------------------------------------------------------------
class Choice : public juce::Component
{
public:
    Choice (Ogro2Processor& p, int paramIndex, const juce::String& t, Theme th = Theme::Violet)
        : proc (p), title (t), theme (th)
    {
        addAndMakeVisible (box);
        bind (paramIndex);
    }
    // El ID de cada ítem es el valor del parámetro + 1. No usamos ComboBoxAttachment porque
    // mapea por cantidad de ítems y las listas tienen lugares reservados que no se muestran.
    void bind (int paramIndex)
    {
        attachment.reset();
        box.onChange = nullptr;
        box.clear (juce::dontSendNotification);
        const auto& d = paramDefs()[(size_t) paramIndex];
        if (d.kind == Kind::Int)
            for (int v = (int) d.min; v <= (int) d.max; ++v) box.addItem (juce::String::fromUTF8 (d.text ((float) v).c_str()), v + 1);
        else
            for (int i = 0; i < d.validChoices; ++i) box.addItem (juce::String::fromUTF8 (d.choices[(size_t) i].c_str()), i + 1);
        auto* param = proc.param (paramIndex);
        if (param == nullptr) return;
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v) {
            box.setSelectedId (juce::roundToInt (v) + 1, juce::dontSendNotification);
        });
        box.onChange = [this] { if (attachment && box.getSelectedId() > 0) attachment->setValueAsCompleteGesture ((float) (box.getSelectedId() - 1)); };
        attachment->sendInitialUpdate();
        box.setTooltip (juce::String::fromUTF8 (d.name.c_str()));
    }
    void paint (juce::Graphics& g) override
    {
        if (title.isEmpty()) return;
        g.setColour (theme == Theme::Violet ? col::violetText : col::label);
        drawSpaced (g, upper (title), getLocalBounds().removeFromTop (15).toFloat(), labelFont (12.0f), 0.06f, juce::Justification::centredLeft);
    }
    void resized() override { box.setBounds (getLocalBounds().withTrimmedTop (title.isEmpty() ? 0 : 18)); }

    juce::ComboBox box;

private:
    Ogro2Processor& proc;
    juce::String title;
    Theme theme;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// ---------------------------------------------------------------------------
// Fuente o destino de la matriz MOD / de un carril MOTION (se guarda por ID, no es parámetro)
// ---------------------------------------------------------------------------
class RouteChoice : public juce::Component
{
public:
    enum class Kind { ModSource, ModDest, LaneDest };
    RouteChoice (Ogro2Processor& p, Kind k, int index) : proc (p), kind (k), slot (index)
    {
        addAndMakeVisible (box);
        if (kind == Kind::ModSource)
            for (int i = 0; i < SrcCount; ++i) box.addItem (juce::String::fromUTF8 (modSource (i).name), i + 1);
        else
        {
            // Destinos agrupados por pista en submenús
            const auto& dests = modDests();
            auto* root = box.getRootMenu();
            root->addItem (1, "Nada");
            for (int t = -1; t < kTracks; ++t)
            {
                juce::PopupMenu sub;
                for (int i = 0; i < (int) dests.size(); ++i)
                    if (paramDefs()[(size_t) dests[(size_t) i].param].track == t)
                        sub.addItem (i + 2, juce::String::fromUTF8 (dests[(size_t) i].name.c_str()));
                root->addSubMenu (t < 0 ? juce::String ("GLOBAL") : juce::String (trackName (t)), sub);
            }
        }
        box.onChange = [this] {
            const int id = box.getSelectedId();
            if (id <= 0) return;
            if (kind == Kind::ModSource) proc.setModSource (slot, id - 1);
            else
            {
                const int param = id == 1 ? -1 : modDests()[(size_t) (id - 2)].param;
                if (kind == Kind::ModDest) proc.setModDest (slot, param); else proc.setLaneDest (slot, param);
            }
        };
        box.setTooltip (kind == Kind::ModSource ? T ("Fuente de la modulación") : T ("Qué perilla se mueve"));
        refresh();
    }
    // Muestra lo que tiene el motor (después de cargar un preset o una sesión)
    void refresh()
    {
        int id = 1;
        if (kind == Kind::ModSource) id = proc.engine.routing.src[slot].load() + 1;
        else
        {
            const int param = kind == Kind::ModDest ? proc.engine.routing.dst[slot].load() : proc.engine.routing.laneDst[slot].load();
            const auto& dests = modDests();
            for (int i = 0; i < (int) dests.size(); ++i) if (dests[(size_t) i].param == param) id = i + 2;
        }
        if (box.getSelectedId() != id) box.setSelectedId (id, juce::dontSendNotification);
    }
    void resized() override { box.setBounds (getLocalBounds()); }
    juce::ComboBox box;

private:
    Ogro2Processor& proc;
    Kind kind;
    int slot;
};

// ---------------------------------------------------------------------------
// Semilla del azar: clic = tirar el dado (semilla nueva); arrastrar = elegir
// ---------------------------------------------------------------------------
class SeedBox : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SeedBox (Ogro2Processor& p) : proc (p)
    {
        setTooltip (T ("Semilla del azar: con AZAR FIJO, la misma semilla da siempre el mismo resultado. Clic: tirar el dado. Arrastrá para elegir."));
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        paint::lcd (g, r, 6.0f);
        paint::glowText (g, "#" + juce::String ((int) proc.value (P::Seed)), r, lcdFont (r.getHeight() * 0.85f), juce::Justification::centred);
    }
    void mouseDown (const juce::MouseEvent&) override { startSeed = (int) proc.value (P::Seed); dragged = false; }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        dragged = true;
        proc.setValue (P::Seed, (float) juce::jlimit (1, 9999, startSeed - e.getDistanceFromDragStartY() / 3));
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (! dragged) proc.setValue (P::Seed, (float) juce::Random::getSystemRandom().nextInt ({ 1, 10000 }));
        repaint();
    }

private:
    Ogro2Processor& proc;
    int startSeed = 1;
    bool dragged = false;
};

// ---------------------------------------------------------------------------
// Tecla con LED enlazada a un parámetro booleano
// ---------------------------------------------------------------------------
class Toggle : public juce::Component
{
public:
    Toggle (Ogro2Processor& p, int paramIndex, const juce::String& text, juce::Colour ledColour = col::led) : proc (p)
    {
        button.setButtonText (text);
        button.setColour (juce::ToggleButton::tickColourId, ledColour);
        addAndMakeVisible (button);
        bind (paramIndex);
    }
    void bind (int paramIndex)
    {
        attachment.reset();
        attachment = std::make_unique<APVTS::ButtonAttachment> (proc.state, juce::String (paramDefs()[(size_t) paramIndex].id), button);
    }
    void setKeyColour (juce::Colour c) { button.getProperties().set ("keyColour", (juce::int64) c.getARGB()); button.repaint(); }
    void resized() override { button.setBounds (getLocalBounds()); }
    juce::ToggleButton button;

private:
    Ogro2Processor& proc;
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

// Tecla de calculadora sin parámetro (acciones)
inline void styleKey (juce::TextButton& b, juce::Colour c, juce::Colour text = col::ink)
{
    b.setColour (juce::TextButton::buttonColourId, c);
    b.setColour (juce::TextButton::textColourOffId, text);
}

// ---------------------------------------------------------------------------
// Pantalla de fósforo (número grande + texto chico)
// ---------------------------------------------------------------------------
class Lcd : public juce::Component
{
public:
    void setText (const juce::String& big, const juce::String& small)
    {
        if (big == bigText && small == smallText) return;
        bigText = big; smallText = small; repaint();
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        paint::lcd (g, r);
        auto in = r.reduced (10.0f, 3.0f);
        const float bigH = smallText.isEmpty() ? in.getHeight() : in.getHeight() * 0.62f;
        paint::glowText (g, bigText, in.removeFromTop (bigH), lcdFont (bigH * 1.08f), justification);
        if (smallText.isNotEmpty())
        {
            g.setColour (col::phosphorDim);
            g.setFont (lcdFont (in.getHeight() * 1.05f));
            g.drawText (smallText, in, justification, false);
        }
    }
    juce::Justification justification = juce::Justification::centredRight;

private:
    juce::String bigText, smallText;
};

// ---------------------------------------------------------------------------
// Display del preset: nombre y mood en fósforo; clic = lista de presets
// ---------------------------------------------------------------------------
class PresetDisplay : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void()> onClick;
    void setText (const juce::String& n, const juce::String& m)
    {
        if (n == name && m == mood) return;
        name = n; mood = m; repaint();
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        paint::lcd (g, r);
        auto in = r.reduced (12.0f, 4.0f);
        paint::glowText (g, upper (name), in.removeFromTop (in.getHeight() * 0.56f), lcdFont (27.0f), juce::Justification::bottomLeft);
        g.setColour (col::phosphorDim);
        g.setFont (lcdFont (17.0f));
        g.drawText (mood, in, juce::Justification::topLeft, true);
    }
    void mouseUp (const juce::MouseEvent& e) override { if (onClick && e.mouseWasClicked()) onClick(); }

private:
    juce::String name, mood;
};

// ---------------------------------------------------------------------------
// Medidor de fósforo (nivel o reducción de ganancia)
// ---------------------------------------------------------------------------
class Meter : public juce::Component
{
public:
    Meter (const juce::String& l, bool reduction, float range) : label (l), isGr (reduction), rangeDb (range) {}
    void set (float db)
    {
        const float s = shown + 0.4f * (db - shown);
        if (std::abs (s - shown) > 0.05f) { shown = s; repaint(); }
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto lab = r.removeFromBottom (14.0f);
        g.setColour (col::caption);
        g.setFont (labelFont (11.0f));
        g.drawText (label, lab, juce::Justification::centred);
        paint::lcd (g, r, 4.0f);
        auto bar = r.reduced (3.0f);
        const float frac = isGr ? juce::jlimit (0.0f, 1.0f, shown / rangeDb) : juce::jlimit (0.0f, 1.0f, (shown + rangeDb) / rangeDb);
        const int segs = juce::jmax (6, (int) (bar.getHeight() / 5.0f));
        const float segH = bar.getHeight() / (float) segs;
        for (int i = 0; i < segs; ++i)
        {
            const float pos = (float) i / (float) segs;
            const bool lit = pos < frac;
            juce::Colour c = isGr ? col::orange : (pos > 0.9f ? col::red : (pos > 0.75f ? col::yellow : col::phosphor));
            const float y = isGr ? bar.getY() + segH * (float) i : bar.getBottom() - segH * (float) (i + 1);
            g.setColour (lit ? c : col::phosphorOff);
            g.fillRect (bar.getX(), y + 0.5f, bar.getWidth(), segH - 1.0f);
        }
    }

private:
    juce::String label;
    bool isGr;
    float rangeDb, shown = -100.0f;
};

// ---------------------------------------------------------------------------
// Fila de pista: LED de disparo, nombre, perilla de volumen, mute y solo
// ---------------------------------------------------------------------------
class TrackRow : public juce::Component
{
public:
    TrackRow (Ogro2Processor& p, int t)
        : proc (p), track (t),
          mute (p, P::track (t, P::Mute), "M", col::orange),
          solo (p, P::track (t, P::Solo), "S", col::yellow)
    {
        level.setTravel (420); // más recorrido: ajuste fino del volumen de cada pista
        level.setColour (juce::Slider::rotarySliderFillColourId, col::track (t));
        level.setColour (juce::Slider::rotarySliderOutlineColourId, col::ink.withAlpha (0.14f));
        level.getProperties().set ("glow", false);
        levelAtt = std::make_unique<APVTS::SliderAttachment> (p.state, juce::String (paramDefs()[(size_t) P::track (t, P::Level)].id), level);
        level.setDoubleClickReturnValue (true, paramDefs()[(size_t) P::track (t, P::Level)].def);
        level.setTooltip (T ("Volumen de ") + trackName (t) + T (" · doble clic: 0 dB · Shift: ajuste fino"));
        level.onValueChange = [this] { repaint(); };
        addAndMakeVisible (level);
        addAndMakeVisible (mute);
        addAndMakeVisible (solo);
        mute.button.setTooltip ("Mute");
        solo.button.setTooltip ("Solo");
    }
    std::function<void (int)> onSelect;
    void setSelected (bool s)
    {
        if (s == selected) return;
        selected = s;
        level.setColour (juce::Slider::rotarySliderOutlineColourId, s ? juce::Colours::white.withAlpha (0.2f) : col::ink.withAlpha (0.14f));
        repaint();
    }
    void flash (float v) { if (std::abs (v - glow) > 0.01f) { glow = v; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f, 1.0f);
        if (selected)
        {
            g.setColour (col::violet.withAlpha (0.3f));
            g.fillRoundedRectangle (r.translated (0, 3.0f).expanded (1.0f), 10.0f);
            g.setColour (col::violetDeep);
            g.fillRoundedRectangle (r, 9.0f);
            g.setColour (col::violetRing);
            g.drawRoundedRectangle (r.reduced (0.5f), 9.0f, 1.0f);
        }
        else
        {
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.fillRoundedRectangle (r, 9.0f);
        }
        // LED de disparo
        auto led = juce::Rectangle<float> (r.getX() + 10.0f, r.getCentreY() - 4.5f, 9.0f, 9.0f);
        if (glow > 0.02f) { g.setColour (col::led.withAlpha (0.4f * glow)); g.fillEllipse (led.expanded (4.0f)); }
        g.setColour ((selected ? col::violetDeep.brighter (0.4f) : col::ledOff).interpolatedWith (col::led, glow));
        g.fillEllipse (led);
        // Color y nombre
        g.setColour (col::track (track));
        g.fillRoundedRectangle (r.getX() + 25.0f, r.getCentreY() - 8.0f, 4.0f, 16.0f, 2.0f);
        g.setColour (selected ? col::violetText : col::ink);
        drawSpaced (g, trackName (track), { r.getX() + 36.0f, r.getY(), 96.0f, r.getHeight() }, labelFont (14.0f), 0.04f,
                    juce::Justification::centredLeft);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (6, 5);
        solo.setBounds (r.removeFromRight (24).withSizeKeepingCentre (24, 24));
        r.removeFromRight (4);
        mute.setBounds (r.removeFromRight (24).withSizeKeepingCentre (24, 24));
        r.removeFromRight (6);
        level.setBounds (r.removeFromRight (34).withSizeKeepingCentre (34, 34));
    }
    void mouseDown (const juce::MouseEvent&) override { if (onSelect) onSelect (track); }

private:
    Ogro2Processor& proc;
    int track;
    bool selected = false;
    float glow = 0.0f;
    FineSlider level;
    std::unique_ptr<APVTS::SliderAttachment> levelAtt;
    Toggle mute, solo;
};

// ---------------------------------------------------------------------------
// Macro grande: anillo bipolar desde el centro, nombre, explicación y cuánto mueve
// ---------------------------------------------------------------------------
class MacroKnob : public juce::Component
{
public:
    MacroKnob (Ogro2Processor& p, int paramIndex, const juce::String& n, const juce::String& cap, juce::Colour c)
        : name (n), caption (cap), colour (c)
    {
        slider.setColour (juce::Slider::rotarySliderFillColourId, c);
        slider.setColour (juce::Slider::rotarySliderOutlineColourId, col::ink.withAlpha (0.14f));
        slider.getProperties().set ("bipolar", true);
        slider.setTravel (320);
        att = std::make_unique<APVTS::SliderAttachment> (p.state, juce::String (paramDefs()[(size_t) paramIndex].id), slider);
        slider.setDoubleClickReturnValue (true, paramDefs()[(size_t) paramIndex].def);
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (slider);
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().withTrimmedLeft (68.0f);
        g.setColour (col::ink);
        drawSpaced (g, name, r.removeFromTop (20.0f).withTrimmedTop (2.0f), labelFont (13.5f), 0.0f, juce::Justification::bottomLeft);
        g.setColour (col::caption);
        g.setFont (textFont (13.0f));
        g.drawFittedText (caption, r.removeFromTop (34.0f).toNearestInt(), juce::Justification::topLeft, 2, 0.9f);
        const int pct = juce::roundToInt ((slider.getValue() - 0.5) * 200.0);
        const auto txt = (pct > 0 ? "+" : "") + juce::String (pct) + "%";
        auto chip = juce::Rectangle<float> (r.getX(), r.getY(), 52.0f, 22.0f);
        paint::lcd (g, chip, 5.0f);
        paint::glowText (g, txt, chip, lcdFont (21.0f), juce::Justification::centred);
    }
    void resized() override { slider.setBounds (0, (getHeight() - 64) / 2, 64, 64); }
    FineSlider slider;

private:
    juce::String name, caption;
    juce::Colour colour;
    std::unique_ptr<APVTS::SliderAttachment> att;
};

// ---------------------------------------------------------------------------
// Orbe violeta de play (tocar sin el transporte de Ableton)
// ---------------------------------------------------------------------------
class PlayOrb : public juce::Button
{
public:
    PlayOrb() : juce::Button ("play") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (6.0f);
        const auto c = r.getCentre();
        const bool on = getToggleState();
        for (int i = 4; i >= 1; --i)
        {
            g.setColour (juce::Colour (0xff8c5aff).withAlpha ((on ? 0.12f : 0.07f) * (float) i / 2.0f));
            g.fillEllipse (r.expanded (2.5f * (float) i));
        }
        juce::ColourGradient orb (juce::Colours::white, c.x - r.getWidth() * 0.14f, c.y - r.getHeight() * 0.2f,
                                  juce::Colour (0xff1e0a5a), r.getRight(), r.getBottom(), true);
        orb.addColour (0.13, juce::Colour (0xffdcd0ff));
        orb.addColour (0.40, on ? juce::Colour (0xffb48cff) : juce::Colour (0xff9c76ff));
        orb.addColour (0.68, juce::Colour (0xff5a2adf));
        g.setGradientFill (orb);
        g.fillEllipse (down ? r.reduced (1.0f) : r);
        g.setColour (juce::Colour (0xffdcd0ff).withAlpha (over ? 1.0f : 0.8f));
        g.drawEllipse (r, 1.5f);
        // Símbolo
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        if (on) { g.fillRect (c.x - 7.0f, c.y - 7.0f, 5.0f, 14.0f); g.fillRect (c.x + 2.0f, c.y - 7.0f, 5.0f, 14.0f); }
        else { juce::Path tri; tri.addTriangle (c.x - 5.0f, c.y - 8.0f, c.x - 5.0f, c.y + 8.0f, c.x + 9.0f, c.y); g.fillPath (tri); }
    }
};

// ---------------------------------------------------------------------------
// Disco negro con anillo de neón: nivel de salida del master
// ---------------------------------------------------------------------------
class MasterDisc : public juce::Component
{
public:
    void set (float outDb, float limDb)
    {
        const float d = shownDb + 0.35f * (outDb - shownDb);
        if (std::abs (d - shownDb) > 0.05f || std::abs (limDb - lim) > 0.05f) { shownDb = d; lim = limDb; repaint(); }
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (4.0f);
        const auto c = r.getCentre();
        const float R = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (c.x - R, c.y - R + 5.0f, R * 2, R * 2);
        g.setColour (juce::Colour (0xff0b090e));
        g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
        g.setColour (juce::Colours::white.withAlpha (0.035f));
        for (float rr = R * 0.4f; rr < R - 2.0f; rr += 4.0f) g.drawEllipse (c.x - rr, c.y - rr, rr * 2, rr * 2, 1.0f);
        g.setColour (juce::Colour (0xff26222c));
        g.drawEllipse (c.x - R, c.y - R, R * 2, R * 2, 2.0f);
        // Anillo de nivel (de -48 a 0 dB)
        const float frac = juce::jlimit (0.0f, 1.0f, (shownDb + 48.0f) / 48.0f);
        const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = a0 + frac * juce::MathConstants<float>::pi * 1.5f;
        const float rr = R * 0.86f;
        juce::Path trk; trk.addCentredArc (c.x, c.y, rr, rr, 0, a0, juce::MathConstants<float>::pi * 2.75f, true);
        g.setColour (col::discRing.withAlpha (0.12f));
        g.strokePath (trk, juce::PathStrokeType (R * 0.1f));
        if (frac > 0.001f)
        {
            juce::Path arc; arc.addCentredArc (c.x, c.y, rr, rr, 0, a0, a1, true);
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (col::discRing.withAlpha (0.1f));
                g.strokePath (arc, juce::PathStrokeType (R * 0.1f + 4.0f * (float) i));
            }
            g.setColour (shownDb > -1.0f ? col::red : col::discRing);
            g.strokePath (arc, juce::PathStrokeType (R * 0.1f));
        }
        g.setColour (juce::Colour (0xffe9e1ff));
        g.fillPath (paint::star ({ c.x + R * 0.6f, c.y - R * 0.6f }, 9.0f, 1.6f));
        // Textos
        g.setColour (juce::Colour (0xffc9b3ff));
        drawSpaced (g, "MASTER", { c.x - R, c.y - R * 0.42f, R * 2, 16.0f }, titleFont (11.0f), 0.18f, juce::Justification::centred);
        paint::glowText (g, shownDb <= -60.0f ? juce::String ("-inf") : juce::String (shownDb, 1) + " dB",
                         { c.x - R, c.y - 14.0f, R * 2, 30.0f }, lcdFont (30.0f), juce::Justification::centred);
        g.setColour (juce::Colour (0xff9c8cc8));
        g.setFont (textFont (12.0f));
        g.drawText (T ("LIMITADOR ") + juce::String (lim, 1) + " dB", juce::Rectangle<float> (c.x - R, c.y + R * 0.22f, R * 2, 16.0f),
                    juce::Justification::centred);
    }

private:
    float shownDb = -100.0f, lim = 0.0f;
};

// ---------------------------------------------------------------------------
// Edición de pasos
// ---------------------------------------------------------------------------
enum class LaneMode { Vel = 0, Note, Prob, Ratchet, Slide };
constexpr int kViewSteps = 16;

struct SeqContext
{
    Ogro2Processor& proc;
    int track() const { return proc.selectedTrack.load(); }
    int pattern() const { return (int) proc.value (P::Pattern); }
    PatternBank& bank() const { return proc.engine.bank; }
};

// Teclas de pasos tipo calculadora con LED arriba y número abajo
class StepKeys : public juce::Component
{
public:
    explicit StepKeys (SeqContext c) : ctx (c) { setRepaintsOnMouseActivity (false); }

    void paint (juce::Graphics& g) override
    {
        const int t = ctx.track(), p = ctx.pattern();
        const int len = ctx.bank().getLength (p, t);
        const bool playing = ctx.proc.engine.meters.playing.load();
        const int cur = ctx.proc.engine.meters.step[t].load();
        const float w = (float) getWidth() / (float) kViewSteps;
        const int root = (int) ctx.proc.value (P::Root);
        for (int s = 0; s < kViewSteps; ++s)
        {
            const Step st = ctx.bank().get (p, t, s);
            const bool active = st.on && s < len;
            const bool here = playing && s == cur;
            auto cell = juce::Rectangle<float> ((float) s * w, 0.0f, w, (float) getHeight()).reduced (3.5f, 0.0f);
            paint::led (g, { cell.getCentreX() - 6.0f, 1.0f, 12.0f, 7.0f }, here ? col::led : col::ledGreen, here || active);
            auto key = cell.withTrimmedTop (14.0f).withTrimmedBottom (18.0f);
            juce::Colour kc = st.vel >= 118 && active ? col::orange : col::stepKey (s);
            if (s >= len) kc = kc.withMultipliedSaturation (0.2f).darker (0.25f).withAlpha (0.6f);
            paint::key (g, key, kc, active, false);
            auto face = active ? key.translated (0, 1.5f) : key;
            if (here)
            {
                g.setColour (col::violet);
                g.drawRoundedRectangle (key.expanded (2.5f), 9.0f, 2.0f);
            }
            if (active)
            {
                const auto lightR = juce::Rectangle<float> (face.getX() + face.getWidth() * 0.2f, face.getBottom() - 9.0f, face.getWidth() * 0.6f, 4.0f);
                g.setColour (col::track (t).withAlpha (0.35f));
                g.fillRoundedRectangle (lightR.expanded (2.0f), 3.0f);
                g.setColour (col::track (t));
                g.fillRoundedRectangle (lightR, 2.0f);
                g.setColour (col::ink.withAlpha (0.85f));
                g.setFont (labelFont (13.0f));
                juce::String txt;
                if (t >= Bass) txt = noteName ((t == Bass ? 36 : 48) + root + st.note);
                if (st.ratchet > 1) txt = txt.isEmpty() ? "x" + juce::String (st.ratchet) : txt + " x" + juce::String (st.ratchet);
                if (txt.isEmpty() && st.prob < 100) txt = juce::String (st.prob) + "%";
                g.drawText (txt, face.reduced (2.0f, 0.0f), juce::Justification::centred, false);
            }
            g.setColour (s % 4 == 0 ? col::ink : col::dim);
            g.setFont (labelFont (12.0f));
            g.drawText (juce::String (s + 1), cell.withTop ((float) getHeight() - 16.0f), juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int s = stepAt (e.x);
        if (s < 0) return;
        const int t = ctx.track(), p = ctx.pattern();
        Step st = ctx.bank().get (p, t, s);
        if (e.mods.isPopupMenu())
        {
            if (! st.on) st.on = true;
            st.vel = st.vel >= 118 ? 100 : 127;
            ctx.bank().set (p, t, s, st);
            dragMode = -1;
        }
        else
        {
            st.on = ! st.on;
            if (st.on && st.vel < 10) st.vel = 100;
            ctx.bank().set (p, t, s, st);
            dragMode = st.on ? 1 : 0;
        }
        lastStep = s;
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragMode < 0) return;
        const int s = stepAt (e.x);
        if (s < 0 || s == lastStep) return;
        lastStep = s;
        const int t = ctx.track(), p = ctx.pattern();
        Step st = ctx.bank().get (p, t, s);
        st.on = dragMode == 1;
        ctx.bank().set (p, t, s, st);
        repaint();
    }

private:
    int stepAt (int x) const { return juce::jlimit (0, kViewSteps - 1, (int) ((float) x / ((float) getWidth() / kViewSteps))); }
    SeqContext ctx;
    int dragMode = 0, lastStep = -1;
};

// Pantalla de fósforo con el valor de cada paso (se dibuja arrastrando)
class StepLane : public juce::Component
{
public:
    explicit StepLane (SeqContext c) : ctx (c) {}
    void setMode (LaneMode m) { mode = m; repaint(); }
    LaneMode getMode() const { return mode; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        paint::lcd (g, r);
        const int t = ctx.track(), p = ctx.pattern();
        const int len = ctx.bank().getLength (p, t);
        const auto in = r.reduced (10.0f, 8.0f);
        const float w = in.getWidth() / (float) kViewSteps;
        const int root = (int) ctx.proc.value (P::Root);
        const int base = t == Bass ? 36 + root : (t == Chord ? 48 + root : 60);
        // Guías
        g.setColour (col::phosphorOff);
        for (int s = 4; s < kViewSteps; s += 4) g.fillRect (in.getX() + w * (float) s - 0.5f, in.getY(), 1.0f, in.getHeight());
        if (mode == LaneMode::Note) g.fillRect (in.getX(), in.getCentreY(), in.getWidth(), 1.0f);

        for (int s = 0; s < kViewSteps; ++s)
        {
            const Step st = ctx.bank().get (p, t, s);
            const bool active = st.on && s < len;
            auto cr = juce::Rectangle<float> (in.getX() + w * (float) s, in.getY() + 14.0f, w, in.getHeight() - 14.0f).reduced (4.0f, 0.0f);
            juce::String label;
            juce::Rectangle<float> bar;
            switch (mode)
            {
                case LaneMode::Vel: bar = cr.withTop (cr.getBottom() - cr.getHeight() * (float) st.vel / 127.0f); label = juce::String (st.vel); break;
                case LaneMode::Prob: bar = cr.withTop (cr.getBottom() - cr.getHeight() * (float) st.prob / 100.0f); label = juce::String (st.prob) + "%"; break;
                case LaneMode::Note:
                {
                    const float cy = cr.getCentreY(), y = cy - (float) st.note / 24.0f * cr.getHeight() * 0.5f;
                    bar = juce::Rectangle<float>::leftTopRightBottom (cr.getX(), juce::jmin (cy, y), cr.getRight(), juce::jmax (cy, y) + 2.0f);
                    label = (st.note > 0 ? "+" : "") + juce::String (st.note);
                    if (t >= Bass) label = juce::String (noteName (base + st.note));
                    break;
                }
                case LaneMode::Ratchet:
                    bar = cr.withTop (cr.getBottom() - cr.getHeight() * (float) st.ratchet / 4.0f);
                    label = "x" + juce::String (st.ratchet);
                    break;
                case LaneMode::Slide:
                    bar = st.slide ? cr.withSizeKeepingCentre (cr.getWidth(), 18.0f) : juce::Rectangle<float>();
                    label = st.slide ? "LIGA" : "";
                    break;
            }
            if (! bar.isEmpty())
            {
                if (active)
                {
                    g.setColour (col::phosphor.withAlpha (0.25f));
                    g.fillRoundedRectangle (bar.expanded (2.0f), 3.0f);
                }
                g.setColour (active ? col::phosphor : col::phosphorOff);
                g.fillRoundedRectangle (bar, 2.0f);
            }
            g.setColour (active ? col::phosphor : col::phosphorDim.withAlpha (0.55f));
            g.setFont (lcdFont (17.0f));
            g.drawText (label, juce::Rectangle<float> (cr.getX() - 4.0f, in.getY() - 2.0f, cr.getWidth() + 8.0f, 14.0f), juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { lastS = -1; slideSet = -1; apply (e); }
    void mouseDrag (const juce::MouseEvent& e) override { apply (e); }

private:
    void apply (const juce::MouseEvent& e)
    {
        const auto r = getLocalBounds().toFloat().reduced (10.0f, 8.0f).withTrimmedTop (14.0f);
        const int s = juce::jlimit (0, kViewSteps - 1, (int) ((float) (e.x - 10) / (r.getWidth() / kViewSteps)));
        const float f = juce::jlimit (0.0f, 1.0f, (r.getBottom() - (float) e.y) / r.getHeight());
        const int t = ctx.track(), p = ctx.pattern();
        Step st = ctx.bank().get (p, t, s);
        switch (mode)
        {
            case LaneMode::Vel: st.vel = juce::jmax (1, juce::roundToInt (f * 127.0f)); break;
            case LaneMode::Prob: st.prob = juce::roundToInt (f * 20.0f) * 5; break;
            case LaneMode::Note: st.note = juce::jlimit (-24, 24, juce::roundToInt ((f - 0.5f) * 48.0f)); break;
            case LaneMode::Ratchet: st.ratchet = juce::jlimit (1, 4, 1 + (int) (f * 4.0f)); break;
            case LaneMode::Slide:
                if (s == lastS) return;
                if (slideSet < 0) slideSet = st.slide ? 0 : 1;
                st.slide = slideSet == 1;
                break;
        }
        lastS = s;
        ctx.bank().set (p, t, s, st);
        repaint();
    }

    SeqContext ctx;
    LaneMode mode = LaneMode::Vel;
    int lastS = -1, slideSet = -1;
};

// ---------------------------------------------------------------------------
// Carril de automatización dibujable (pantalla de fósforo con curva violeta)
// ---------------------------------------------------------------------------
class MotionCanvas : public juce::Component
{
public:
    MotionCanvas (Ogro2Processor& p, int l) : proc (p), lane (l) {}

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        paint::lcd (g, r);
        auto in = r.reduced (6.0f);
        const int bars = juce::jlimit (1, 8, (int) proc.value (P::lane (lane, P::LaneBars)));
        for (int b = 0; b <= bars * 4; ++b)
        {
            const float x = in.getX() + in.getWidth() * (float) b / (float) (bars * 4);
            g.setColour (b % 4 == 0 ? col::phosphorDim.withAlpha (0.5f) : col::phosphorOff);
            g.fillRect (x - 0.5f, in.getY(), 1.0f, in.getHeight());
        }
        const bool active = proc.engine.routing.laneDst[lane].load() >= 0;
        const auto c = active ? juce::Colour (0xffb48cff) : col::phosphorDim;
        juce::Path fill, line;
        const float w = in.getWidth() / (float) kLanePoints;
        fill.startNewSubPath (in.getX(), in.getBottom());
        for (int i = 0; i < kLanePoints; ++i)
        {
            const float v = proc.engine.bank.lane (lane, i);
            const float x0 = in.getX() + w * (float) i, y = in.getBottom() - v * in.getHeight();
            fill.lineTo (x0, y); fill.lineTo (x0 + w, y);
            if (i == 0) line.startNewSubPath (x0, y); else line.lineTo (x0, y);
            line.lineTo (x0 + w, y);
        }
        fill.lineTo (in.getRight(), in.getBottom());
        fill.closeSubPath();
        g.setColour (c.withAlpha (0.16f));
        g.fillPath (fill);
        g.setColour (c.withAlpha (0.3f));
        g.strokePath (line, juce::PathStrokeType (5.0f));
        g.setColour (c);
        g.strokePath (line, juce::PathStrokeType (2.2f));
        if (proc.engine.meters.playing.load())
        {
            const float x = in.getX() + in.getWidth() * proc.engine.meters.lanePhase[lane].load();
            g.setColour (col::phosphor);
            g.fillRect (x - 1.0f, in.getY(), 2.0f, in.getHeight());
        }
        if (! active)
        {
            g.setColour (col::phosphorDim);
            g.setFont (lcdFont (20.0f));
            g.drawText (T ("Elegí un destino y dibujá con el mouse"), in, juce::Justification::centred);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override { last = -1; draw (e); }
    void mouseDrag (const juce::MouseEvent& e) override { draw (e); }

private:
    void draw (const juce::MouseEvent& e)
    {
        auto in = getLocalBounds().toFloat().reduced (6.0f);
        const int i = juce::jlimit (0, kLanePoints - 1, (int) ((e.position.x - in.getX()) / in.getWidth() * kLanePoints));
        const float v = juce::jlimit (0.0f, 1.0f, (in.getBottom() - e.position.y) / in.getHeight());
        if (last >= 0 && last != i)
        {
            const int a = juce::jmin (last, i), b = juce::jmax (last, i);
            for (int k = a; k <= b; ++k)
            {
                const float t = b == a ? 1.0f : (float) (k - last) / (float) (i - last);
                proc.engine.bank.setLane (lane, k, lastV + (v - lastV) * t);
            }
        }
        else proc.engine.bank.setLane (lane, i, v);
        last = i; lastV = v;
        repaint();
    }
    Ogro2Processor& proc;
    int lane, last = -1;
    float lastV = 0.5f;
};

// Mini osciloscopio de un LFO
class LfoScope : public juce::Component
{
public:
    LfoScope (Ogro2Processor& p, int l) : proc (p), lfo (l) {}
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        paint::lcd (g, r, 6.0f);
        auto in = r.reduced (6.0f, 7.0f);
        const int shape = (int) proc.value (P::lfo (lfo, P::LfoShape));
        juce::Path p;
        for (int i = 0; i <= 64; ++i)
        {
            const float ph = (float) i / 64.0f;
            float v = 0;
            switch (shape)
            {
                case 0: v = std::sin (juce::MathConstants<float>::twoPi * ph); break;
                case 1: v = ph < 0.5f ? 4 * ph - 1 : 3 - 4 * ph; break;
                case 2: v = 2 * ph - 1; break;
                case 3: v = 1 - 2 * ph; break;
                case 4: v = ph < 0.5f ? 1.0f : -1.0f; break;
                default: { const int k = (int) (ph * 8.0f); v = std::sin ((float) k * 12.9898f) * 0.9f; if (shape == 6) v = std::sin (ph * 9.0f) * 0.8f; }
            }
            const float x = in.getX() + in.getWidth() * ph, y = in.getCentreY() - v * in.getHeight() * 0.5f;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (col::phosphor.withAlpha (0.25f));
        g.strokePath (p, juce::PathStrokeType (4.0f));
        g.setColour (col::phosphor);
        g.strokePath (p, juce::PathStrokeType (1.6f));
        const float cur = proc.engine.meters.lfo[lfo].load();
        g.setColour (juce::Colour (0xffb48cff));
        g.fillEllipse (in.getRight() - 7.0f, in.getCentreY() - cur * in.getHeight() * 0.5f - 3.5f, 7.0f, 7.0f);
    }

private:
    Ogro2Processor& proc;
    int lfo;
};

} // namespace ogro2::ui
