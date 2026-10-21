#include "PluginEditor.h"

using namespace ogro2;
using namespace ogro2::ui;

namespace
{
const char* trackDescription (int t)
{
    static const char* d[] = {
        "Seno con envolvente de pitch, click y drive analógico",
        "Ráfagas de ruido filtrado con room corto",
        "Rim / snare: dos modos resonantes, click y ruido",
        "6 osciladores cuadrados inarmónicos + ruido · corta al hat abierto",
        "Hat abierto: lo corta el hat cerrado, como en las máquinas clásicas",
        "FM de 2 operadores con pitch: congas, toms, bleeps y campanas",
        "Sierra / cuadrada / sub · filtro escalera de 4 polos · acento y glide",
        "Polifónico: 2 sierras por nota, filtro con envolvente y chorus",
    };
    return d[juce::jlimit (0, 7, t)];
}

juce::String upperName (int t, int p) { return juce::String::fromUTF8 (soundParamInfo (t, p).name); }

// Rótulo de tarjeta violeta en coordenadas locales
void cardTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& s)
{
    g.setColour (col::violetText);
    drawSpaced (g, s.toUpperCase(), r.toFloat().withHeight (18.0f).translated (12.0f, 8.0f), labelFont (13.0f), 0.08f,
                juce::Justification::centredLeft);
}

// ---------------------------------------------------------------------------
// PISTA: inspector de la pista elegida
// ---------------------------------------------------------------------------
class TrackPage : public Page
{
public:
    explicit TrackPage (Ogro2Processor& p)
        : proc (p),
          level (p, P::track (0, P::Level), "NIVEL"),
          pan (p, P::track (0, P::Pan), "PANEO", Theme::Violet, true),
          dly (p, P::track (0, P::Delay), "DELAY"),
          rev (p, P::track (0, P::Reverb), "REVERB"),
          pump (p, P::track (0, P::Pump), "PUMP"),
          ring (p, P::track (0, P::Ring), "MANDAR AL RING", col::violetRing)
    {
        for (int i = 0; i < kSoundParams; ++i)
            addAndMakeVisible (sound.add (new Knob (p, P::sound (0, i), upperName (0, i))));
        for (auto* c : std::initializer_list<juce::Component*> { &level, &pan, &dly, &rev, &pump, &ring, &audition })
            addAndMakeVisible (c);
        level.setTravel (420);
        for (auto* k : { &dly, &rev, &pump }) k->setRing (juce::Colour (0xfff2a466));
        audition.setButtonText ("PROBAR");
        styleKey (audition, col::orange);
        audition.onClick = [this] { proc.engine.audition (track); };
        audition.setTooltip (T ("Dispara el sonido para escucharlo (también con el transporte parado)"));
        pump.slider.setTooltip (T ("Cuánto se agacha esta pista con cada kick (sidechain fantasma, aunque el kick esté muteado)"));
        ring.button.setTooltip (T ("Manda esta pista por el ring mod (el ring se ajusta en el panel RING)"));
        trackChanged (0);
    }
    juce::String title() const override { return trackName (track); }

    void trackChanged (int t) override
    {
        track = t;
        for (int i = 0; i < kSoundParams; ++i) sound[i]->bind (P::sound (t, i), upperName (t, i));
        level.bind (P::track (t, P::Level), "NIVEL");
        pan.bind (P::track (t, P::Pan), "PANEO");
        dly.bind (P::track (t, P::Delay), "DELAY");
        rev.bind (P::track (t, P::Reverb), "REVERB");
        pump.bind (P::track (t, P::Pump), "PUMP");
        ring.bind (P::track (t, P::Ring));
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        // Encabezado
        g.setColour (col::track (track).withAlpha (0.35f));
        g.fillRoundedRectangle (0.0f, 2.0f, 8.0f, 28.0f, 4.0f);
        g.setColour (col::track (track));
        g.fillRoundedRectangle (1.5f, 4.0f, 5.0f, 24.0f, 2.5f);
        g.setColour (col::violetText);
        drawSpaced (g, trackName (track), { 18.0f, 0.0f, 200.0f, 32.0f }, titleFont (19.0f), 0.1f, juce::Justification::centredLeft);
        const float nameW = (float) juce::GlyphArrangement::getStringWidthInt (titleFont (19.0f), trackName (track)) * 1.1f + 34.0f;
        g.setColour (col::violetValue);
        g.setFont (textFont (14.0f));
        g.drawText (T (trackDescription (track)), juce::Rectangle<float> (nameW, 0.0f, (float) getWidth() - nameW - 130.0f, 32.0f),
                    juce::Justification::centredLeft, true);
        // Tarjetas de abajo
        paint::violetCard (g, mixBox.toFloat(), T ("Mezcla"));
        paint::violetCard (g, sendBox.toFloat(), T ("Envíos"));
        paint::violetCard (g, ringBox.toFloat(), T ("Ring mod"));
        // Traza de cobre: camino de la señal hacia los efectos
        const float ty = (float) dly.getBounds().getCentreY() - 4.0f;
        paint::copperTrace (g, { (float) dly.getBounds().getCentreX(), ty }, { (float) pump.getBounds().getCentreX(), ty }, 4.0f);
        g.setColour (col::violetValue);
        g.setFont (textFont (13.0f));
        g.drawFittedText (T ("La pista pasa por el ring modulator. La frecuencia y la forma se ajustan en el panel RING."),
                          ringBox.reduced (12).withTrimmedTop (76), juce::Justification::topLeft, 5);
    }
    void resized() override
    {
        audition.setBounds (getWidth() - 120, 0, 120, 32);
        const int w = getWidth() / kSoundParams;
        for (int i = 0; i < kSoundParams; ++i) sound[i]->setBounds (i * w, 44, w, 104);
        const int y = 162, h = getHeight() - y;
        mixBox = { 0, y, 196, h };
        sendBox = { 208, y, 236, h };
        ringBox = { 456, y, getWidth() - 456, h };
        const int ky = y + 40, kh = 112;
        level.setBounds (mixBox.getX() + 8, ky, 86, kh);
        pan.setBounds (mixBox.getX() + 100, ky, 86, kh);
        dly.setBounds (sendBox.getX() + 6, ky, 74, kh);
        rev.setBounds (sendBox.getX() + 81, ky, 74, kh);
        pump.setBounds (sendBox.getX() + 156, ky, 74, kh);
        ring.setBounds (ringBox.getX() + 12, y + 36, ringBox.getWidth() - 24, 32);
    }

private:
    Ogro2Processor& proc;
    int track = 0;
    juce::OwnedArray<Knob> sound;
    Knob level, pan, dly, rev, pump;
    Toggle ring;
    juce::TextButton audition;
    juce::Rectangle<int> mixBox, sendBox, ringBox;
};

