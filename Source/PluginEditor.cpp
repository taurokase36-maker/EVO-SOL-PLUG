#include "PluginEditor.h"

using namespace evo::ui;
namespace pid = evo::params::id;

// ============================================================================
// Knob
// ============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& t,
            juce::Colour accent, bool bipolar)
    : title (t)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.getProperties().set ("bipolar", bipolar);
    slider.setVelocityBasedMode (false);
    slider.setMouseDragSensitivity (220);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);

    if (auto* p = state.getParameter (paramId))
    {
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
        slider.setTooltip (p->getName (64) + T (" — doble clic: valor por defecto"));
    }
    slider.onValueChange = [this] { repaint(); };
}

void Knob::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    g.setColour (colours::textDim);
    g.setFont (font (11.0f, true));
    g.drawText (title, r.removeFromTop (15), juce::Justification::centred);
    g.setColour (colours::text);
    g.setFont (font (12.0f));
    g.drawText (slider.getTextFromValue (slider.getValue()), r.removeFromBottom (15), juce::Justification::centred);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (15);
    r.removeFromBottom (15);
    const int s = juce::jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (s, s));
}

// ============================================================================
// Choice
// ============================================================================
Choice::Choice (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& t)
    : title (t)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId)))
    {
        int i = 1;
        for (const auto& c : p->choices) box.addItem (c, i++);
    }
    addAndMakeVisible (box);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, paramId, box);
}

void Choice::paint (juce::Graphics& g)
{
    g.setColour (colours::textDim);
    g.setFont (font (11.0f, true));
    g.drawText (title, getLocalBounds().removeFromTop (14), juce::Justification::centredLeft);
}

void Choice::resized() { box.setBounds (getLocalBounds().withTrimmedTop (16)); }

// ============================================================================
// Switch
// ============================================================================
Switch::Switch (juce::AudioProcessorValueTreeState& state, const char* paramId, const juce::String& text, juce::Colour accent)
{
    button.setButtonText (text);
    button.setColour (juce::ToggleButton::tickColourId, accent);
    addAndMakeVisible (button);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramId, button);
}

// ============================================================================
// Meter
// ============================================================================
Meter::Meter (const juce::String& l, Mode m, juce::Colour c, float range)
    : label (l), mode (m), colour (c), rangeDb (range) {}

void Meter::setValue (float db)
{
    value = db;
    // Balística suave para que se lea bien
    shown = shown + 0.35f * (value - shown);
    repaint();
}

