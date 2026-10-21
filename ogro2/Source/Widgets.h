// OGRO 2 — componentes de interfaz
#pragma once

#include "Ogro2LookAndFeel.h"
#include "PluginProcessor.h"

namespace ogro2::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

// ---------------------------------------------------------------------------
// Perilla con título arriba y valor abajo (se puede re-enlazar a otro parámetro)
// ---------------------------------------------------------------------------
class Knob : public juce::Component
{
public:
    Knob (Ogro2Processor& p, juce::Colour cap, bool bipolarKnob = false) : proc (p)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setColour (juce::Slider::rotarySliderFillColourId, cap);
        slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        slider.setMouseDragSensitivity (200);
        slider.getProperties().set ("bipolar", bipolarKnob);
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (slider);
    }
    Knob (Ogro2Processor& p, int paramIndex, const juce::String& t, juce::Colour cap, bool bipolarKnob = false)
        : Knob (p, cap, bipolarKnob)
    {
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
        slider.setTooltip (juce::String::fromUTF8 (d.name.c_str()) + T (" — doble clic: valor por defecto"));
        repaint();
    }
    void setCap (juce::Colour c, bool bipolarKnob)
    {
        slider.setColour (juce::Slider::rotarySliderFillColourId, c);
        slider.getProperties().set ("bipolar", bipolarKnob);
        repaint();
    }
    void setDark (bool d) { dark = d; repaint(); }
    void setTitleSize (float s) { titleSize = s; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        g.setColour (dark ? col::ink : col::cream);
        g.setFont (font (titleSize, true));
        g.drawText (title, r.removeFromTop (14), juce::Justification::centred);
        g.setColour (dark ? col::ink.withAlpha (0.75f) : col::dim.brighter (0.3f));
        g.setFont (font (11.0f));
        g.drawText (slider.getTextFromValue (slider.getValue()), r.removeFromBottom (14), juce::Justification::centred);
    }
    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromTop (14);
        r.removeFromBottom (14);
        const int s = juce::jmin (r.getWidth(), r.getHeight());
        slider.setBounds (r.withSizeKeepingCentre (s, s));
    }

    juce::Slider slider;

private:
    Ogro2Processor& proc;
    juce::String title;
    int index = -1;
    bool dark = false;
    float titleSize = 10.5f;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// ---------------------------------------------------------------------------
// Combo enlazado a un parámetro de elección
// ---------------------------------------------------------------------------
class Choice : public juce::Component
{
public:
    Choice (Ogro2Processor& p, int paramIndex, const juce::String& t, bool destMenu = false) : proc (p), title (t)
    {
        addAndMakeVisible (box);
        bind (paramIndex, destMenu);
    }
    void bind (int paramIndex, bool destMenu)
    {
        attachment.reset();
        box.clear (juce::dontSendNotification);
        const auto& d = paramDefs()[(size_t) paramIndex];
        if (destMenu) fillDestinations();
        else
            for (int i = 0; i < (int) d.choices.size(); ++i) box.addItem (juce::String::fromUTF8 (d.choices[(size_t) i].c_str()), i + 1);
        attachment = std::make_unique<APVTS::ComboBoxAttachment> (proc.state, juce::String (d.id), box);
    }
    void setTitle (const juce::String& t) { title = t; repaint(); }
    void paint (juce::Graphics& g) override
    {
        if (title.isEmpty()) return;
        g.setColour (col::dim.brighter (0.2f));
        g.setFont (font (10.0f, true));
        g.drawText (title, getLocalBounds().removeFromTop (13), juce::Justification::centredLeft);
    }
    void resized() override { box.setBounds (getLocalBounds().withTrimmedTop (title.isEmpty() ? 0 : 14)); }

    juce::ComboBox box;

private:
    // Destinos de modulación agrupados por pista en submenús
    void fillDestinations()
    {
        const auto& dests = modDests();
        auto* root = box.getRootMenu();
        root->addItem (1, "Nada");
        int id = 2;
        const int perTrack = kSoundParams + 4;
        for (int t = 0; t < kTracks; ++t)
        {
            juce::PopupMenu sub;
            for (int k = 0; k < perTrack; ++k, ++id)
                sub.addItem (id, juce::String::fromUTF8 (dests[(size_t) (id - 1)].name.c_str()));
            root->addSubMenu (trackName (t), sub);
        }
        juce::PopupMenu glob;
        for (; id <= (int) dests.size(); ++id) glob.addItem (id, juce::String::fromUTF8 (dests[(size_t) (id - 1)].name.c_str()));
        root->addSubMenu ("GLOBAL", glob);
    }