// ---------------------------------------------------------------------------
// FX: delay, reverb, pump y drum bus
// ---------------------------------------------------------------------------
class FxPage : public Page
{
public:
    explicit FxPage (Ogro2Processor& p)
        : proc (p),
          dlyDiv (p, P::DlyDiv, "TIEMPO"),
          dlyFb (p, P::DlyFb, "FEEDBACK"), dlyTone (p, P::DlyTone, "TONO"), dlyRet (p, P::DlyRet, "RETORNO"),
          revSize (p, P::RevSize, T ("TAMAÑO")), revDecay (p, P::RevDecay, "DECAY"), revRet (p, P::RevRet, "RETORNO"),
          pumpRel (p, P::PumpRel, "TIEMPO"),
          busComp (p, P::BusComp, T ("COMPRESIÓN")), busCrudo (p, P::BusCrudo, "CRUDO"),
          gr (T ("GR"), true, 18.0f)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &dlyDiv, &dlyFb, &dlyTone, &dlyRet, &revSize, &revDecay, &revRet,
                                                                 &pumpRel, &busComp, &busCrudo, &gr })
            addAndMakeVisible (c);
        for (auto* k : { &busComp, &busCrudo }) k->setRing (juce::Colour (0xfff2a466));
    }
    juce::String title() const override { return "FX"; }
    void tick() override { gr.set (proc.engine.meters.drumGr.load()); }
    void paint (juce::Graphics& g) override
    {
        paint::violetCard (g, b1.toFloat(), T ("Delay ping-pong"));
        paint::violetCard (g, b2.toFloat(), "Reverb");
        paint::violetCard (g, b3.toFloat(), "Pump");
        paint::violetCard (g, b4.toFloat(), T ("Drum bus"));
        g.setColour (col::violetValue);
        g.setFont (textFont (13.0f));
        g.drawFittedText (T ("Con cada kick, las pistas con PUMP se agachan y vuelven en este tiempo: el bombeo clásico del house."),
                          b3.reduced (12).withTrimmedTop (30).withTrimmedLeft (96), juce::Justification::topLeft, 5);
        g.drawFittedText (T ("Kick, clap, rim, hats y perc pasan por un compresor rápido y saturación de cinta y válvula."),
                          b4.reduced (12).withTrimmedTop (30).withTrimmedLeft (196).withTrimmedRight (34), juce::Justification::topLeft, 5);
    }
    void resized() override
    {
        const int w = (getWidth() - 12) / 2, h = (getHeight() - 12) / 2;
        b1 = { 0, 0, w, h }; b2 = { w + 12, 0, w, h }; b3 = { 0, h + 12, w, h }; b4 = { w + 12, h + 12, w, h };
        dlyDiv.setBounds (b1.getX() + 12, b1.getY() + 36, 86, 50);
        int x = b1.getX() + 104;
        for (auto* k : { &dlyFb, &dlyTone, &dlyRet }) { k->setBounds (x, b1.getY() + 30, 64, h - 40); x += 64; }
        x = b2.getX() + 18;
        for (auto* k : { &revSize, &revDecay, &revRet }) { k->setBounds (x, b2.getY() + 30, 88, h - 40); x += 92; }
        pumpRel.setBounds (b3.getX() + 8, b3.getY() + 30, 86, h - 40);
        busComp.setBounds (b4.getX() + 6, b4.getY() + 30, 90, h - 40);
        busCrudo.setBounds (b4.getX() + 98, b4.getY() + 30, 90, h - 40);
        gr.setBounds (b4.getRight() - 34, b4.getY() + 34, 22, h - 44);
    }

private:
    Ogro2Processor& proc;
    Choice dlyDiv;
    Knob dlyFb, dlyTone, dlyRet, revSize, revDecay, revRet, pumpRel, busComp, busCrudo;
    Meter gr;
    juce::Rectangle<int> b1, b2, b3, b4;
};

// ---------------------------------------------------------------------------
// MOD: 4 LFOs y matriz de 12 conexiones
// ---------------------------------------------------------------------------
class ModPage : public Page
{
public:
    explicit ModPage (Ogro2Processor& p) : proc (p)
    {
        for (int l = 0; l < P::kLfos; ++l)
        {
            addAndMakeVisible (shapes.add (new Choice (p, P::lfo (l, P::LfoShape), "")));
            addAndMakeVisible (rates.add (new Choice (p, P::lfo (l, P::LfoRate), "")));
            addAndMakeVisible (scopes.add (new LfoScope (p, l)));
        }
        for (int s = 0; s < P::kSlots; ++s)
        {
            addAndMakeVisible (srcs.add (new Choice (p, P::mod (s, P::ModSrc), "")));
            addAndMakeVisible (dsts.add (new Choice (p, P::mod (s, P::ModDst), "", true)));
            auto* amt = amts.add (new juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox));
            amt->getProperties().set ("bipolar", true);
            amt->setColour (juce::Slider::trackColourId, col::violetRing);
            amt->setDoubleClickReturnValue (true, 0.0);
            addAndMakeVisible (amt);
            amtAtt.add (new APVTS::SliderAttachment (p.state, juce::String (paramDefs()[(size_t) P::mod (s, P::ModAmt)].id), *amt));
            amt->setTooltip (T ("Cantidad (doble clic = 0)"));
        }
    }
    juce::String title() const override { return T ("Modulación"); }
    void tick() override { for (auto* s : scopes) s->repaint(); }

    void paint (juce::Graphics& g) override
    {
        paint::violetCard (g, lfoBox.toFloat(), T ("LFOs sincronizados"));
        paint::violetCard (g, matBox.toFloat(), T ("Matriz · fuente → destino → cantidad"));
        g.setColour (col::violetRing);
        for (int l = 0; l < P::kLfos; ++l)
            drawSpaced (g, juce::String (l + 1), { (float) lfoBox.getX() + 10.0f, (float) rowY (l), 18.0f, 30.0f }, titleFont (15.0f), 0.0f,
                        juce::Justification::centred);
        g.setColour (col::violetValue);
        g.setFont (labelFont (12.0f));
        for (int s = 0; s < P::kSlots; ++s)
            g.drawText (juce::String (s + 1), juce::Rectangle<int> (matBox.getX() + 8, slotY (s), 18, 22), juce::Justification::centred);
        g.setFont (textFont (12.5f));
        g.drawText (T ("MOVIMIENTO escala los LFOs y los carriles. Las macros como fuente no se escalan."),
                    matBox.reduced (12, 6).withTrimmedTop (matBox.getHeight() - 26), juce::Justification::centredLeft);
    }
    void resized() override
    {
        lfoBox = { 0, 0, 236, getHeight() };
        matBox = { 248, 0, getWidth() - 248, getHeight() };
        for (int l = 0; l < P::kLfos; ++l)
        {
            const int y = rowY (l);
            shapes[l]->setBounds (lfoBox.getX() + 32, y, 120, 30);
            rates[l]->setBounds (lfoBox.getX() + 32, y + 36, 120, 30);
            scopes[l]->setBounds (lfoBox.getX() + 160, y + 6, 64, 54);
        }
        for (int s = 0; s < P::kSlots; ++s)
        {
            const int y = slotY (s);
            auto r = juce::Rectangle<int> (matBox.getX() + 30, y, matBox.getWidth() - 42, 24);
            srcs[s]->setBounds (r.removeFromLeft (108));
            r.removeFromLeft (6);
            dsts[s]->setBounds (r.removeFromLeft (136));
            r.removeFromLeft (8);
            amts[s]->setBounds (r);
        }
    }