void Meter::paint (juce::Graphics& g)
{
    using namespace juce;
    auto r = getLocalBounds().toFloat();
    const bool horizontal = r.getWidth() > r.getHeight();
    g.setFont (font (10.0f, true));

    Rectangle<float> bar;
    if (horizontal)
    {
        auto lab = r.removeFromLeft (64.0f);
        g.setColour (colours::textDim);
        g.drawText (label, lab, Justification::centredLeft);
        auto val = r.removeFromRight (44.0f);
        g.setColour (colours::text);
        const String txt = mode == Mode::Bipolar ? (shown >= 0 ? "+" : "") + String (shown, 1) + " dB"
                                                 : (mode == Mode::Reduction ? "-" + String (jmax (0.0f, shown), 1) + " dB" : String (shown, 1));
        g.drawText (txt, val, Justification::centredRight);
        bar = r.reduced (4.0f, 2.0f);
    }
    else
    {
        auto lab = r.removeFromBottom (14.0f);
        g.setColour (colours::textDim);
        g.drawText (label, lab, Justification::centred);
        bar = r.reduced (2.0f, 2.0f);
    }

    g.setColour (colours::track);
    g.fillRoundedRectangle (bar, 3.0f);

    float frac = 0.0f;
    if (mode == Mode::Level) frac = jlimit (0.0f, 1.0f, (shown + rangeDb) / rangeDb);
    else if (mode == Mode::Reduction) frac = jlimit (0.0f, 1.0f, shown / rangeDb);

    if (mode == Mode::Bipolar)
    {
        const float half = jlimit (-1.0f, 1.0f, shown / rangeDb);
        const float cx = bar.getCentreX();
        Rectangle<float> v = half >= 0 ? Rectangle<float> (cx, bar.getY(), bar.getWidth() * 0.5f * half, bar.getHeight())
                                       : Rectangle<float> (cx + bar.getWidth() * 0.5f * half, bar.getY(), -bar.getWidth() * 0.5f * half, bar.getHeight());
        g.setColour (colour);
        g.fillRoundedRectangle (v, 2.0f);
        g.setColour (colours::textDim.withAlpha (0.6f));
        g.fillRect (cx - 0.5f, bar.getY(), 1.0f, bar.getHeight());
        return;
    }

    Rectangle<float> v;
    if (horizontal)
        v = mode == Mode::Reduction ? bar.withWidth (bar.getWidth() * frac)
                                    : bar.withWidth (bar.getWidth() * frac);
    else
        v = mode == Mode::Reduction ? bar.withHeight (bar.getHeight() * frac)
                                    : bar.withTrimmedTop (bar.getHeight() * (1.0f - frac));

    Colour c = colour;
    if (mode == Mode::Level && shown > -1.0f) c = colours::rose;
    ColourGradient grad (c.brighter (0.3f), v.getX(), v.getY(), c.darker (0.2f), v.getRight(), v.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (v, 2.0f);
}

// ============================================================================
// PitchGraph
// ============================================================================
void PitchGraph::push (bool voiced, float inMidi, float outMidi, int target)
{
    head = (head + 1) % kHistory;
    voicedHist[(size_t) head] = voiced;
    inHist[(size_t) head] = inMidi;
    outHist[(size_t) head] = outMidi;
    lastVoiced = voiced;
    if (voiced)
    {
        if (target >= 0) lastTarget = target;
        lastDeviation = (inMidi - (float) target) * 100.0f;
        centre += 0.08f * (outMidi - centre);
    }
    repaint();
}

void PitchGraph::paint (juce::Graphics& g)
{
    using namespace juce;
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff0e0c13));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

    auto info = r.removeFromRight (118.0f);
    auto plot = r.reduced (10.0f, 8.0f);

    const float span = 4.5f; // semitonos a cada lado
    const float lo = centre - span, hi = centre + span;
    auto yOf = [&] (float m) { return plot.getBottom() - (m - lo) / (hi - lo) * plot.getHeight(); };

    const auto mask = evo::keyMask (currentKey, (evo::ScaleType) currentScale);
    g.setFont (font (9.5f, true));
    for (int n = (int) std::floor (lo); n <= (int) std::ceil (hi); ++n)
    {
        const float y = yOf ((float) n);
        if (y < plot.getY() - 1 || y > plot.getBottom() + 1) continue;
        const bool allowed = mask[(size_t) (((n % 12) + 12) % 12)];
        g.setColour (allowed ? colours::amber.withAlpha (n == lastTarget ? 0.55f : 0.16f) : Colours::white.withAlpha (0.035f));
        g.fillRect (plot.getX() + 22.0f, y - 0.5f, plot.getWidth() - 22.0f, n == lastTarget ? 1.6f : 1.0f);
        if (allowed)
        {
            g.setColour (colours::textDim.withAlpha (0.9f));
            g.drawText (String (evo::noteNames()[((n % 12) + 12) % 12]) + String (n / 12 - 1),
                        Rectangle<float> (plot.getX(), y - 6.0f, 24.0f, 12.0f), Justification::centredLeft);
        }
    }

    // Trazos: entrada (gris) y corregido (sol)
    auto drawTrace = [&] (const std::array<float, kHistory>& hist, Colour c, float thickness)
    {
        Path p;
        bool open = false;
        for (int i = 0; i < kHistory; ++i)
        {
            const int idx = (head + 1 + i) % kHistory;
            const float x = plot.getX() + 24.0f + (plot.getWidth() - 24.0f) * (float) i / (float) (kHistory - 1);
            if (! voicedHist[(size_t) idx]) { open = false; continue; }
            const float y = jlimit (plot.getY(), plot.getBottom(), yOf (hist[(size_t) idx]));
            if (! open) { p.startNewSubPath (x, y); open = true; }
            else p.lineTo (x, y);
        }
        g.setColour (c);
        g.strokePath (p, PathStrokeType (thickness, PathStrokeType::curved, PathStrokeType::rounded));
    };
    drawTrace (inHist, Colours::white.withAlpha (0.35f), 1.4f);
    drawTrace (outHist, colours::sun, 2.4f);

    // Panel de nota
    info = info.reduced (8.0f, 10.0f);
    g.setColour (colours::textDim);
    g.setFont (font (10.0f, true));
    g.drawText ("NOTA", info.removeFromTop (14.0f), Justification::centred);
    const String note = lastTarget >= 0 ? String (evo::noteNames()[lastTarget % 12]) + String (lastTarget / 12 - 1) : String ("--");
    g.setColour (lastVoiced ? colours::gold : colours::textDim);
    g.setFont (font (34.0f, true));
    g.drawText (note, info.removeFromTop (44.0f), Justification::centred);
    g.setFont (font (12.0f, true));
    const float dev = lastVoiced ? lastDeviation : 0.0f;
    g.setColour (std::abs (dev) < 10.0f ? colours::good : colours::rose);
    g.drawText (lastVoiced ? ((dev >= 0 ? "+" : "") + String (roundToInt (dev)) + T (" ¢ entrada")) : String ("sin voz"),
                info.removeFromTop (16.0f), Justification::centred);

    // Barra de desvío (±50 cents)
    auto bar = info.removeFromTop (16.0f).reduced (6.0f, 5.0f);
    g.setColour (colours::track);
    g.fillRoundedRectangle (bar, 3.0f);
    if (lastVoiced)
    {
        const float frac = jlimit (-1.0f, 1.0f, dev / 50.0f);
        const float cx = bar.getCentreX();
        g.setColour (std::abs (dev) < 10.0f ? colours::good : colours::sun);
        g.fillRoundedRectangle (frac >= 0 ? Rectangle<float> (cx, bar.getY(), bar.getWidth() * 0.5f * frac, bar.getHeight())
                                          : Rectangle<float> (cx + bar.getWidth() * 0.5f * frac, bar.getY(), -bar.getWidth() * 0.5f * frac, bar.getHeight()),
                                2.0f);
    }
    g.setColour (colours::text);
    g.fillRect (bar.getCentreX() - 0.5f, bar.getY() - 2.0f, 1.0f, bar.getHeight() + 4.0f);
}