    Ogro2Processor& proc;
    juce::String title;
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

// ---------------------------------------------------------------------------
// Tecla ON/OFF con LED enlazada a un parámetro booleano
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
    void resized() override { button.setBounds (getLocalBounds()); }
    juce::ToggleButton button;

private:
    Ogro2Processor& proc;
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

// ---------------------------------------------------------------------------
// Medidor (nivel o reducción de ganancia)
// ---------------------------------------------------------------------------
class Meter : public juce::Component
{
public:
    Meter (const juce::String& l, bool reduction, float range) : label (l), isGr (reduction), rangeDb (range) {}
    void set (float db) { shown += 0.4f * (db - shown); repaint(); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto lab = r.removeFromBottom (12.0f);
        g.setColour (col::dim);
        g.setFont (font (9.0f, true));
        g.drawText (label, lab, juce::Justification::centred);
        auto bar = r.reduced (2.0f, 1.0f);
        g.setColour (col::lcd);
        g.fillRect (bar);
        const float frac = isGr ? juce::jlimit (0.0f, 1.0f, shown / rangeDb) : juce::jlimit (0.0f, 1.0f, (shown + rangeDb) / rangeDb);
        // Segmentos tipo LED
        const int segs = juce::jmax (6, (int) (bar.getHeight() / 5.0f));
        const float segH = bar.getHeight() / (float) segs;
        for (int i = 0; i < segs; ++i)
        {
            const float pos = (float) i / (float) segs;
            const bool lit = isGr ? pos < frac : pos < frac;
            if (! lit) continue;
            juce::Colour c = isGr ? col::orange : (pos > 0.88f ? col::red : (pos > 0.7f ? col::yellow : juce::Colour (0xff8fd18a)));
            const float y = isGr ? bar.getY() + segH * (float) i : bar.getBottom() - segH * (float) (i + 1);
            g.setColour (c);
            g.fillRect (bar.getX() + 1.0f, y + 0.5f, bar.getWidth() - 2.0f, segH - 1.0f);
        }
    }

private:
    juce::String label;
    bool isGr;
    float rangeDb, shown = -100.0f;
};

// ---------------------------------------------------------------------------
// Pantalla LCD (tempo, nombre de preset)
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
        g.setColour (col::lcd);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        auto inner = r.reduced (6.0f, 3.0f);
        g.setColour (col::lcdText.withAlpha (0.25f));
        g.setFont (font (inner.getHeight() * 0.62f, true));
        g.drawText ("888.8", inner.withTrimmedBottom (inner.getHeight() * 0.3f), juce::Justification::centredRight);
        g.setColour (col::lcdText);
        g.drawText (bigText, inner.withTrimmedBottom (inner.getHeight() * 0.3f), juce::Justification::centredRight);
        g.setFont (font (9.0f, true));
        g.setColour (col::lcdText.withAlpha (0.8f));
        g.drawText (smallText, inner.removeFromBottom (inner.getHeight() * 0.3f), juce::Justification::centredRight);
    }

private:
    juce::String bigText, smallText;
};