private:
    int rowY (int l) const { return 34 + l * 82; }
    int slotY (int s) const { return 34 + s * 26; }
    Ogro2Processor& proc;
    juce::OwnedArray<Choice> shapes, rates, srcs, dsts;
    juce::OwnedArray<LfoScope> scopes;
    juce::OwnedArray<juce::Slider> amts;
    juce::OwnedArray<APVTS::SliderAttachment> amtAtt;
    juce::Rectangle<int> lfoBox, matBox;
};

// ---------------------------------------------------------------------------
// MOTION: 4 carriles de automatización dibujables
// ---------------------------------------------------------------------------
class MotionPage : public Page
{
public:
    explicit MotionPage (Ogro2Processor& p) : proc (p)
    {
        static const char* shapeNames[] = { "RAMPA", "SENO", "TRI", "AZAR", "PLANO" };
        for (int l = 0; l < kLanes; ++l)
        {
            addAndMakeVisible (dests.add (new Choice (p, P::lane (l, P::LaneDst), "", true)));
            addAndMakeVisible (bars.add (new Choice (p, P::lane (l, P::LaneBars), "")));
            auto* d = depths.add (new Knob (p, P::lane (l, P::LaneDepth), ""));
            d->setShowValue (false);
            d->slider.setTooltip (T ("Profundidad del carril"));
            addAndMakeVisible (d);
            addAndMakeVisible (canvases.add (new MotionCanvas (p, l)));
            for (int s = 0; s < 5; ++s)
            {
                auto* b = shapeButtons.add (new juce::TextButton (shapeNames[s]));
                styleKey (*b, col::keyCream);
                b->onClick = [this, l, s] { applyShape (l, s); };
                addAndMakeVisible (b);
            }
        }
    }
    juce::String title() const override { return "MOTION"; }
    void tick() override { for (auto* c : canvases) c->repaint(); }
    void paint (juce::Graphics& g) override
    {
        for (int l = 0; l < kLanes; ++l)
        {
            const auto r = laneRect (l);
            paint::violetCard (g, r.toFloat());
            g.setColour (col::violetText);
            drawSpaced (g, "MOTION " + juce::String (l + 1), { (float) r.getX() + 12.0f, (float) r.getY() + 6.0f, 120.0f, 16.0f },
                        labelFont (13.0f), 0.08f, juce::Justification::centredLeft);
        }
    }
    void resized() override
    {
        for (int l = 0; l < kLanes; ++l)
        {
            const auto r = laneRect (l);
            dests[l]->setBounds (r.getX() + 10, r.getY() + 26, 140, 26);
            bars[l]->setBounds (r.getX() + 156, r.getY() + 26, 84, 26);
            depths[l]->setBounds (r.getX() + 196, r.getY() + 0, 44, 26);
            for (int s = 0; s < 5; ++s) shapeButtons[l * 5 + s]->setBounds (r.getX() + 10 + s * 46, r.getY() + 58, 44, 24);
            canvases[l]->setBounds (r.getX() + 252, r.getY() + 8, r.getWidth() - 260, r.getHeight() - 16);
        }
    }

private:
    juce::Rectangle<int> laneRect (int l) const
    {
        const int h = (getHeight() - 3 * 8) / 4;
        return { 0, l * (h + 8), getWidth(), h };
    }
    void applyShape (int l, int s)
    {
        uint32_t seed = (uint32_t) juce::Random::getSystemRandom().nextInt();
        auto f = s == 0 ? std::function<float (float)> (pb::rampUp)
               : s == 1 ? std::function<float (float)> (pb::sine)
               : s == 2 ? std::function<float (float)> (pb::tri)
               : s == 3 ? pb::steps (seed, 16)
                        : std::function<float (float)> ([] (float) { return 0.5f; });
        pb::lane (proc.engine.bank, l, f);
        canvases[l]->repaint();
    }
    Ogro2Processor& proc;
    juce::OwnedArray<Choice> dests, bars;
    juce::OwnedArray<Knob> depths;
    juce::OwnedArray<MotionCanvas> canvases;
    juce::OwnedArray<juce::TextButton> shapeButtons;
};