// ============================================================================
// SunLogo
// ============================================================================
void SunLogo::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto r = getLocalBounds().toFloat();
    const auto c = r.getCentre();
    const float R = jmin (r.getWidth(), r.getHeight()) * 0.5f;

    // Rayos
    for (int i = 0; i < 12; ++i)
    {
        const float a = (float) i / 12.0f * MathConstants<float>::twoPi + pulse * 0.25f;
        const float r0 = R * 0.62f, r1 = R * (i % 2 == 0 ? 0.98f : 0.86f);
        g.setColour ((i % 2 == 0 ? colours::amber : colours::sun).withAlpha (0.9f));
        g.drawLine (c.x + r0 * std::cos (a), c.y + r0 * std::sin (a), c.x + r1 * std::cos (a), c.y + r1 * std::sin (a), R * 0.09f);
    }
    // Disco
    const float d = R * 0.5f;
    ColourGradient grad (colours::gold, c.x - d * 0.4f, c.y - d * 0.5f, colours::sun, c.x + d, c.y + d, true);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - d, c.y - d, d * 2.0f, d * 2.0f);
}

// ============================================================================
// Content
// ============================================================================
Content::Content (EvoSolProcessor& p)
    : proc (p), st (p.state),
      inGain (st, pid::inGain, "ENTRADA", colours::amber, true),
      outGain (st, pid::outGain, "SALIDA", colours::amber, true),
      ceiling (st, pid::ceiling, "TECHO", colours::rose),
      inMeter ("IN", Meter::Mode::Level, colours::good, 48.0f),
      outMeterL ("L", Meter::Mode::Level, colours::amber, 48.0f),
      outMeterR ("R", Meter::Mode::Level, colours::amber, 48.0f),
      tuneOn (st, pid::tuneOn, "ON", colours::sun),
      key (st, pid::key, "KEY"),
      scale (st, pid::scale, "ESCALA"),
      range (st, pid::range, "VOZ"),
      retune (st, pid::retune, "VELOCIDAD", colours::sun),
      humanize (st, pid::humanize, "HUMANIZAR", colours::sun),
      flex (st, pid::flex, "FLEX-TUNE", colours::sun),
      vibrato (st, pid::vibrato, "VIBRATO", colours::sun, true),
      throat (st, pid::throat, "GARGANTA", colours::sun, true),
      transpose (st, pid::transpose, "TRANSPONER", colours::sun, true),
      detune (st, pid::detune, "REFERENCIA", colours::sun, true),
      tracking (st, pid::tracking, "TRACKING", colours::sun),
      riderOn (st, pid::riderOn, T ("Auto-ganancia"), colours::violet),
      riderTarget (st, pid::riderTarget, "OBJETIVO", colours::violet),
      riderRange (st, pid::riderRange, "RANGO", colours::violet),
      smooth (st, pid::smooth, "SUAVIZADO", colours::violet),
      deEss (st, pid::deEss, "DE-ESSER", colours::violet),
      riderMeter ("RIDER", Meter::Mode::Bipolar, colours::violet, 12.0f),
      smoothMeter ("SUAVIZADO", Meter::Mode::Reduction, colours::violet, 8.0f),
      deEssMeter ("DE-ESSER", Meter::Mode::Reduction, colours::violet, 12.0f),
      coreOn (st, pid::coreOn, "ON", colours::amber),
      hpf (st, pid::hpf, "LOW-CUT", colours::amber),
      body (st, pid::body, "CUERPO", colours::amber, true),
      presence (st, pid::presence, "PRESENCIA", colours::amber, true),
      air (st, pid::air, "AIRE", colours::amber),
      comp (st, pid::comp, T ("COMPRESIÓN"), colours::amber),
      color (st, pid::color, "COLOR", colours::amber),
      warmth (st, pid::warmth, "CALOR", colours::amber),
      parallel (st, pid::parallel, "MEZCLA", colours::amber),
      fetMeter ("FET", Meter::Mode::Reduction, colours::amber, 18.0f),
      optoMeter ("OPTO", Meter::Mode::Reduction, colours::amber, 12.0f),
      limMeter ("LIMIT", Meter::Mode::Reduction, colours::rose, 12.0f),
      harm1 (st, pid::harm1, T ("ARMONÍA 1")),
      harm2 (st, pid::harm2, T ("ARMONÍA 2")),
      delayDiv (st, pid::delayDiv, "TIEMPO DELAY"),
      harmLevel (st, pid::harmLevel, T ("ARMONÍAS"), colours::rose),
      doubler (st, pid::doubler, "DOBLE", colours::rose),
      radio (st, pid::radio, "RADIO", colours::rose),
      width (st, pid::width, "ANCHO", colours::rose),
      duck (st, pid::duck, "DUCKING", colours::rose),
      delayMix (st, pid::delayMix, "DELAY", colours::rose),
      delayFb (st, pid::delayFb, "FEEDBACK", colours::rose),
      delayTone (st, pid::delayTone, "TONO", colours::rose),
      reverbMix (st, pid::reverbMix, "REVERB", colours::rose),
      reverbSize (st, pid::reverbSize, T ("TAMAÑO"), colours::rose),
      reverbDecay (st, pid::reverbDecay, "DECAY", colours::rose),
      reverbPre (st, pid::reverbPre, "PRE-DELAY", colours::rose)
{
    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &logo, &presetBox, &prevPreset, &nextPreset, &inGain, &outGain, &ceiling, &inMeter, &outMeterL, &outMeterR,
             &tuneOn, &key, &scale, &range, &pitchGraph, &retune, &humanize, &flex, &vibrato, &throat, &transpose,
             &detune, &tracking, &riderOn, &riderTarget, &riderRange, &smooth, &deEss, &analyseButton, &applyButton,
             &assistantStatus, &riderMeter, &smoothMeter, &deEssMeter, &coreOn, &hpf, &body, &presence, &air, &comp,
             &color, &warmth, &parallel, &fetMeter, &optoMeter, &limMeter, &harm1, &harm2, &delayDiv, &harmLevel,
             &doubler, &radio, &width, &duck, &delayMix, &delayFb, &delayTone, &reverbMix, &reverbSize, &reverbDecay,
             &reverbPre })
        addAndMakeVisible (c);

    // Presets
    const auto& list = evo::presets::factory();
    for (int i = 0; i < (int) list.size(); ++i) presetBox.addItem (list[(size_t) i].name, i + 1);
    presetBox.setSelectedId (proc.getPresetIndex() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && idx != proc.getPresetIndex()) proc.loadPreset (idx);
    };
    presetBox.setTooltip (T ("Presets de fábrica (no cambian tu key ni escala)"));
    auto step = [this] (int d) {
        const int n = presetBox.getNumItems();
        const int idx = (proc.getPresetIndex() + d + n) % n;
        proc.loadPreset (idx);
        presetBox.setSelectedId (idx + 1, juce::dontSendNotification);
    };
    prevPreset.onClick = [step] { step (-1); };
    nextPreset.onClick = [step] { step (1); };

    // Asistente
    analyseButton.setButtonText ("ANALIZAR VOZ");
    analyseButton.setColour (juce::TextButton::buttonOnColourId, colours::violet);
    analyseButton.setTooltip (T ("Reproducí 10-20 segundos de la voz mientras escucha"));
    analyseButton.onClick = [this] {
        if (proc.isAnalysing()) proc.stopAnalysis();
        else { hasSuggestion = false; proc.startAnalysis(); }
    };
    applyButton.setButtonText ("APLICAR");
    applyButton.setColour (juce::TextButton::buttonOnColourId, colours::good);
    applyButton.setEnabled (false);
    applyButton.onClick = [this] {
        if (hasSuggestion) proc.applySuggestion (lastSuggestion);
    };
    assistantStatus.setFont (font (12.5f));
    assistantStatus.setColour (juce::Label::textColourId, colours::text);
    assistantStatus.setJustificationType (juce::Justification::topLeft);
    assistantStatus.setMinimumHorizontalScale (1.0f);

    // Tooltips de ayuda
    retune.slider.setTooltip (T ("Velocidad de afinación: 0 = efecto robot/hard-tune, valores altos = natural"));
    humanize.slider.setTooltip (T ("Afina más suave las notas largas para que suenen naturales"));
    flex.slider.setTooltip (T ("Solo corrige cuando te acercás a la nota: conserva tu expresión"));
    vibrato.slider.setTooltip (T ("Quita (−) o exagera (+) el vibrato natural"));
    throat.slider.setTooltip (T ("Mueve los formantes: voz más grave/oscura (−) o más fina (+)"));
    smooth.slider.setTooltip (T ("Detecta resonancias molestas en tiempo real y las suaviza solo cuando aparecen"));
    deEss.slider.setTooltip (T ("De-esser que aprende el brillo de tu voz y controla las eses a cualquier nivel"));
    comp.slider.setTooltip (T ("Macro de compresión: FET rápido + óptico nivelador"));
    warmth.slider.setTooltip (T ("Saturación analógica válvula + cinta (sobremuestreada 4x)"));
    parallel.slider.setTooltip (T ("Compresión en paralelo: mezcla la señal comprimida con la original"));

    startTimerHz (30);
    updateAssistant();
}