// ---------------------------------------------------------------------------
// Fila de pista (lista de la izquierda)
// ---------------------------------------------------------------------------
class TrackRow : public juce::Component
{
public:
    TrackRow (Ogro2Processor& p, int t)
        : proc (p), track (t),
          mute (p, P::track (t, P::Mute), "M", col::orange),
          solo (p, P::track (t, P::Solo), "S", col::yellow),
          level (p, P::track (t, P::Level), "", col::track (t))
    {
        addAndMakeVisible (mute);
        addAndMakeVisible (solo);
        addAndMakeVisible (level);
        mute.button.setTooltip ("Mute");
        solo.button.setTooltip ("Solo");
    }
    std::function<void (int)> onSelect;
    void setSelected (bool s) { if (s != selected) { selected = s; repaint(); } }
    void flash (float v) { if (std::abs (v - glow) > 0.01f) { glow = v; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (selected ? col::bodyLight.brighter (0.12f) : col::panel);
        g.fillRoundedRectangle (r, 4.0f);
        if (selected)
        {
            g.setColour (col::cream.withAlpha (0.8f));
            g.drawRoundedRectangle (r, 4.0f, 1.2f);
        }
        g.setColour (col::track (track));
        g.fillRoundedRectangle (r.withWidth (6.0f), 2.0f);
        // LED de disparo
        auto led = juce::Rectangle<float> (r.getX() + 14.0f, r.getCentreY() - 4.0f, 8.0f, 8.0f);
        if (glow > 0.02f) { g.setColour (col::led.withAlpha (0.35f * glow)); g.fillEllipse (led.expanded (4.0f)); }
        g.setColour (col::ledOff.interpolatedWith (col::led, glow));
        g.fillEllipse (led);
        g.setColour (col::cream);
        g.setFont (font (13.0f, true));
        g.drawText (trackName (track), juce::Rectangle<float> (r.getX() + 28.0f, r.getY(), 80.0f, r.getHeight()), juce::Justification::centredLeft);
        g.setColour (col::dim);
        g.setFont (font (9.0f, true));
        g.drawText (juce::String (track + 1), juce::Rectangle<float> (r.getX() + 28.0f, r.getY() + 2.0f, 80.0f, 10.0f), juce::Justification::topLeft);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (3);
        solo.setBounds (r.removeFromRight (30).reduced (0, 6));
        r.removeFromRight (3);
        mute.setBounds (r.removeFromRight (30).reduced (0, 6));
        r.removeFromRight (2);
        level.setBounds (r.removeFromRight (40).withSizeKeepingCentre (40, 40).translated (0, 0));
    }
    void mouseDown (const juce::MouseEvent&) override { if (onSelect) onSelect (track); }

private:
    // Perilla de nivel compacta sin textos
    class MiniLevel : public juce::Component
    {
    public:
        MiniLevel (Ogro2Processor& p, int idx, const juce::String&, juce::Colour c) : knob (p, idx, "", c)
        {
            addAndMakeVisible (knob.slider);
            knob.slider.setTooltip (T ("Nivel de la pista"));
        }
        void resized() override { knob.slider.setBounds (getLocalBounds()); }
        Knob knob;
    };

    Ogro2Processor& proc;
    int track;
    bool selected = false;
    float glow = 0.0f;
    Toggle mute, solo;
    MiniLevel level;
};

// ---------------------------------------------------------------------------
// Edición de pasos
// ---------------------------------------------------------------------------
enum class LaneMode { Vel = 0, Note, Prob, Ratchet, Slide };

struct SeqContext
{
    Ogro2Processor& proc;
    int track() const { return proc.selectedTrack.load(); }
    int pattern() const { return (int) proc.value (P::Pattern); }
    PatternBank& bank() const { return proc.engine.bank; }
};

// 16 teclas de pasos con LEDs (estilo caja de ritmos)
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
        const float w = (float) getWidth() / (float) kSteps;
        for (int s = 0; s < kSteps; ++s)
        {
            const Step st = ctx.bank().get (p, t, s);
            auto cell = juce::Rectangle<float> ((float) s * w, 0.0f, w, (float) getHeight()).reduced (5.0f, 0.0f);
            // LED
            auto led = juce::Rectangle<float> (cell.getCentreX() - 5.0f, 2.0f, 10.0f, 10.0f);
            const bool here = playing && s == cur;
            const bool lit = (st.on && s < len) != here; // el LED del paso actual parpadea invertido
            if (lit) { g.setColour (col::led.withAlpha (0.35f)); g.fillEllipse (led.expanded (4.0f)); }
            g.setColour (lit ? col::led : col::ledOff);
            g.fillEllipse (led);
            // Tecla
            auto key = cell.withTrimmedTop (18.0f).withTrimmedBottom (16.0f);
            juce::Colour kc = col::stepKey (s);
            if (s >= len) kc = kc.withMultipliedSaturation (0.15f).darker (0.6f);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.fillRoundedRectangle (key.translated (0, 3.0f), 4.0f);
            juce::ColourGradient grad (kc.brighter (0.12f), key.getX(), key.getY(), kc.darker (0.25f), key.getX(), key.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (key, 4.0f);
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.drawLine (key.getX() + 4, key.getY() + 1.5f, key.getRight() - 4, key.getY() + 1.5f, 1.0f);
            if (here)
            {
                g.setColour (col::cream);
                g.drawRoundedRectangle (key.expanded (2.0f), 5.0f, 2.0f);
            }
            if (st.on && s < len && st.vel >= 118)
            {
                g.setColour (col::ink.withAlpha (0.8f));
                g.setFont (font (10.0f, true));
                g.drawText ("AC", key.withTrimmedTop (key.getHeight() - 16.0f), juce::Justification::centred);
            }
            if (st.on && st.ratchet > 1 && s < len)
            {
                g.setColour (col::ink.withAlpha (0.8f));
                g.setFont (font (10.0f, true));
                g.drawText ("x" + juce::String (st.ratchet), key.withHeight (16.0f), juce::Justification::centred);
            }
            // Número
            g.setColour (s % 4 == 0 ? col::cream : col::dim);
            g.setFont (font (10.0f, true));
            g.drawText (juce::String (s + 1), cell.withTop ((float) getHeight() - 14.0f), juce::Justification::centred);
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
    int stepAt (int x) const { return juce::jlimit (0, kSteps - 1, (int) ((float) x / ((float) getWidth() / kSteps))); }
    SeqContext ctx;
    int dragMode = 0, lastStep = -1;
};

// Barra de valores por paso (se dibuja arrastrando)
class StepLane : public juce::Component
{
public:
    explicit StepLane (SeqContext c) : ctx (c) {}
    void setMode (LaneMode m) { mode = m; repaint(); }
    LaneMode getMode() const { return mode; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::lcd);
        g.fillRoundedRectangle (r, 4.0f);
        const int t = ctx.track(), p = ctx.pattern();
        const int len = ctx.bank().getLength (p, t);
        const float w = r.getWidth() / (float) kSteps;
        const auto tc = col::track (t);
        const int root = (int) ctx.proc.value (P::Root);
        const int base = t == Bass ? 36 + root : (t == Chord ? 48 + root : 60);

        // Guías
        g.setColour (col::line.withAlpha (0.5f));
        for (int s = 4; s < kSteps; s += 4) g.fillRect (r.getX() + w * (float) s - 0.5f, r.getY() + 4.0f, 1.0f, r.getHeight() - 8.0f);
        if (mode == LaneMode::Note)
        {
            g.setColour (col::line.withAlpha (0.8f));
            g.fillRect (r.getX() + 2.0f, r.getCentreY(), r.getWidth() - 4.0f, 1.0f);
        }

        for (int s = 0; s < kSteps; ++s)
        {
            const Step st = ctx.bank().get (p, t, s);
            const bool active = st.on && s < len;
            auto cr = juce::Rectangle<float> (r.getX() + w * (float) s, r.getY(), w, r.getHeight()).reduced (6.0f, 8.0f);
            const auto c = active ? tc : col::dim.withAlpha (0.25f);
            g.setColour (c);
            juce::String label;
            switch (mode)
            {
                case LaneMode::Vel:
                {
                    const float f = (float) st.vel / 127.0f;
                    g.fillRect (cr.withTop (cr.getBottom() - cr.getHeight() * f));
                    label = juce::String (st.vel);
                    break;
                }
                case LaneMode::Prob:
                {
                    const float f = (float) st.prob / 100.0f;
                    g.fillRect (cr.withTop (cr.getBottom() - cr.getHeight() * f));
                    label = juce::String (st.prob) + "%";
                    break;
                }
                case LaneMode::Note:
                {
                    const float f = (float) st.note / 24.0f;
                    const float cy = cr.getCentreY();
                    const float y = cy - f * cr.getHeight() * 0.5f;
                    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (cr.getX(), juce::jmin (cy, y), cr.getRight(), juce::jmax (cy, y) + 2.0f));
                    label = (st.note > 0 ? "+" : "") + juce::String (st.note);
                    if (t >= Bass) label = juce::String (noteName (base + st.note));
                    break;
                }
                case LaneMode::Ratchet:
                {
                    const float bh = cr.getHeight() / 4.0f;
                    for (int k = 0; k < st.ratchet; ++k)
                        g.fillRect (cr.getX(), cr.getBottom() - bh * (float) (k + 1) + 2.0f, cr.getWidth(), bh - 4.0f);
                    label = "x" + juce::String (st.ratchet);
                    break;
                }
                case LaneMode::Slide:
                {
                    if (st.slide) g.fillRoundedRectangle (cr.withSizeKeepingCentre (cr.getWidth(), 18.0f), 4.0f);
                    else { g.setColour (c.withAlpha (0.4f)); g.drawRoundedRectangle (cr.withSizeKeepingCentre (cr.getWidth(), 18.0f), 4.0f, 1.0f); }
                    label = st.slide ? "LIGA" : "";
                    break;
                }
            }
            g.setColour (active ? col::cream : col::dim.withAlpha (0.6f));
            g.setFont (font (10.0f, true));
            g.drawText (label, juce::Rectangle<float> (cr.getX() - 4.0f, r.getY() + 1.0f, cr.getWidth() + 8.0f, 10.0f), juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { lastS = -1; slideSet = -1; apply (e); }
    void mouseDrag (const juce::MouseEvent& e) override { apply (e); }

private:
    void apply (const juce::MouseEvent& e)
    {
        const auto r = getLocalBounds().toFloat().reduced (0.0f, 8.0f);
        const int s = juce::jlimit (0, kSteps - 1, (int) ((float) e.x / ((float) getWidth() / kSteps)));
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
        if (onEdit) onEdit();
    }

public:
    std::function<void()> onEdit;

private:
    SeqContext ctx;
    LaneMode mode = LaneMode::Vel;
    int lastS = -1, slideSet = -1;
};

// ---------------------------------------------------------------------------
// Carril de automatización dibujable
// ---------------------------------------------------------------------------
class MotionCanvas : public juce::Component
{
public:
    MotionCanvas (Ogro2Processor& p, int l) : proc (p), lane (l) {}

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::lcd);
        g.fillRoundedRectangle (r, 4.0f);
        auto in = r.reduced (4.0f);
        const int bars = laneBars ((int) proc.value (P::lane (lane, P::LaneBars)));
        // Grilla de compases y negras
        for (int b = 0; b <= bars * 4; ++b)
        {
            const float x = in.getX() + in.getWidth() * (float) b / (float) (bars * 4);
            g.setColour (b % 4 == 0 ? col::line : col::line.withAlpha (0.35f));
            g.fillRect (x - 0.5f, in.getY(), 1.0f, in.getHeight());
        }
        g.setColour (col::line.withAlpha (0.6f));
        g.fillRect (in.getX(), in.getCentreY(), in.getWidth(), 1.0f);

        const bool active = (int) proc.value (P::lane (lane, P::LaneDst)) > 0;
        const auto c = active ? col::orange : col::dim;
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
        g.setColour (c.withAlpha (0.22f));
        g.fillPath (fill);
        g.setColour (c);
        g.strokePath (line, juce::PathStrokeType (2.0f));

        if (proc.engine.meters.playing.load())
        {
            const float ph = proc.engine.meters.lanePhase[lane].load();
            const float x = in.getX() + in.getWidth() * ph;
            g.setColour (col::cream);
            g.fillRect (x - 1.0f, in.getY(), 2.0f, in.getHeight());
        }
        if (! active)
        {
            g.setColour (col::dim);
            g.setFont (font (11.0f, true));
            g.drawText (T ("Elegí un destino y dibujá con el mouse"), in.withHeight (18.0f), juce::Justification::centred);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override { last = -1; draw (e); }
    void mouseDrag (const juce::MouseEvent& e) override { draw (e); }

private:
    void draw (const juce::MouseEvent& e)
    {
        auto in = getLocalBounds().toFloat().reduced (4.0f);
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
        g.setColour (col::lcd);
        g.fillRoundedRectangle (r, 3.0f);
        auto in = r.reduced (4.0f, 5.0f);
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
        g.setColour (col::lcdText.withAlpha (0.7f));
        g.strokePath (p, juce::PathStrokeType (1.5f));
        const float cur = proc.engine.meters.lfo[lfo].load();
        g.setColour (col::lcdText);
        g.fillEllipse (in.getRight() - 6.0f, in.getCentreY() - cur * in.getHeight() * 0.5f - 3.0f, 6.0f, 6.0f);
    }

private:
    Ogro2Processor& proc;
    int lfo;
};

} // namespace ogro2::ui