// ---------------------------------------------------------------------------
// RING MOD
// ---------------------------------------------------------------------------
class RingPage : public Page
{
public:
    explicit RingPage (Ogro2Processor& p)
        : proc (p),
          on (p, P::RingOn, "RING ENCENDIDO", col::ledGreen),
          freq (p, P::RingFreq, "FRECUENCIA"),
          wave (p, P::RingWave, "FORMA"),
          mix (p, P::RingMix, "MEZCLA"),
          spread (p, P::RingSpread, T ("ESTÉREO"))
    {
        for (auto* c : std::initializer_list<juce::Component*> { &on, &freq, &wave, &mix, &spread }) addAndMakeVisible (c);
        freq.setTitleSize (14.0f);
        for (int t = 0; t < kTracks; ++t)
        {
            auto* r = routes.add (new Toggle (p, P::track (t, P::Ring), trackName (t), col::track (t)));
            addAndMakeVisible (r);
        }
    }
    juce::String title() const override { return "RING MOD"; }
    void tick() override { repaint (scopeRect); }
    void paint (juce::Graphics& g) override
    {
        paint::violetCard (g, ctlBox.toFloat(), T ("Portadora"));
        paint::violetCard (g, routeBox.toFloat(), T ("Qué pistas pasan por el ring"));
        paint::lcd (g, scopeRect.toFloat());
        const int w = (int) proc.value (P::RingWave);
        const float f = proc.value (P::RingFreq);
        const float cycles = juce::jlimit (1.0f, 24.0f, f / 60.0f);
        auto r = scopeRect.toFloat().reduced (8.0f);
        juce::Path pth;
        for (int i = 0; i <= 200; ++i)
        {
            const float x = (float) i / 200.0f, ph = std::fmod (x * cycles, 1.0f);
            const float v = w == 0 ? std::sin (juce::MathConstants<float>::twoPi * ph) : w == 1 ? (ph < 0.5f ? 4 * ph - 1 : 3 - 4 * ph) : (ph < 0.5f ? 1.0f : -1.0f);
            const float px = r.getX() + r.getWidth() * x, py = r.getCentreY() - v * (r.getHeight() * 0.42f);
            if (i == 0) pth.startNewSubPath (px, py); else pth.lineTo (px, py);
        }
        g.setColour (col::phosphor.withAlpha (0.25f));
        g.strokePath (pth, juce::PathStrokeType (4.0f));
        g.setColour (col::phosphor);
        g.strokePath (pth, juce::PathStrokeType (1.6f));
        g.setColour (col::violetValue);
        g.setFont (textFont (13.0f));
        g.drawFittedText (T ("El ring multiplica cada pista ruteada por la portadora: frecuencias bajas dan trémolo y movimiento, "
                             "altas dan campanas, metal y texturas inarmónicas. Ideal para percs y acordes minimal."),
                          routeBox.reduced (12).withTrimmedTop (150), juce::Justification::topLeft, 6);
    }
    void resized() override
    {
        ctlBox = { 0, 0, 300, getHeight() };
        routeBox = { 312, 0, getWidth() - 312, getHeight() };
        on.setBounds (12, 34, 180, 32);
        freq.setBounds (8, 74, 130, 150);
        wave.setBounds (150, 84, 138, 52);
        mix.setBounds (146, 146, 72, 96);
        spread.setBounds (220, 146, 72, 96);
        scopeRect = { 12, 250, 276, getHeight() - 262 };
        const int bw = (routeBox.getWidth() - 24 - 10) / 2;
        for (int t = 0; t < kTracks; ++t)
            routes[t]->setBounds (routeBox.getX() + 12 + (t % 2) * (bw + 10), 34 + (t / 2) * 28, bw, 26);
    }

private:
    Ogro2Processor& proc;
    Toggle on;
    Knob freq;
    Choice wave;
    Knob mix, spread;
    juce::OwnedArray<Toggle> routes;
    juce::Rectangle<int> ctlBox, routeBox, scopeRect;
};

// ---------------------------------------------------------------------------
// MASTER: filtro DJ, EQ y dinámica estilo EVO SOL, textura
// ---------------------------------------------------------------------------
class MasterPage : public Page
{
public:
    explicit MasterPage (Ogro2Processor& p)
        : proc (p),
          filter (p, P::MFilter, "FILTRO DJ", Theme::Violet, true), reso (p, P::MReso, "RESO"),
          lowcut (p, P::MLowCut, "LOW-CUT"), body (p, P::MBody, "CUERPO", Theme::Violet, true),
          pres (p, P::MPres, "PRESENCIA", Theme::Violet, true), air (p, P::MAir, "AIRE"),
          comp (p, P::MComp, T ("COMPRESIÓN")), color (p, P::MColor, "COLOR"),
          warmth (p, P::MWarmth, "CALOR"), parallel (p, P::MParallel, "MEZCLA"),
          texture (p, P::MTexture, "TEXTURA"),
          grComp ("COMP", true, 12.0f), grLim ("LIMIT", true, 12.0f)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &filter, &reso, &lowcut, &body, &pres, &air, &comp, &color, &warmth,
                                                                 &parallel, &texture, &grComp, &grLim })
            addAndMakeVisible (c);
        for (auto* k : { &comp, &color, &warmth, &parallel }) k->setRing (juce::Colour (0xfff2a466));
        texture.slider.setTooltip (T ("Polvo de vinilo y hiss de cinta"));
        filter.slider.setTooltip (T ("Izquierda: pasa-bajos · derecha: pasa-altos · centro: abierto"));
    }
    juce::String title() const override { return "MASTER"; }
    void tick() override
    {
        auto& m = proc.engine.meters;
        grComp.set (m.masterGr.load()); grLim.set (m.limGr.load());
    }
    void paint (juce::Graphics& g) override
    {
        paint::violetCard (g, fBox.toFloat(), "Filtro DJ");
        paint::violetCard (g, eqBox.toFloat(), T ("EQ estilo EVO SOL"));
        paint::violetCard (g, dynBox.toFloat(), T ("Dinámica · FET + óptico + saturación"));
        paint::violetCard (g, texBox.toFloat(), "Textura");
    }
    void resized() override
    {
        const int h2 = (getHeight() - 12) / 2;
        fBox = { 0, 0, 150, getHeight() };
        eqBox = { 162, 0, getWidth() - 162, h2 };
        dynBox = { 162, h2 + 12, getWidth() - 162 - 132, h2 };
        texBox = { getWidth() - 120, h2 + 12, 120, h2 };
        filter.setBounds (fBox.getX() + 10, 40, 130, 150);
        reso.setBounds (fBox.getX() + 30, 206, 90, 110);
        int x = eqBox.getX() + 8;
        const int kw = (eqBox.getWidth() - 16) / 4;
        for (auto* k : { &lowcut, &body, &pres, &air }) { k->setBounds (x, eqBox.getY() + 30, kw, h2 - 38); x += kw; }
        x = dynBox.getX() + 6;
        const int dw = (dynBox.getWidth() - 12 - 60) / 4;
        for (auto* k : { &comp, &color, &warmth, &parallel }) { k->setBounds (x, dynBox.getY() + 30, dw, h2 - 38); x += dw; }
        grComp.setBounds (dynBox.getRight() - 58, dynBox.getY() + 30, 24, h2 - 40);
        grLim.setBounds (dynBox.getRight() - 30, dynBox.getY() + 30, 24, h2 - 40);
        texture.setBounds (texBox.getX() + 10, texBox.getY() + 30, 100, h2 - 38);
    }

private:
    Ogro2Processor& proc;
    Knob filter, reso, lowcut, body, pres, air, comp, color, warmth, parallel, texture;
    Meter grComp, grLim;
    juce::Rectangle<int> fBox, eqBox, dynBox, texBox;
};
} // namespace