Content::~Content() { stopTimer(); }

void Content::layoutKnobRow (std::initializer_list<Knob*> knobs, juce::Rectangle<int> area)
{
    const int n = (int) knobs.size();
    const int w = area.getWidth() / juce::jmax (1, n);
    int i = 0;
    for (auto* k : knobs)
        k->setBounds (area.getX() + i++ * w, area.getY(), w, area.getHeight());
}

void Content::resized()
{
    headerArea = { 0, 0, kWidth, 78 };
    tuneArea = { 14, 88, 690, 318 };
    aiArea = { 716, 88, 390, 318 };
    coreArea = { 14, 418, 500, 328 };
    fxArea = { 526, 418, 580, 328 };

    // Cabecera
    logo.setBounds (16, 10, 58, 58);
    presetBox.setBounds (448, 26, 230, 30);
    prevPreset.setBounds (414, 26, 30, 30);
    nextPreset.setBounds (682, 26, 30, 30);
    inGain.setBounds (790, 6, 66, 68);
    outGain.setBounds (856, 6, 66, 68);
    ceiling.setBounds (922, 6, 66, 68);
    inMeter.setBounds (1010, 10, 22, 60);
    outMeterL.setBounds (1046, 10, 22, 60);
    outMeterR.setBounds (1070, 10, 22, 60);

    // Afinación
    {
        auto r = tuneArea;
        tuneOn.setBounds (r.getRight() - 84, r.getY() + 7, 76, 22);
        key.setBounds (r.getX() + 14, r.getY() + 44, 80, 42);
        range.setBounds (r.getX() + 100, r.getY() + 44, 96, 42);
        scale.setBounds (r.getX() + 14, r.getY() + 92, 182, 42);
        pitchGraph.setBounds (r.getX() + 208, r.getY() + 44, r.getWidth() - 222, 140);
        layoutKnobRow ({ &retune, &humanize, &flex, &vibrato, &throat, &transpose, &detune, &tracking },
                       { r.getX() + 10, r.getY() + 192, r.getWidth() - 20, 116 });
    }
    // Inteligencia
    {
        auto r = aiArea;
        analyseButton.setBounds (r.getX() + 14, r.getY() + 44, 196, 34);
        applyButton.setBounds (r.getX() + 218, r.getY() + 44, r.getWidth() - 232, 34);
        assistantStatus.setBounds (r.getX() + 10, r.getY() + 82, r.getWidth() - 20, 62);
        layoutKnobRow ({ &riderTarget, &riderRange, &smooth, &deEss }, { r.getX() + 10, r.getY() + 148, r.getWidth() - 20, 108 });
        riderOn.setBounds (r.getX() + 14, r.getY() + 276, 140, 22);
        riderMeter.setBounds (r.getX() + 168, r.getY() + 262, r.getWidth() - 182, 15);
        smoothMeter.setBounds (r.getX() + 168, r.getY() + 279, r.getWidth() - 182, 15);
        deEssMeter.setBounds (r.getX() + 168, r.getY() + 296, r.getWidth() - 182, 15);
    }
    // Núcleo
    {
        auto r = coreArea;
        coreOn.setBounds (r.getRight() - 84, r.getY() + 7, 76, 22);
        const int knobsW = r.getWidth() - 20 - 92;
        layoutKnobRow ({ &hpf, &body, &presence, &air }, { r.getX() + 10, r.getY() + 46, knobsW, 132 });
        layoutKnobRow ({ &comp, &color, &warmth, &parallel }, { r.getX() + 10, r.getY() + 184, knobsW, 132 });
        const int mx = r.getRight() - 96;
        fetMeter.setBounds (mx, r.getY() + 52, 26, 262);
        optoMeter.setBounds (mx + 30, r.getY() + 52, 26, 262);
        limMeter.setBounds (mx + 60, r.getY() + 52, 26, 262);
    }
    // Ear candy
    {
        auto r = fxArea;
        harm1.setBounds (r.getX() + 14, r.getY() + 42, 168, 42);
        harm2.setBounds (r.getX() + 192, r.getY() + 42, 168, 42);
        delayDiv.setBounds (r.getX() + 398, r.getY() + 42, 168, 42);
        const int slot = (r.getWidth() - 20) / 7;
        layoutKnobRow ({ &harmLevel, &doubler, &radio, &width, &duck }, { r.getX() + 10, r.getY() + 90, slot * 7, 112 });
        layoutKnobRow ({ &delayMix, &delayFb, &delayTone, &reverbMix, &reverbSize, &reverbDecay, &reverbPre },
                       { r.getX() + 10, r.getY() + 208, slot * 7, 112 });
    }
}