// ============================================================================
// Content
// ============================================================================
Content::Content (Ogro2Processor& p) : proc (p), ctx { p }, lane (ctx), keys (ctx)
{
    // ---- Cabecera ----
    styleKey (prevPreset, col::keyCream);
    styleKey (nextPreset, col::keyCream);
    styleKey (fileButton, col::keyCream);
    fileButton.setButtonText (T ("≡"));
    prevPreset.setTooltip ("Preset anterior");
    nextPreset.setTooltip ("Preset siguiente");
    fileButton.setTooltip (T ("Guardar o abrir presets propios"));
    auto step = [this] (int d) {
        const int n = (int) factoryPresets().size();
        proc.loadPreset ((proc.getPresetIndex() + d + n) % n);
        refreshPreset();
        selectTrack (proc.selectedTrack);
    };
    prevPreset.onClick = [step] { step (-1); };
    nextPreset.onClick = [step] { step (1); };
    fileButton.onClick = [this] { showFileMenu(); };
    presetDisplay.onClick = [this] { showPresetMenu(); };
    presetDisplay.setTooltip (T ("Presets por mood: cada uno trae kit, patrones A-H, modulaciones y carriles. Clic para elegir."));
    for (auto* c : std::initializer_list<juce::Component*> { &prevPreset, &nextPreset, &fileButton, &presetDisplay, &playOrb, &bpmLcd })
        addAndMakeVisible (c);

    for (int i = 0; i < kPatterns; ++i)
    {
        auto* b = patternButtons.add (new juce::TextButton (juce::String::charToString ((juce::juce_wchar) ('A' + i))));
        styleKey (*b, (i / 2) % 2 == 0 ? col::keyBlue : col::keyCream);
        b->setColour (juce::TextButton::buttonOnColourId, col::violet);
        b->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        b->onClick = [this, i] { proc.setValue (P::Pattern, (float) i); };
        b->setTooltip (T ("Patrón ") + b->getButtonText() + T (" (cambia al empezar el próximo compás)"));
        addAndMakeVisible (b);
    }
    playOrb.onClick = [this] { proc.engine.setInternalPlay (playOrb.getToggleState()); };
    playOrb.setTooltip (T ("Toca sin el transporte de Ableton (para probar). Con Ableton en play, OGRO sigue a Ableton."));
    rootChoice = std::make_unique<Choice> (p, P::Root, "TONALIDAD", false, Theme::Cream);
    addAndMakeVisible (*rootChoice);

    // ---- Pistas ----
    for (int t = 0; t < kTracks; ++t)
    {
        auto* r = rows.add (new TrackRow (p, t));
        r->onSelect = [this] (int tr) { selectTrack (tr); showPage (PageTrack); };
        addAndMakeVisible (r);
    }

    // ---- Ventana violeta ----
    pages.add (new TrackPage (p));
    pages.add (new FxPage (p));
    pages.add (new ModPage (p));
    pages.add (new MotionPage (p));
    pages.add (new RingPage (p));
    pages.add (new MasterPage (p));
    for (auto* pg : pages) addChildComponent (pg);
    backButton.setButtonText (T ("‹  PISTA"));
    styleKey (backButton, juce::Colours::white.withAlpha (0.9f));
    backButton.setTooltip (T ("Volver al sonido de la pista elegida"));
    backButton.onClick = [this] { showPage (PageTrack); };
    addChildComponent (backButton);

    // ---- Columna derecha ----
    static const char* macroCaps[] = { "filtros + saturación", "LFO + MOTION", "delay + reverb", "batería más sucia" };
    static const char* macroTips[] = {
        "CALOR: abre los filtros de bajo y acordes y suma saturación en el kick y el master",
        "MOVIMIENTO: cuánto actúan los LFOs y los carriles MOTION (al mínimo, quieto; al máximo, el doble)",
        "ESPACIO: más delay, más reverb y sala más grande",
        "CRUDEZA: más saturación y compresión en las percusiones",
    };
    static const juce::uint32 macroCols[] = { 0xffe5391c, 0xfff08a3c, 0xff3f7fc4, 0xffd9a21b };
    for (int m = 0; m < 4; ++m)
    {
        auto* k = macros.add (new MacroKnob (p, P::Macro1 + m, macroName (m), T (macroCaps[m]), juce::Colour (macroCols[m])));
        k->slider.setTooltip (T (macroTips[m]) + T (" · al medio no cambia nada · doble clic: al medio"));
        addAndMakeVisible (k);
    }
    static const char* panelNames[] = { "FX", "MOD", "MOTION", "RING", "MASTER" };
    static const char* panelTips[] = { "Delay, reverb, pump y drum bus", "LFOs y matriz de modulación",
                                       "Carriles de automatización dibujados", "Ring modulator",
                                       "Filtro DJ, EQ, compresión y textura del master" };
    for (int i = 0; i < 5; ++i)
    {
        auto* b = panelButtons.add (new juce::TextButton (panelNames[i]));
        styleKey (*b, col::keyCream);
        b->setColour (juce::TextButton::buttonOnColourId, col::ink);
        b->setTooltip (T (panelTips[i]));
        b->onClick = [this, i] { showPage (currentPage == i + 1 ? (int) PageTrack : i + 1); };
        addAndMakeVisible (b);
    }
    addAndMakeVisible (disc);
    ceiling = std::make_unique<Knob> (p, P::MCeiling, "TECHO", Theme::Cream);
    volume = std::make_unique<Knob> (p, P::MVol, "VOLUMEN", Theme::Cream);
    addAndMakeVisible (*ceiling);
    addAndMakeVisible (*volume);

    // ---- Groove ----
    swing = std::make_unique<Knob> (p, P::Swing, "SWING", Theme::Cream);
    addAndMakeVisible (*swing);
    styleKey (lenMinus, col::keyCream);
    styleKey (lenPlus, col::keyCream);
    lenMinus.onClick = [this] { auto& b = proc.engine.bank; b.setLength (ctx.pattern(), ctx.track(), b.getLength (ctx.pattern(), ctx.track()) - 1); };
    lenPlus.onClick = [this] { auto& b = proc.engine.bank; b.setLength (ctx.pattern(), ctx.track(), b.getLength (ctx.pattern(), ctx.track()) + 1); };
    lenMinus.setTooltip (T ("Largo de la pista: con largos distintos se arman polirritmos"));
    lenPlus.setTooltip (T ("Largo de la pista: con largos distintos se arman polirritmos"));
    inventButton.setButtonText ("INVENTAR");
    styleKey (inventButton, col::orange);
    inventButton.setTooltip (T ("Genera un patrón nuevo para esta pista, con sentido musical"));
    inventButton.onClick = [this] {
        inventPattern (proc.engine.bank, ctx.pattern(), ctx.track(), (uint32_t) juce::Random::getSystemRandom().nextInt());
    };
    clearButton.setButtonText ("LIMPIAR");
    copyButton.setButtonText ("COPIAR");
    pasteButton.setButtonText ("PEGAR");
    for (auto* b : { &clearButton, &copyButton, &pasteButton }) styleKey (*b, col::keyCream);
    clearButton.setTooltip (T ("Borra los pasos de esta pista en el patrón actual"));
    copyButton.setTooltip (T ("Copia el patrón completo (todas las pistas)"));
    pasteButton.setTooltip (T ("Pega el patrón copiado en el patrón actual"));
    clearButton.onClick = [this] { for (int s = 0; s < kSteps; ++s) proc.engine.bank.set (ctx.pattern(), ctx.track(), s, Step()); };
    copyButton.onClick = [this] { clipboardPattern = ctx.pattern(); };
    pasteButton.onClick = [this] { if (clipboardPattern >= 0 && clipboardPattern != ctx.pattern()) proc.engine.bank.copyPattern (clipboardPattern, ctx.pattern()); };
    for (auto* c : std::initializer_list<juce::Component*> { &lenMinus, &lenPlus, &inventButton, &clearButton, &copyButton, &pasteButton })
        addAndMakeVisible (c);

    // ---- Secuenciador ----
    addAndMakeVisible (lane);
    addAndMakeVisible (keys);
    static const char* modes[] = { "VELOCIDAD", "NOTA", "PROB.", "RATCHET", "LIGADO" };
    static const char* modeTips[] = { "Velocidad de cada paso", "Nota de cada paso en semitonos (bajo y acordes muestran la nota)",
                                      "Probabilidad de que suene cada paso", "Repeticiones dentro del paso (1 a 4)",
                                      "Ligado: el bajo hace glide hacia la nota sin re-disparar" };
    for (int i = 0; i < 5; ++i)
    {
        auto* b = modeButtons.add (new juce::TextButton (modes[i]));
        styleKey (*b, col::keyCream);
        b->getProperties().set ("flat", true);
        b->setColour (juce::TextButton::buttonOnColourId, col::ink);
        b->setTooltip (T (modeTips[i]));
        b->onClick = [this, i] {
            lane.setMode ((LaneMode) i);
            for (int k = 0; k < modeButtons.size(); ++k) modeButtons[k]->setToggleState (k == i, juce::dontSendNotification);
        };
        addAndMakeVisible (b);
    }
    modeButtons[0]->setToggleState (true, juce::dontSendNotification);

    refreshPreset();
    selectTrack (proc.selectedTrack);
    showPage (PageTrack);
    startTimerHz (30);
}