void Content::drawPanel (juce::Graphics& g, juce::Rectangle<int> ri, const juce::String& title,
                         const juce::String& subtitle, juce::Colour accent)
{
    using namespace juce;
    auto r = ri.toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);

    // Línea de acento arriba
    ColourGradient grad (accent, r.getX() + 14.0f, r.getY(), accent.withAlpha (0.0f), r.getX() + r.getWidth() * 0.6f, r.getY(), false);
    g.setGradientFill (grad);
    g.fillRect (r.getX() + 14.0f, r.getY() + 34.0f, r.getWidth() * 0.6f, 1.0f);

    g.setColour (accent);
    g.fillEllipse (r.getX() + 16.0f, r.getY() + 14.0f, 7.0f, 7.0f);
    g.setColour (colours::text);
    g.setFont (font (14.5f, true));
    g.drawText (title, Rectangle<float> (r.getX() + 30.0f, r.getY() + 6.0f, 260.0f, 22.0f), Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (font (11.5f));
    const float tw = GlyphArrangement::getStringWidth (font (14.5f, true), title);
    g.drawText (subtitle, Rectangle<float> (r.getX() + 40.0f + tw, r.getY() + 7.0f, r.getWidth() - 140.0f - tw, 22.0f),
                Justification::centredLeft);
}