Content::~Content() { stopTimer(); }

void Content::refreshPreset()
{
    const auto& presets = factoryPresets();
    if (proc.userPresetName.isNotEmpty())
        presetDisplay.setText (proc.userPresetName, T ("preset propio"));
    else
    {
        const auto& pr = presets[(size_t) juce::jlimit (0, (int) presets.size() - 1, proc.getPresetIndex())];
        presetDisplay.setText (juce::String::fromUTF8 (pr.name.c_str()), juce::String::fromUTF8 (pr.mood.c_str()).toLowerCase());
    }
}

void Content::showPresetMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("PRESETS DE FÁBRICA");
    const auto& presets = factoryPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
        m.addItem (i + 1, juce::String::fromUTF8 (presets[(size_t) i].name.c_str()) + "  ·  " + juce::String::fromUTF8 (presets[(size_t) i].mood.c_str()),
                   true, proc.userPresetName.isEmpty() && i == proc.getPresetIndex());
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetDisplay), [this] (int r) {
        if (r <= 0) return;
        proc.loadPreset (r - 1);
        refreshPreset();
        selectTrack (proc.selectedTrack);
    });
}

void Content::showFileMenu()
{
    auto presetDir = [] {
        auto d = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("OGRO 2 Presets");
        d.createDirectory();
        return d;
    };
    juce::PopupMenu m;
    m.addItem (1, T ("Guardar preset propio..."));
    m.addItem (2, T ("Abrir preset propio..."));
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (fileButton), [this, presetDir] (int r) {
        if (r == 1)
        {
            chooser = std::make_unique<juce::FileChooser> ("Guardar preset de OGRO 2", presetDir().getChildFile ("Mi preset.ogro"), "*.ogro");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [this] (const juce::FileChooser& fc) {
                                      auto f = fc.getResult();
                                      if (f == juce::File()) return;
                                      if (! f.hasFileExtension ("ogro")) f = f.withFileExtension ("ogro");
                                      proc.saveUserPreset (f);
                                      refreshPreset();
                                  });
        }
        else if (r == 2)
        {
            chooser = std::make_unique<juce::FileChooser> ("Abrir preset de OGRO 2", presetDir(), "*.ogro");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc) {
                                      const auto f = fc.getResult();
                                      if (f.existsAsFile() && proc.loadUserPreset (f)) { refreshPreset(); selectTrack (proc.selectedTrack); }
                                  });
        }
    });
}

void Content::selectTrack (int t)
{
    proc.selectedTrack = t;
    for (int i = 0; i < rows.size(); ++i) rows[i]->setSelected (i == t);
    for (auto* pg : pages) pg->trackChanged (t);
    lane.repaint();
    keys.repaint();
    repaint (seqCard);
}

void Content::showPage (int i)
{
    currentPage = i;
    for (int k = 0; k < pages.size(); ++k) pages[k]->setVisible (k == i);
    for (int k = 0; k < panelButtons.size(); ++k) panelButtons[k]->setToggleState (k + 1 == i, juce::dontSendNotification);
    backButton.setVisible (i != PageTrack);
    repaint (window.expanded (8));
}

void Content::resized()
{
    chassis = { 12, 12, 1256, 796 };
    header = { 12, 12, 1256, 96 };
    tube = { 28, 108, 1224, 16 };
    trackCard = { 28, 138, 232, 398 };
    window = { 276, 138, 648, 398 };
    macroCard = { 940, 138, 312, 214 };
    masterCard = { 940, 400, 312, 136 };
    grooveCard = { 28, 552, 232, 240 };
    seqCard = { 276, 552, 976, 240 };

    // Cabecera
    prevPreset.setBounds (268, 35, 32, 50);
    presetDisplay.setBounds (304, 32, 232, 56);
    nextPreset.setBounds (540, 35, 32, 50);
    fileButton.setBounds (576, 35, 40, 50);
    for (int i = 0; i < kPatterns; ++i) patternButtons[i]->setBounds (640 + i * 33, 52, 31, 33);
    playOrb.setBounds (920, 28, 66, 66);
    bpmLcd.setBounds (994, 36, 124, 52);
    rootChoice->setBounds (1134, 30, 116, 58);

    // Pistas
    for (int t = 0; t < kTracks; ++t) rows[t]->setBounds (trackCard.getX() + 8, trackCard.getY() + 36 + t * 44, trackCard.getWidth() - 16, 42);

    // Ventana violeta
    auto inner = window.reduced (18, 14);
    for (auto* pg : pages) pg->setBounds (currentPage == PageTrack ? inner : inner);
    for (int k = 1; k < pages.size(); ++k) pages[k]->setBounds (inner.withTrimmedTop (42));
    pages[0]->setBounds (inner);
    backButton.setBounds (window.getRight() - 18 - 116, window.getY() + 14, 116, 32);

    // Columna derecha
    for (int m = 0; m < 4; ++m)
        macros[m]->setBounds (macroCard.getX() + 10 + (m % 2) * 148, macroCard.getY() + 40 + (m / 2) * 86, 146, 80);
    const int bw = macroCard.getWidth() / 5;
    for (int i = 0; i < 5; ++i) panelButtons[i]->setBounds (macroCard.getX() + i * bw, 360, bw - 4, 32);
    disc.setBounds (masterCard.getX() + 6, masterCard.getY() + 4, 128, 128);
    ceiling->setBounds (masterCard.getX() + 142, masterCard.getY() + 14, 80, 110);
    volume->setBounds (masterCard.getX() + 224, masterCard.getY() + 14, 80, 110);

    // Groove
    swing->setBounds (grooveCard.getX() + 10, grooveCard.getY() + 36, 84, 104);
    lenMinus.setBounds (grooveCard.getX() + 108, grooveCard.getY() + 92, 32, 34);
    lenPlus.setBounds (grooveCard.getX() + 188, grooveCard.getY() + 92, 32, 34);
    inventButton.setBounds (grooveCard.getX() + 12, grooveCard.getY() + 150, grooveCard.getWidth() - 24, 40);
    const int third = (grooveCard.getWidth() - 24 - 8) / 3;
    clearButton.setBounds (grooveCard.getX() + 12, grooveCard.getY() + 198, third, 32);
    copyButton.setBounds (grooveCard.getX() + 16 + third, grooveCard.getY() + 198, third, 32);
    pasteButton.setBounds (grooveCard.getX() + 20 + 2 * third, grooveCard.getY() + 198, third, 32);

    // Secuenciador
    int x = seqCard.getX() + 300;
    static const int modeW[] = { 104, 64, 70, 88, 80 };
    for (int i = 0; i < 5; ++i) { modeButtons[i]->setBounds (x, seqCard.getY() + 10, modeW[i], 30); x += modeW[i] + 4; }
    lane.setBounds (seqCard.getX() + 14, seqCard.getY() + 48, seqCard.getWidth() - 28, 84);
    keys.setBounds (seqCard.getX() + 14, seqCard.getY() + 138, seqCard.getWidth() - 28, 96);
}

void Content::paintLogo (juce::Graphics& g, juce::Rectangle<float> plate)
{
    // Placa azul (como el dragón cromado), destello rojo/amarillo detrás y letras de cromo
    g.setColour (juce::Colour (0xff141e46).withAlpha (0.35f));
    g.fillRoundedRectangle (plate.translated (0, 3.0f), 12.0f);
    juce::ColourGradient blue (juce::Colour (0xff1f3b70), 0, plate.getY(), juce::Colour (0xff6e9bc4), 0, plate.getBottom(), false);
    blue.addColour (0.55, juce::Colour (0xff3e679c));
    g.setGradientFill (blue);
    g.fillRoundedRectangle (plate, 12.0f);
    g.saveState();
    g.reduceClipRegion (plate.toNearestInt());
    const auto sc = juce::Point<float> (plate.getX() + 160.0f, plate.getCentreY() + 2.0f);
    g.setColour (col::red);
    g.fillPath (paint::star (sc, 30.0f, 4.0f));
    g.setColour (col::yellow);
    g.fillPath (paint::star (sc, 17.0f, 2.5f));
    g.setColour (col::red);
    g.fillPath (paint::star ({ plate.getX() + 24.0f, plate.getY() + 16.5f }, 8.5f, 1.5f));
    g.restoreState();

    auto chrome = [&g] (const juce::String& s, juce::Font f, float x, float baseline, float kern) {
        f.setExtraKerningFactor (kern);
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, s, x, baseline);
        juce::Path path;
        ga.createPath (path);
        const auto b = path.getBounds();
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillPath (path, juce::AffineTransform::translation (0, 2.0f));
        juce::ColourGradient cg (juce::Colours::white, 0, b.getY(), juce::Colours::white, 0, b.getBottom(), false);
        cg.addColour (0.34, juce::Colour (0xffeef0f6));
        cg.addColour (0.52, juce::Colour (0xff9a9dab));
        cg.addColour (0.57, juce::Colour (0xff3a3c48));
        cg.addColour (0.80, juce::Colour (0xffc6c9d6));
        g.setGradientFill (cg);
        g.fillPath (path);
        return b;
    };
    chrome ("OGRO", titleFont (35.0f), plate.getX() + 12.0f, plate.getY() + 44.0f, 0.02f);
    chrome ("2", titleFont (21.0f), plate.getX() + 180.0f, plate.getY() + 27.0f, 0.0f);
    g.setColour (col::dim);
    drawSpaced (g, T ("EVO THE SUN  ·  GROOVEBOX"), { plate.getX() + 2.0f, plate.getBottom() + 4.0f, 240.0f, 14.0f }, labelFont (11.0f), 0.2f,
                juce::Justification::centredLeft);
}