void Content::paint (juce::Graphics& g)
{
    using namespace juce;
    // Fondo con un leve resplandor cálido arriba
    g.fillAll (colours::bg);
    ColourGradient glow (colours::sun.withAlpha (0.13f), 60.0f, 0.0f, colours::bg.withAlpha (0.0f), 520.0f, 300.0f, true);
    g.setGradientFill (glow);
    g.fillRect (getLocalBounds());

    // Cabecera
    g.setColour (colours::text);
    g.setFont (font (28.0f, true));
    g.drawText ("EVO SOL", 84, 12, 220, 32, Justification::centredLeft);
    g.setColour (colours::amber);
    g.setFont (font (11.0f, true));
    g.drawText (T ("VOCAL SUITE  ·  EVO THE SUN"), 86, 44, 260, 16, Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (font (10.5f, true));
    g.drawText ("PRESET", 448, 8, 230, 16, Justification::centredLeft);

    drawPanel (g, tuneArea, T ("AFINACIÓN"), T ("corrección en key · humanize · formantes"), colours::sun);
    drawPanel (g, aiArea, "INTELIGENCIA", T ("asistente + procesos adaptativos"), colours::violet);
    drawPanel (g, coreArea, T ("NÚCLEO ANALÓGICO"), T ("EQ · FET · óptico · saturación"), colours::amber);
    drawPanel (g, fxArea, "EAR CANDY", T ("armonías · doble · delay · reverb"), colours::rose);
}

void Content::updateAssistant()
{
    using namespace juce;
    if (proc.isAnalysing())
    {
        analyseButton.setButtonText ("DETENER Y CALCULAR");
        analyseButton.setToggleState (true, dontSendNotification);
        String s = T ("Escuchando… ") + String (proc.analysisSeconds(), 1) + T (" s de voz analizados\n");
        int k; bool minor; float conf;
        if (proc.liveKey (k, minor, conf))
            s << T ("Key probable: ") << evo::noteNames()[k] << (minor ? " menor" : " mayor")
              << " (" << roundToInt (conf * 100.0f) << " %)";
        else
            s << T ("Reproducí la voz sola (10-20 s ideal).");
        assistantStatus.setText (s, dontSendNotification);
        return;
    }

    analyseButton.setButtonText ("ANALIZAR VOZ");
    analyseButton.setToggleState (false, dontSendNotification);

    evo::VocalAssistant::Suggestion s;
    if (! hasSuggestion && proc.getSuggestion (s))
    {
        lastSuggestion = s;
        hasSuggestion = true;
    }

    if (hasSuggestion && lastSuggestion.valid)
    {
        const auto& x = lastSuggestion;
        auto sg = [] (float v) { return (v >= 0 ? "+" : "") + String (v, 1); };
        String t;
        t << T ("Listo: ") << evo::noteNames()[x.key] << (x.minor ? " menor" : " mayor") << " ("
          << roundToInt (x.keyConfidence * 100.0f) << T (" %) · voz ") << evo::voiceRangeNames()[x.range] << "\n"
          << "Low-cut " << roundToInt (x.hpfHz) << T (" Hz · Cuerpo ") << sg (x.bodyDb) << T (" · Presencia ") << sg (x.presenceDb)
          << "\nAire " << sg (x.airDb) << T (" dB · De-esser ") << roundToInt (x.deEss * 100.0f) << T (" % · Entrada ") << sg (x.inputGainDb) << " dB";
        assistantStatus.setText (t, dontSendNotification);
        applyButton.setEnabled (true);
    }
    else if (hasSuggestion)
    {
        assistantStatus.setText (T ("No alcanzó: necesito al menos 2 segundos de voz cantada.\nVolvé a analizar con la voz sonando."),
                                 dontSendNotification);
        applyButton.setEnabled (false);
    }
    else
    {
        assistantStatus.setText (T ("Tocá ANALIZAR y reproducí la voz. EVO SOL detecta la key,\n"
                                    "el registro, el balance tonal y las eses, y te propone\n"
                                    "un ajuste para toda la cadena."), dontSendNotification);
        applyButton.setEnabled (false);
    }
}

void Content::timerCallback()
{
    auto& m = proc.engine.meters;
    const int k = juce::roundToInt (st.getRawParameterValue (pid::key)->load());
    const int sc = juce::roundToInt (st.getRawParameterValue (pid::scale)->load());
    pitchGraph.setScale (k, sc);
    pitchGraph.push (m.voiced.load(), m.inMidi.load(), m.outMidi.load(), m.targetNote.load());

    inMeter.setValue (m.inDb.load());
    outMeterL.setValue (m.outDbL.load());
    outMeterR.setValue (m.outDbR.load());
    riderMeter.setValue (m.riderDb.load());
    smoothMeter.setValue (m.smoothGr.load());
    deEssMeter.setValue (m.deEssGr.load());
    fetMeter.setValue (m.fetGr.load());
    optoMeter.setValue (m.optoGr.load());
    limMeter.setValue (m.limGr.load());

    if (presetBox.getSelectedId() != proc.getPresetIndex() + 1)
        presetBox.setSelectedId (proc.getPresetIndex() + 1, juce::dontSendNotification);

    sunPhase += 0.01f;
    logo.setPulse (sunPhase);
    updateAssistant();
}

// ============================================================================
// Editor
// ============================================================================
EvoSolEditor::EvoSolEditor (EvoSolProcessor& p)
    : AudioProcessorEditor (p), content (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    const double ratio = (double) Content::kWidth / (double) Content::kHeight;
    setResizable (true, true);
    setResizeLimits (Content::kWidth * 3 / 4, Content::kHeight * 3 / 4, Content::kWidth * 3 / 2, Content::kHeight * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (ratio);
    setSize (Content::kWidth, Content::kHeight);
}

EvoSolEditor::~EvoSolEditor() { setLookAndFeel (nullptr); }

void EvoSolEditor::paint (juce::Graphics& g) { g.fillAll (colours::bg); }

void EvoSolEditor::resized()
{
    const float scale = (float) getWidth() / (float) Content::kWidth;
    content.setBounds (0, 0, Content::kWidth, Content::kHeight);
    content.setTransform (juce::AffineTransform::scale (scale));
}