void Content::paint (juce::Graphics& g)
{
    g.fillAll (col::black);
    // Aparato de plástico crema con grilla fina
    auto ch = chassis.toFloat();
    g.setColour (col::chassis);
    g.fillRoundedRectangle (ch, 22.0f);
    g.saveState();
    g.reduceClipRegion (chassis);
    g.setColour (col::grid);
    for (float x = ch.getX(); x < ch.getRight(); x += 12.0f) g.fillRect (x, ch.getY(), 1.0f, ch.getHeight());
    for (float y = ch.getY(); y < ch.getBottom(); y += 12.0f) g.fillRect (ch.getX(), y, ch.getWidth(), 1.0f);
    g.restoreState();
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.drawLine (ch.getX() + 22.0f, ch.getY() + 1.5f, ch.getRight() - 22.0f, ch.getY() + 1.5f, 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.12f));
    g.drawLine (ch.getX() + 22.0f, ch.getBottom() - 1.5f, ch.getRight() - 22.0f, ch.getBottom() - 1.5f, 3.0f);

    paintLogo (g, { 38.0f, 24.0f, 204.0f, 58.0f });
    paint::copperTube (g, tube.toFloat());

    // Rótulos de la cabecera
    g.setColour (col::label);
    drawSpaced (g, T ("PATRÓN"), { 640.0f, 22.0f, 120.0f, 14.0f }, labelFont (12.0f), 0.08f, juce::Justification::centredLeft);
    const int playingPattern = proc.engine.meters.pattern.load();
    const bool isPlaying = proc.engine.meters.playing.load();
    for (int i = 0; i < kPatterns; ++i)
        paint::led (g, { 640.0f + (float) i * 33.0f + 9.5f, 40.0f, 12.0f, 7.0f }, col::ledGreen, isPlaying && i == playingPattern);

    // Tarjetas
    for (auto r : { trackCard, macroCard, masterCard, grooveCard, seqCard }) paint::card (g, r.toFloat());
    paint::violetWindow (g, window.toFloat());
    g.setColour (col::ink);
    drawSpaced (g, "PISTAS", { (float) trackCard.getX() + 14.0f, (float) trackCard.getY() + 10.0f, 200.0f, 18.0f }, titleFont (12.0f), 0.16f,
                juce::Justification::centredLeft);
    drawSpaced (g, "MACROS", { (float) macroCard.getX() + 14.0f, (float) macroCard.getY() + 10.0f, 200.0f, 18.0f }, titleFont (12.0f), 0.16f,
                juce::Justification::centredLeft);
    g.setColour (col::caption);
    g.setFont (textFont (12.0f));
    g.drawText (T ("al medio = sin cambio"), juce::Rectangle<int> (macroCard.getRight() - 160, macroCard.getY() + 10, 146, 18),
                juce::Justification::centredRight);

    // Título de la página en la ventana violeta (paneles)
    if (currentPage != PageTrack && currentPage < pages.size())
    {
        g.setColour (col::violetText);
        drawSpaced (g, pages[currentPage]->title().toUpperCase(), { (float) window.getX() + 18.0f, (float) window.getY() + 14.0f, 400.0f, 32.0f },
                    titleFont (19.0f), 0.1f, juce::Justification::centredLeft);
    }

    // Groove
    const int t = proc.selectedTrack.load();
    g.setColour (col::violetRing);
    g.fillRoundedRectangle ((float) grooveCard.getX() + 14.0f, (float) grooveCard.getY() + 13.0f, 5.0f, 14.0f, 2.5f);
    g.setColour (col::ink);
    drawSpaced (g, "GROOVE", { (float) grooveCard.getX() + 26.0f, (float) grooveCard.getY() + 10.0f, 200.0f, 20.0f }, titleFont (12.0f), 0.16f,
                juce::Justification::centredLeft);
    g.setColour (col::label);
    drawSpaced (g, "LARGO", { (float) grooveCard.getX() + 108.0f, (float) grooveCard.getY() + 62.0f, 110.0f, 20.0f }, labelFont (12.0f), 0.08f,
                juce::Justification::centredLeft);
    auto lenR = juce::Rectangle<float> ((float) grooveCard.getX() + 144.0f, (float) grooveCard.getY() + 92.0f, 40.0f, 32.0f);
    paint::lcd (g, lenR, 6.0f);
    paint::glowText (g, juce::String (proc.engine.bank.getLength (ctx.pattern(), t)), lenR, lcdFont (28.0f), juce::Justification::centred);

    // Secuenciador
    g.setColour (col::track (t));
    g.fillRoundedRectangle ((float) seqCard.getX() + 14.0f, (float) seqCard.getY() + 18.0f, 5.0f, 14.0f, 2.5f);
    g.setColour (col::ink);
    drawSpaced (g, T ("SECUENCIA · ") + trackName (t), { (float) seqCard.getX() + 26.0f, (float) seqCard.getY() + 14.0f, 270.0f, 22.0f },
                titleFont (12.0f), 0.14f, juce::Justification::centredLeft);
}

void Content::timerCallback()
{
    auto& m = proc.engine.meters;
    for (int t = 0; t < kTracks; ++t)
    {
        const int c = m.triggers[t].load();
        if (c != lastTrigger[t]) { lastTrigger[t] = c; flash[t] = 1.0f; }
        else flash[t] *= 0.8f;
        rows[t]->flash (flash[t]);
    }
    const int sel = (int) proc.value (P::Pattern);
    const int playingPattern = m.pattern.load();
    ++blink;
    for (int i = 0; i < kPatterns; ++i)
    {
        const bool queued = m.playing.load() && i == sel && sel != playingPattern;
        patternButtons[i]->setToggleState (i == sel && (! queued || (blink / 6) % 2 == 0), juce::dontSendNotification);
    }
    const bool hostPlaying = m.playing.load() && ! proc.engine.isInternalPlaying();
    bpmLcd.setText (juce::String (m.bpm.load(), 1), hostPlaying ? "BPM · ABLETON" : (proc.engine.isInternalPlaying() ? "BPM · INTERNO" : "BPM"));
    if (playOrb.getToggleState() != proc.engine.isInternalPlaying())
        playOrb.setToggleState (proc.engine.isInternalPlaying(), juce::dontSendNotification);
    keys.repaint();
    const auto rev = proc.engine.bank.revision.load();
    if (rev != lastRevision) { lastRevision = rev; lane.repaint(); repaint (grooveCard); }
    if (auto* pg = pages[currentPage]) pg->tick();
    disc.set (juce::jmax (m.outL.load(), m.outR.load()), m.limGr.load());
    repaint (juce::Rectangle<int> (636, 36, 270, 14));
    refreshPreset();
}

// ============================================================================
// Editor
// ============================================================================
Ogro2Editor::Ogro2Editor (Ogro2Processor& p) : AudioProcessorEditor (p), content (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    setResizable (true, true);
    setResizeLimits (Content::kW * 2 / 3, Content::kH * 2 / 3, Content::kW * 3 / 2, Content::kH * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) Content::kW / (double) Content::kH);
    setSize (Content::kW, Content::kH);
}

Ogro2Editor::~Ogro2Editor() { setLookAndFeel (nullptr); }

void Ogro2Editor::paint (juce::Graphics& g) { g.fillAll (ogro2::ui::col::black); }

void Ogro2Editor::resized()
{
    content.setBounds (0, 0, Content::kW, Content::kH);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) Content::kW));
}
