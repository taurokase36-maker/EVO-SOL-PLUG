#include "PluginEditor.h"

using namespace ogro;
using namespace ogro::ui;

namespace
{
const char* trackDescription (int t)
{
    static const char* d[] = {
        "Seno con envolvente de pitch, click y drive analógico",
        "Ráfagas de ruido filtrado con room corto",
        "Rim / snare: dos modos resonantes, click y ruido",
        "6 osciladores cuadrados inarmónicos + ruido. Corta al hat abierto",
        "Hat abierto: lo corta el hat cerrado, como en las máquinas clásicas",
        "FM de 2 operadores con pitch: congas, toms, bleeps y campanas",
        "Sierra / cuadrada / sub con filtro escalera de 4 polos, acento y glide",
        "Polifónico: 2 sierras por nota, filtro con envolvente y chorus estéreo",
    };
    return d[juce::jlimit (0, 7, t)];
}

juce::String upperName (int t, int p) { return juce::String::fromUTF8 (soundParamInfo (t, p).name); }

// ---------------------------------------------------------------------------
// SONIDO: las 8 perillas de la pista elegida + mezcla
// ---------------------------------------------------------------------------
class SoundPage : public Page
{
public:
    explicit SoundPage (OgroProcessor& p)
        : proc (p),
          level (p, P::track (0, P::Level), "NIVEL", col::cream),
          pan (p, P::track (0, P::Pan), "PANEO", col::cream, true),
          dly (p, P::track (0, P::Delay), "DELAY", col::cream),
          rev (p, P::track (0, P::Reverb), "REVERB", col::cream),
          pump (p, P::track (0, P::Pump), "PUMP", col::cream),
          ring (p, P::track (0, P::Ring), "RING MOD", col::led),
          mute (p, P::track (0, P::Mute), "MUTE", col::orange),
          solo (p, P::track (0, P::Solo), "SOLO", col::yellow)
    {
        for (int i = 0; i < kSoundParams; ++i)
            addAndMakeVisible (sound.add (new Knob (p, P::sound (0, i), upperName (0, i), col::track (0))));
        for (auto* c : std::initializer_list<juce::Component*> { &level, &pan, &dly, &rev, &pump, &ring, &mute, &solo, &audition })
            addAndMakeVisible (c);
        audition.setButtonText (T ("PROBAR  ▶"));
        audition.setColour (juce::TextButton::buttonColourId, col::orange);
        audition.setColour (juce::TextButton::textColourOffId, col::ink);
        audition.onClick = [this] { proc.engine.audition (track); };
        audition.setTooltip (T ("Dispara el sonido para escucharlo (también funciona con el transporte parado)"));
        pump.slider.setTooltip (T ("Cuánto se agacha esta pista con cada kick (sidechain)"));
        ring.button.setTooltip (T ("Manda esta pista por el ring mod (pestaña RING MOD)"));
        trackChanged (0);
    }

    void trackChanged (int t) override
    {
        track = t;
        for (int i = 0; i < kSoundParams; ++i)
        {
            sound[i]->bind (P::sound (t, i), upperName (t, i));
            sound[i]->setCap (col::track (t), false);
        }
        level.bind (P::track (t, P::Level), "NIVEL");
        pan.bind (P::track (t, P::Pan), "PANEO");
        dly.bind (P::track (t, P::Delay), "DELAY");
        rev.bind (P::track (t, P::Reverb), "REVERB");
        pump.bind (P::track (t, P::Pump), "PUMP");
        ring.bind (P::track (t, P::Ring));
        mute.bind (P::track (t, P::Mute));
        solo.bind (P::track (t, P::Solo));
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        box (g, soundBox, T ("SONIDO · ") + trackName (track), col::track (track));
        box (g, mixBox, T ("MEZCLA Y ENVÍOS"), col::cream);
        g.setColour (col::dim);
        g.setFont (font (11.5f));
        g.drawText (T (trackDescription (track)), soundBox.getX() + 18, 26, soundBox.getWidth() - 220, 14, juce::Justification::centredLeft);
        g.setFont (font (11.0f));
        g.drawFittedText (T ("PUMP agacha la pista con cada kick (sidechain fantasma, aunque el kick esté muteado).\n"
                             "DELAY y REVERB son envíos a los efectos de la pestaña FX."),
                          mixBox.withTrimmedLeft (640).withTrimmedTop (100).withTrimmedRight (16).withHeight (44), juce::Justification::topLeft, 3);
    }
    void resized() override
    {
        soundBox = { 0, 0, getWidth(), 200 };
        mixBox = { 0, 210, getWidth(), getHeight() - 210 };
        const int w = (getWidth() - 32) / kSoundParams;
        for (int i = 0; i < kSoundParams; ++i) sound[i]->setBounds (16 + i * w, 44, w, 150);
        int x = 16;
        for (auto* k : { &level, &pan, &dly, &rev, &pump }) { k->setBounds (x, mixBox.getY() + 32, 108, 124); x += 112; }
        ring.setBounds (640, mixBox.getY() + 40, 130, 32);
        mute.setBounds (780, mixBox.getY() + 40, 100, 32);
        solo.setBounds (890, mixBox.getY() + 40, 100, 32);
        audition.setBounds (getWidth() - 176, 4, 160, 26);
    }

private:
    OgroProcessor& proc;
    int track = 0;
    juce::OwnedArray<Knob> sound;
    Knob level, pan, dly, rev, pump;
    Toggle ring, mute, solo;
    juce::TextButton audition;
    juce::Rectangle<int> soundBox, mixBox;
};

// ---------------------------------------------------------------------------
// MODULACIÓN: 4 LFOs + matriz de 12 conexiones
// ---------------------------------------------------------------------------
class ModPage : public Page
{
public:
    explicit ModPage (OgroProcessor& p) : proc (p)
    {
        for (int l = 0; l < P::kLfos; ++l)
        {
            addAndMakeVisible (shapes.add (new Choice (p, P::lfo (l, P::LfoShape), "FORMA")));
            addAndMakeVisible (rates.add (new Choice (p, P::lfo (l, P::LfoRate), "VELOCIDAD (SYNC)")));
            addAndMakeVisible (scopes.add (new LfoScope (p, l)));
        }
        for (int s = 0; s < P::kSlots; ++s)
        {
            addAndMakeVisible (srcs.add (new Choice (p, P::mod (s, P::ModSrc), "")));
            addAndMakeVisible (dsts.add (new Choice (p, P::mod (s, P::ModDst), "", true)));
            auto* amt = amts.add (new juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox));
            amt->getProperties().set ("bipolar", true);
            amt->setColour (juce::Slider::trackColourId, col::orange);
            amt->setDoubleClickReturnValue (true, 0.0);
            addAndMakeVisible (amt);
            amtAtt.add (new APVTS::SliderAttachment (p.state, juce::String (paramDefs()[(size_t) P::mod (s, P::ModAmt)].id), *amt));
            amt->setTooltip (T ("Cantidad (doble clic = 0)"));
        }
    }
    void tick() override { for (auto* s : scopes) s->repaint(); }

    void paint (juce::Graphics& g) override
    {
        box (g, lfoBox, "LFOs  (sincronizados al tempo)");
        box (g, matBox, T ("MATRIZ DE MODULACIÓN  ·  fuente → destino → cantidad"));
        g.setFont (font (16.0f, true));
        for (int l = 0; l < P::kLfos; ++l)
        {
            g.setColour (col::orange);
            g.drawText (juce::String (l + 1), lfoBox.getX() + 10, lfoBox.getY() + 46 + l * 82, 20, 24, juce::Justification::centred);
        }
        g.setFont (font (11.0f, true));
        for (int s = 0; s < P::kSlots; ++s)
        {
            const auto r = slotRect (s);
            g.setColour (col::bodyLight);
            g.fillRoundedRectangle (r.toFloat(), 4.0f);
            g.setColour (col::dim);
            g.drawText (juce::String (s + 1), r.withWidth (22), juce::Justification::centred);
        }
        g.setColour (col::dim);
        g.setFont (font (10.5f));
        g.drawText (T ("La macro MOVIMIENTO escala todos los LFOs y carriles. Las macros como fuente no se escalan."),
                    matBox.withTrimmedTop (matBox.getHeight() - 22).withTrimmedLeft (16), juce::Justification::centredLeft);
    }
    void resized() override
    {
        lfoBox = { 0, 0, 370, getHeight() };
        matBox = { 380, 0, getWidth() - 380, getHeight() };
        for (int l = 0; l < P::kLfos; ++l)
        {
            const int y = 32 + l * 82;
            shapes[l]->setBounds (36, y, 116, 40);
            rates[l]->setBounds (158, y, 116, 40);
            scopes[l]->setBounds (282, y + 6, 76, 40);
        }
        for (int s = 0; s < P::kSlots; ++s)
        {
            auto r = slotRect (s).reduced (4, 6);
            r.removeFromLeft (20);
            srcs[s]->setBounds (r.removeFromLeft (88));
            r.removeFromLeft (4);
            dsts[s]->setBounds (r.removeFromLeft (112));
            r.removeFromLeft (4);
            amts[s]->setBounds (r);
        }
    }

private:
    juce::Rectangle<int> slotRect (int s) const
    {
        const int colW = (matBox.getWidth() - 30) / 2;
        const int c = s / 6, row = s % 6;
        return { matBox.getX() + 10 + c * (colW + 10), matBox.getY() + 32 + row * 50, colW, 44 };
    }
    OgroProcessor& proc;
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
    explicit MotionPage (OgroProcessor& p) : proc (p)
    {
        for (int l = 0; l < kLanes; ++l)
        {
            addAndMakeVisible (dests.add (new Choice (p, P::lane (l, P::LaneDst), "DESTINO", true)));
            addAndMakeVisible (bars.add (new Choice (p, P::lane (l, P::LaneBars), "LARGO")));
            addAndMakeVisible (depths.add (new Knob (p, P::lane (l, P::LaneDepth), "PROF.", col::orange)));
            addAndMakeVisible (canvases.add (new MotionCanvas (p, l)));
            static const char* shapeNames[] = { "RAMPA", "SENO", "TRI", "AZAR", "PLANO" };
            for (int s = 0; s < 5; ++s)
            {
                auto* b = shapeButtons.add (new juce::TextButton (shapeNames[s]));
                b->onClick = [this, l, s] { applyShape (l, s); };
                addAndMakeVisible (b);
            }
        }
    }
    void tick() override { for (auto* c : canvases) c->repaint(); }

    void paint (juce::Graphics& g) override
    {
        for (int l = 0; l < kLanes; ++l)
        {
            const auto r = laneRect (l);
            g.setColour (col::panel);
            g.fillRoundedRectangle (r.toFloat(), 5.0f);
            g.setColour (col::line);
            g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 5.0f, 1.0f);
            g.setColour (col::orange);
            g.setFont (font (12.0f, true));
            g.drawText ("MOTION " + juce::String (l + 1), r.getX() + 10, r.getY() + 4, 90, 16, juce::Justification::centredLeft);
        }
    }
    void resized() override
    {
        for (int l = 0; l < kLanes; ++l)
        {
            const auto r = laneRect (l);
            dests[l]->setBounds (r.getX() + 10, r.getY() + 20, 150, 38);
            bars[l]->setBounds (r.getX() + 166, r.getY() + 20, 100, 38);
            depths[l]->setBounds (r.getX() + 272, r.getY() + 2, 58, 84);
            for (int s = 0; s < 5; ++s) shapeButtons[l * 5 + s]->setBounds (r.getX() + 10 + s * 52, r.getY() + 62, 50, 22);
            canvases[l]->setBounds (r.getX() + 340, r.getY() + 6, r.getWidth() - 348, r.getHeight() - 12);
        }
    }

private:
    juce::Rectangle<int> laneRect (int l) const
    {
        const int h = (getHeight() - 3 * 6) / 4;
        return { 0, l * (h + 6), getWidth(), h };
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
    OgroProcessor& proc;
    juce::OwnedArray<Choice> dests, bars;
    juce::OwnedArray<Knob> depths;
    juce::OwnedArray<MotionCanvas> canvases;
    juce::OwnedArray<juce::TextButton> shapeButtons;
};

// ---------------------------------------------------------------------------
// RING MOD (sección propia)
// ---------------------------------------------------------------------------
class RingPage : public Page
{
public:
    explicit RingPage (OgroProcessor& p)
        : proc (p),
          on (p, P::RingOn, "RING MOD ON", col::led),
          freq (p, P::RingFreq, "FRECUENCIA", col::red),
          wave (p, P::RingWave, "FORMA"),
          mix (p, P::RingMix, "MEZCLA", col::orange),
          spread (p, P::RingSpread, T ("ESTÉREO"), col::orange)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &on, &freq, &wave, &mix, &spread }) addAndMakeVisible (c);
        for (int t = 0; t < kTracks; ++t)
            addAndMakeVisible (routes.add (new Toggle (p, P::track (t, P::Ring), trackName (t), col::track (t))));
    }
    void tick() override { repaint (scopeRect); }
    void paint (juce::Graphics& g) override
    {
        box (g, ctlBox, "RING MODULATOR", col::red);
        box (g, routeBox, T ("RUTEO · qué pistas pasan por el ring"), col::red);
        // Portadora dibujada
        auto r = scopeRect.toFloat();
        g.setColour (col::lcd);
        g.fillRoundedRectangle (r, 4.0f);
        const int w = (int) proc.value (P::RingWave);
        const float f = proc.value (P::RingFreq);
        const float cycles = juce::jlimit (1.0f, 24.0f, f / 60.0f);
        juce::Path pth;
        for (int i = 0; i <= 200; ++i)
        {
            const float x = (float) i / 200.0f, ph = std::fmod (x * cycles, 1.0f);
            const float v = w == 0 ? std::sin (juce::MathConstants<float>::twoPi * ph) : w == 1 ? (ph < 0.5f ? 4 * ph - 1 : 3 - 4 * ph) : (ph < 0.5f ? 1.0f : -1.0f);
            const float px = r.getX() + 6 + (r.getWidth() - 12) * x, py = r.getCentreY() - v * (r.getHeight() * 0.38f);
            if (i == 0) pth.startNewSubPath (px, py); else pth.lineTo (px, py);
        }
        g.setColour (col::lcdText);
        g.strokePath (pth, juce::PathStrokeType (1.6f));
        g.setColour (col::dim);
        g.setFont (font (11.0f));
        g.drawFittedText (T ("El ring mod multiplica cada pista ruteada por una onda portadora: "
                             "frecuencias bajas dan trémolo y movimiento, frecuencias altas dan campanas, metal y texturas "
                             "inarmónicas. Ideal para percs y acordes minimal. Podés modular la frecuencia desde MOD o MOTION."),
                          routeBox.reduced (16).withTrimmedTop (150), juce::Justification::topLeft, 5);
    }
    void resized() override
    {
        ctlBox = { 0, 0, 470, getHeight() };
        routeBox = { 480, 0, getWidth() - 480, getHeight() };
        on.setBounds (16, 32, 160, 30);
        freq.setBounds (16, 70, 200, 210);
        wave.setBounds (240, 78, 200, 40);
        mix.setBounds (236, 140, 100, 130);
        spread.setBounds (340, 140, 100, 130);
        scopeRect = { 16, 290, 438, getHeight() - 304 };
        const int bw = (routeBox.getWidth() - 32 - 30) / 4;
        for (int t = 0; t < kTracks; ++t)
            routes[t]->setBounds (routeBox.getX() + 16 + (t % 4) * (bw + 10), 40 + (t / 4) * 44, bw, 34);
    }

private:
    OgroProcessor& proc;
    Toggle on;
    Knob freq;
    Choice wave;
    Knob mix, spread;
    juce::OwnedArray<Toggle> routes;
    juce::Rectangle<int> ctlBox, routeBox, scopeRect;
};

// ---------------------------------------------------------------------------
// FX: delay, reverb, pump, drum bus
// ---------------------------------------------------------------------------
class FxPage : public Page
{
public:
    explicit FxPage (OgroProcessor& p)
        : proc (p),
          dlyDiv (p, P::DlyDiv, "TIEMPO"),
          dlyFb (p, P::DlyFb, "FEEDBACK", col::orange), dlyTone (p, P::DlyTone, "TONO", col::orange), dlyRet (p, P::DlyRet, "RETORNO", col::orange),
          revSize (p, P::RevSize, T ("TAMAÑO"), col::keyCream), revDecay (p, P::RevDecay, "DECAY", col::keyCream), revRet (p, P::RevRet, "RETORNO", col::keyCream),
          pumpRel (p, P::PumpRel, "TIEMPO", col::yellow),
          busComp (p, P::BusComp, T ("COMPRESIÓN"), col::red), busCrudo (p, P::BusCrudo, "CRUDO", col::red),
          gr (T ("GR"), true, 18.0f)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &dlyDiv, &dlyFb, &dlyTone, &dlyRet, &revSize, &revDecay, &revRet, &pumpRel, &busComp, &busCrudo, &gr })
            addAndMakeVisible (c);
    }
    void tick() override { gr.set (proc.engine.meters.drumGr.load()); }
    void paint (juce::Graphics& g) override
    {
        box (g, b1, "DELAY PING-PONG (sync)");
        box (g, b2, "REVERB", col::keyCream);
        box (g, b3, "PUMP", col::yellow);
        box (g, b4, T ("DRUM BUS · compresión fuerte + saturación cruda"), col::red);
        g.setColour (col::dim);
        g.setFont (font (11.0f));
        g.drawFittedText (T ("Con cada kick, las pistas con PUMP se agachan y vuelven en este tiempo. "
                             "Es el bombeo clásico del house (sidechain)."),
                          b3.reduced (14).withTrimmedTop (24).withTrimmedLeft (120), juce::Justification::topLeft, 5);
        g.drawFittedText (T ("Kick, clap, rim, hats y perc pasan por un compresor rápido y una saturación de cinta/válvula."),
                          b4.reduced (14).withTrimmedTop (142), juce::Justification::topLeft, 2);
    }
    void resized() override
    {
        const int w = (getWidth() - 10) / 2, h = (getHeight() - 10) / 2;
        b1 = { 0, 0, w, h }; b2 = { w + 10, 0, w, h }; b3 = { 0, h + 10, w, h }; b4 = { w + 10, h + 10, w, h };
        dlyDiv.setBounds (b1.getX() + 14, b1.getY() + 40, 110, 40);
        int x = b1.getX() + 134;
        for (auto* k : { &dlyFb, &dlyTone, &dlyRet }) { k->setBounds (x, b1.getY() + 28, 112, 140); x += 116; }
        x = b2.getX() + 20;
        for (auto* k : { &revSize, &revDecay, &revRet }) { k->setBounds (x, b2.getY() + 28, 130, 140); x += 150; }
        pumpRel.setBounds (b3.getX() + 14, b3.getY() + 28, 100, 140);
        busComp.setBounds (b4.getX() + 14, b4.getY() + 26, 120, 118);
        busCrudo.setBounds (b4.getX() + 140, b4.getY() + 26, 120, 118);
        gr.setBounds (b4.getRight() - 46, b4.getY() + 28, 30, b4.getHeight() - 40);
    }

private:
    OgroProcessor& proc;
    Choice dlyDiv;
    Knob dlyFb, dlyTone, dlyRet, revSize, revDecay, revRet, pumpRel, busComp, busCrudo;
    Meter gr;
    juce::Rectangle<int> b1, b2, b3, b4;
};

// ---------------------------------------------------------------------------
// MASTER: filtro DJ, EQ y compresión estilo EVO SOL, salida
// ---------------------------------------------------------------------------
class MasterPage : public Page
{
public:
    explicit MasterPage (OgroProcessor& p)
        : proc (p),
          filter (p, P::MFilter, "FILTRO DJ", col::red, true), reso (p, P::MReso, "RESONANCIA", col::red),
          lowcut (p, P::MLowCut, "LOW-CUT", col::orange), body (p, P::MBody, "CUERPO", col::orange, true),
          pres (p, P::MPres, "PRESENCIA", col::orange, true), air (p, P::MAir, "AIRE", col::orange),
          comp (p, P::MComp, T ("COMPRESIÓN"), col::yellow), color (p, P::MColor, "COLOR", col::yellow),
          warmth (p, P::MWarmth, "CALOR", col::yellow), parallel (p, P::MParallel, "MEZCLA", col::yellow),
          texture (p, P::MTexture, "TEXTURA", col::keyCream), ceiling (p, P::MCeiling, "TECHO", col::keyCream),
          vol (p, P::MVol, "VOLUMEN", col::keyCream, true),
          grComp ("COMP", true, 12.0f), grLim ("LIMIT", true, 12.0f), mL ("L", false, 48.0f), mR ("R", false, 48.0f)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &filter, &reso, &lowcut, &body, &pres, &air, &comp, &color, &warmth,
                                                                 &parallel, &texture, &ceiling, &vol, &grComp, &grLim, &mL, &mR })
            addAndMakeVisible (c);
        texture.slider.setTooltip (T ("Polvo de vinilo y hiss de cinta"));
        filter.slider.setTooltip (T ("Izquierda: pasa-bajos · derecha: pasa-altos · centro: abierto"));
    }
    void tick() override
    {
        auto& m = proc.engine.meters;
        grComp.set (m.masterGr.load()); grLim.set (m.limGr.load()); mL.set (m.outL.load()); mR.set (m.outR.load());
    }
    void paint (juce::Graphics& g) override
    {
        box (g, fBox, "FILTRO DJ", col::red);
        box (g, eqBox, T ("EQ  (estilo EVO SOL)"));
        box (g, dynBox, T ("DINÁMICA  ·  FET + óptico + saturación"), col::yellow);
        box (g, outBox, "SALIDA", col::keyCream);
    }
    void resized() override
    {
        fBox = { 0, 0, 230, getHeight() };
        eqBox = { 240, 0, 470, (getHeight() - 10) / 2 };
        dynBox = { 240, eqBox.getBottom() + 10, 470, (getHeight() - 10) / 2 };
        outBox = { 720, 0, getWidth() - 720, getHeight() };
        filter.setBounds (15, 34, 200, 200);
        reso.setBounds (55, 240, 120, 124);
        int x = eqBox.getX() + 10;
        for (auto* k : { &lowcut, &body, &pres, &air }) { k->setBounds (x, eqBox.getY() + 28, 112, 150); x += 114; }
        x = dynBox.getX() + 10;
        for (auto* k : { &comp, &color, &warmth, &parallel }) { k->setBounds (x, dynBox.getY() + 28, 112, 150); x += 114; }
        texture.setBounds (outBox.getX() + 12, outBox.getY() + 30, 96, 110);
        ceiling.setBounds (outBox.getX() + 12, outBox.getY() + 144, 96, 110);
        vol.setBounds (outBox.getX() + 112, outBox.getY() + 30, 110, 130);
        const int mx = outBox.getX() + 120;
        grComp.setBounds (mx, outBox.getY() + 170, 34, outBox.getHeight() - 182);
        grLim.setBounds (mx + 38, outBox.getY() + 170, 34, outBox.getHeight() - 182);
        mL.setBounds (mx + 90, outBox.getY() + 170, 34, outBox.getHeight() - 182);
        mR.setBounds (mx + 128, outBox.getY() + 170, 34, outBox.getHeight() - 182);
    }

private:
    OgroProcessor& proc;
    Knob filter, reso, lowcut, body, pres, air, comp, color, warmth, parallel, texture, ceiling, vol;
    Meter grComp, grLim, mL, mR;
    juce::Rectangle<int> fBox, eqBox, dynBox, outBox;
};
} // namespace

// ============================================================================
// Content
// ============================================================================
Content::Content (OgroProcessor& p) : proc (p), ctx { p }, lane (ctx), keys (ctx)
{
    // Cabecera
    const auto& presets = factoryPresets();
    for (int i = 0; i < (int) presets.size(); ++i) presetBox.addItem (juce::String::fromUTF8 (presets[(size_t) i].name.c_str()), i + 1);
    presetBox.setColour (juce::ComboBox::backgroundColourId, col::lcd);
    presetBox.onChange = [this] {
        const int idx = presetBox.getSelectedId() - 1;
        if (idx >= 0 && (idx != proc.getPresetIndex() || proc.userPresetName.isNotEmpty())) { proc.loadPreset (idx); selectTrack (proc.selectedTrack); }
    };
    presetBox.setTooltip (T ("Presets por mood: cada uno trae kit, patrones A-H, modulaciones y carriles"));
    auto step = [this] (int d) {
        const int n = (int) factoryPresets().size();
        proc.loadPreset ((proc.getPresetIndex() + d + n) % n);
        refreshPresetBox();
        selectTrack (proc.selectedTrack);
    };
    prevPreset.onClick = [step] { step (-1); };
    nextPreset.onClick = [step] { step (1); };
    saveButton.setButtonText ("GUARDAR");
    loadButton.setButtonText ("ABRIR");
    saveButton.setTooltip (T ("Guarda todo (sonidos, patrones y modulaciones) como preset propio"));
    loadButton.setTooltip (T ("Abre un preset propio guardado"));
    auto presetDir = [] {
        auto d = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("OGRO Presets");
        d.createDirectory();
        return d;
    };
    saveButton.onClick = [this, presetDir] {
        chooser = std::make_unique<juce::FileChooser> ("Guardar preset de OGRO", presetDir().getChildFile ("Mi preset.ogro"), "*.ogro");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc) {
                                  auto f = fc.getResult();
                                  if (f == juce::File()) return;
                                  if (! f.hasFileExtension ("ogro")) f = f.withFileExtension ("ogro");
                                  proc.saveUserPreset (f);
                                  refreshPresetBox();
                              });
    };
    loadButton.onClick = [this, presetDir] {
        chooser = std::make_unique<juce::FileChooser> ("Abrir preset de OGRO", presetDir(), "*.ogro");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc) {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile() && proc.loadUserPreset (f)) { refreshPresetBox(); selectTrack (proc.selectedTrack); }
                              });
    };
    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &prevPreset, &nextPreset, &saveButton, &loadButton, &lcd, &playButton })
        addAndMakeVisible (c);

    for (int i = 0; i < kPatterns; ++i)
    {
        auto* b = patternButtons.add (new juce::TextButton (juce::String::charToString ((juce::juce_wchar) ('A' + i))));
        b->setColour (juce::TextButton::buttonColourId, col::keyCream);
        b->setColour (juce::TextButton::textColourOffId, col::ink);
        b->setColour (juce::TextButton::buttonOnColourId, col::red);
        b->setColour (juce::TextButton::textColourOnId, col::cream);
        b->onClick = [this, i] { proc.setValue (P::Pattern, (float) i); };
        b->setTooltip (T ("Patrón ") + b->getButtonText() + T (" (cambia al empezar el próximo compás)"));
        addAndMakeVisible (b);
    }
    playButton.setButtonText (T ("▶ PLAY"));
    playButton.setClickingTogglesState (true);
    playButton.setColour (juce::TextButton::buttonColourId, col::keyCream);
    playButton.setColour (juce::TextButton::textColourOffId, col::ink);
    playButton.setColour (juce::TextButton::buttonOnColourId, col::red);
    playButton.onClick = [this] { proc.engine.setInternalPlay (playButton.getToggleState()); };
    playButton.setTooltip (T ("Reproduce sin el transporte de Ableton (para probar). Con Ableton en play, OGRO sigue a Ableton."));
    swing = std::make_unique<Knob> (p, P::Swing, "SWING", col::ink);
    swing->setDark (true);
    addAndMakeVisible (*swing);
    rootChoice = std::make_unique<Choice> (p, P::Root, "TONALIDAD");
    addAndMakeVisible (*rootChoice);

    // Pistas
    for (int t = 0; t < kTracks; ++t)
    {
        auto* r = rows.add (new TrackRow (p, t));
        r->onSelect = [this] (int tr) { selectTrack (tr); };
        addAndMakeVisible (r);
    }
    for (int m = 0; m < 4; ++m)
    {
        auto* k = macros.add (new Knob (p, P::Macro1 + m, macroName (m), m == 0 ? col::red : m == 1 ? col::orange : m == 2 ? col::keyCream : col::yellow));
        k->setTitleSize (8.5f);
        addAndMakeVisible (k);
    }
    macros[0]->slider.setTooltip (T ("CALOR: abre filtros, suma saturación y cuerpo"));
    macros[1]->slider.setTooltip (T ("MOVIMIENTO: cuánto actúan los LFOs y los carriles dibujados (0 = quieto, máx = el doble)"));
    macros[2]->slider.setTooltip (T ("ESPACIO: más delay, reverb y tamaño de sala"));
    macros[3]->slider.setTooltip (T ("CRUDEZA: más saturación y compresión en las percusiones"));
    addAndMakeVisible (outL);
    addAndMakeVisible (outR);

    // Páginas
    static const char* tabNames[] = { "SONIDO", "MOD", "MOTION", "RING MOD", "FX", "MASTER" };
    for (int i = 0; i < 6; ++i)
    {
        auto* b = tabs.add (new juce::TextButton (juce::String::fromUTF8 (tabNames[i])));
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { showPage (i); };
        addAndMakeVisible (b);
    }
    pages.add (new SoundPage (p));
    pages.add (new ModPage (p));
    pages.add (new MotionPage (p));
    pages.add (new RingPage (p));
    pages.add (new FxPage (p));
    pages.add (new MasterPage (p));
    for (auto* pg : pages) addChildComponent (pg);

    // Secuenciador
    addAndMakeVisible (lane);
    addAndMakeVisible (keys);
    static const char* modes[] = { "VELOCIDAD", "NOTA", "PROBAB.", "RATCHET", "LIGADO" };
    for (int i = 0; i < 5; ++i)
    {
        auto* b = modeButtons.add (new juce::TextButton (modes[i]));
        b->setColour (juce::TextButton::buttonOnColourId, col::orange);
        b->onClick = [this, i] {
            lane.setMode ((LaneMode) i);
            for (int k = 0; k < modeButtons.size(); ++k) modeButtons[k]->setToggleState (k == i, juce::dontSendNotification);
        };
        addAndMakeVisible (b);
    }
    modeButtons[0]->setToggleState (true, juce::dontSendNotification);
    modeButtons[1]->setTooltip (T ("Nota de cada paso en semitonos (bajo y acordes muestran el nombre de la nota)"));
    modeButtons[4]->setTooltip (T ("Ligado: el bajo hace glide hacia la nota sin re-disparar"));
    inventButton.setButtonText ("INVENTAR");
    inventButton.setColour (juce::TextButton::buttonColourId, col::orange);
    inventButton.setColour (juce::TextButton::textColourOffId, col::ink);
    inventButton.setTooltip (T ("Genera un patrón nuevo para esta pista, con sentido musical"));
    clearButton.setButtonText ("LIMPIAR");
    copyButton.setButtonText ("COPIAR");
    pasteButton.setButtonText ("PEGAR");
    copyButton.setTooltip (T ("Copia el patrón completo (todas las pistas)"));
    pasteButton.setTooltip (T ("Pega el patrón copiado en el patrón actual"));
    inventButton.onClick = [this] {
        inventPattern (proc.engine.bank, ctx.pattern(), ctx.track(), (uint32_t) juce::Random::getSystemRandom().nextInt());
    };
    clearButton.onClick = [this] {
        for (int s = 0; s < kSteps; ++s) proc.engine.bank.set (ctx.pattern(), ctx.track(), s, Step());
    };
    copyButton.onClick = [this] { clipboardPattern = ctx.pattern(); };
    pasteButton.onClick = [this] { if (clipboardPattern >= 0 && clipboardPattern != ctx.pattern()) proc.engine.bank.copyPattern (clipboardPattern, ctx.pattern()); };
    lenMinus.onClick = [this] { auto& b = proc.engine.bank; b.setLength (ctx.pattern(), ctx.track(), b.getLength (ctx.pattern(), ctx.track()) - 1); };
    lenPlus.onClick = [this] { auto& b = proc.engine.bank; b.setLength (ctx.pattern(), ctx.track(), b.getLength (ctx.pattern(), ctx.track()) + 1); };
    lenMinus.setTooltip (T ("Largo de la pista: con largos distintos se arman polirritmos"));
    for (auto* c : std::initializer_list<juce::Component*> { &inventButton, &clearButton, &copyButton, &pasteButton, &lenMinus, &lenPlus })
        addAndMakeVisible (c);

    refreshPresetBox();
    selectTrack (proc.selectedTrack);
    showPage (0);
    startTimerHz (30);
}

Content::~Content() { stopTimer(); }

void Content::refreshPresetBox()
{
    if (proc.userPresetName.isNotEmpty())
        presetBox.setText (proc.userPresetName, juce::dontSendNotification);
    else
        presetBox.setSelectedId (proc.getPresetIndex() + 1, juce::dontSendNotification);
    repaint (headerArea);
}

void Content::selectTrack (int t)
{
    proc.selectedTrack = t;
    for (int i = 0; i < rows.size(); ++i) rows[i]->setSelected (i == t);
    for (auto* pg : pages) pg->trackChanged (t);
    lane.repaint();
    keys.repaint();
    repaint (seqTools);
}

void Content::showPage (int i)
{
    currentPage = i;
    for (int k = 0; k < pages.size(); ++k) pages[k]->setVisible (k == i);
    for (int k = 0; k < tabs.size(); ++k) tabs[k]->setToggleState (k == i, juce::dontSendNotification);
}

void Content::resized()
{
    headerArea = { 0, 0, kW, 92 };
    trackArea = { 12, 108, 222, 320 };
    macroArea = { 12, 446, 222, 108 - 44 };
    pageArea = { 246, 102, kW - 258, 412 };
    seqArea = { 0, 522, kW, kH - 522 };
    seqTools = { 12, 530, 222, kH - 542 };

    // Cabecera
    presetBox.setBounds (276, 30, 236, 30);
    prevPreset.setBounds (516, 30, 30, 30);
    nextPreset.setBounds (548, 30, 30, 30);
    saveButton.setBounds (586, 30, 78, 30);
    loadButton.setBounds (668, 30, 62, 30);
    for (int i = 0; i < kPatterns; ++i) patternButtons[i]->setBounds (752 + i * 30, 36, 27, 30);
    playButton.setBounds (1002, 30, 72, 30);
    lcd.setBounds (1080, 22, 92, 44);
    swing->setBounds (1180, 8, 84, 80);
    rootChoice->setBounds (12, 0, 0, 0);
    rootChoice->setVisible (false);

    // Pistas
    for (int t = 0; t < kTracks; ++t) rows[t]->setBounds (trackArea.getX(), trackArea.getY() + t * 40, trackArea.getWidth(), 38);
    // Macros
    for (int m = 0; m < 4; ++m) macros[m]->setBounds (12 + m * 56, 438, 56, 78);
    outL.setBounds (0, 0, 0, 0);
    outR.setBounds (0, 0, 0, 0);

    // Páginas
    const int tw = 110;
    for (int i = 0; i < tabs.size(); ++i) tabs[i]->setBounds (pageArea.getX() + i * (tw + 6), pageArea.getY(), tw, 28);
    rootChoice->setBounds (pageArea.getRight() - 120, pageArea.getY() - 2, 120, 32);
    rootChoice->setVisible (true);
    for (auto* pg : pages) pg->setBounds (pageArea.getX(), pageArea.getY() + 36, pageArea.getWidth(), pageArea.getHeight() - 36);

    // Secuenciador
    lane.setBounds (246, 534, kW - 258, 104);
    keys.setBounds (246, 644, kW - 258, 166);
    int y = 562;
    for (int i = 0; i < 5; ++i)
    {
        modeButtons[i]->setBounds (seqTools.getX() + (i % 2) * 112, y + (i / 2) * 30, 108, 26);
    }
    lenMinus.setBounds (seqTools.getX() + 112, 658, 30, 26);
    lenPlus.setBounds (seqTools.getX() + 190, 658, 30, 26);
    inventButton.setBounds (seqTools.getX(), 698, 220, 32);
    clearButton.setBounds (seqTools.getX(), 740, 72, 28);
    copyButton.setBounds (seqTools.getX() + 74, 740, 72, 28);
    pasteButton.setBounds (seqTools.getX() + 148, 740, 72, 28);
}

void Content::paintLogo (juce::Graphics& g, juce::Rectangle<float> r)
{
    // Wordmark: letras pesadas con franjas horizontales, estilo hardware de los 80
    juce::Font f (juce::FontOptions (54.0f, juce::Font::bold));
    f.setHorizontalScale (1.12f);
    f.setExtraKerningFactor (0.06f);
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, "OGRO", r.getX(), r.getY() + 50.0f);
    juce::Path path;
    ga.createPath (path);
    g.setColour (col::ink);
    g.fillPath (path);
    // Franjas
    g.saveState();
    g.reduceClipRegion (path);
    for (int i = 0; i < 3; ++i)
    {
        g.setColour (i == 0 ? col::red : i == 1 ? col::orange : col::yellow);
        g.fillRect (r.getX(), r.getY() + 34.0f + (float) i * 6.0f, r.getWidth(), 3.0f);
    }
    g.restoreState();
    g.setColour (col::ink.withAlpha (0.85f));
    g.setFont (font (10.0f, true));
    g.drawText (T ("GROOVEBOX ANALÓGICO  ·  EVO THE SUN"), r.getX() + 2.0f, r.getY() + 58.0f, 240.0f, 14.0f, juce::Justification::left);
}

void Content::paint (juce::Graphics& g)
{
    // Cuerpo
    g.fillAll (col::body);
    juce::ColourGradient bodyGrad (col::bodyLight, 0, 92, col::body.darker (0.3f), 0, (float) kH, false);
    g.setGradientFill (bodyGrad);
    g.fillRect (0, 92, kW, kH - 92);

    // Placa crema
    auto plate = headerArea.toFloat();
    juce::ColourGradient pg (col::plate.brighter (0.06f), 0, 0, col::plateShade, 0, plate.getBottom(), false);
    g.setGradientFill (pg);
    g.fillRect (plate);
    g.setColour (col::ink.withAlpha (0.35f));
    g.fillRect (plate.withTop (plate.getBottom() - 3.0f));
    for (auto pt : { juce::Point<float> (8, 8), juce::Point<float> ((float) kW - 8, 8), juce::Point<float> (8, 84), juce::Point<float> ((float) kW - 8, 84) })
    {
        g.setColour (col::plateShade.darker (0.3f));
        g.fillEllipse (pt.x - 4, pt.y - 4, 8, 8);
        g.setColour (col::ink.withAlpha (0.5f));
        g.drawLine (pt.x - 2.5f, pt.y, pt.x + 2.5f, pt.y, 1.0f);
    }
    paintLogo (g, { 22, 6, 240, 80 });

    g.setColour (col::ink);
    g.setFont (font (10.0f, true));
    g.drawText ("PRESET  /  MOOD", 276, 12, 200, 14, juce::Justification::centredLeft);
    g.drawText (T ("PATRÓN"), 752, 12, 120, 14, juce::Justification::centredLeft);
    g.drawText ("TRANSPORTE", 1002, 8, 120, 14, juce::Justification::centredLeft);
    // Descripción del mood
    const auto& presets = factoryPresets();
    juce::String mood = proc.userPresetName.isNotEmpty() ? T ("Preset propio")
                                                          : juce::String::fromUTF8 (presets[(size_t) juce::jlimit (0, (int) presets.size() - 1, proc.getPresetIndex())].mood.c_str());
    g.setColour (col::ink.withAlpha (0.7f));
    g.setFont (font (10.5f));
    g.drawText (mood, 276, 64, 460, 14, juce::Justification::centredLeft);
    // LEDs de patrón (el que suena)
    const int playingPattern = proc.engine.meters.pattern.load();
    for (int i = 0; i < kPatterns; ++i)
    {
        const bool lit = proc.engine.meters.playing.load() && i == playingPattern;
        auto led = juce::Rectangle<float> (752.0f + (float) i * 30.0f + 10.0f, 26.0f, 7.0f, 7.0f);
        g.setColour (lit ? col::led : col::ledOff.brighter (0.2f));
        g.fillEllipse (led);
    }

    // Secciones del cuerpo
    g.setColour (col::cream);
    g.setFont (font (11.0f, true));
    g.drawText ("PISTAS", trackArea.getX(), 95, 100, 12, juce::Justification::centredLeft);
    g.setColour (col::line);
    g.drawLine (12.0f, 432.0f, 234.0f, 432.0f, 1.0f);

    // Banda del secuenciador
    auto band = seqArea.toFloat().reduced (6.0f, 0.0f);
    g.setColour (col::panel);
    g.fillRoundedRectangle (band.withTrimmedBottom (4.0f), 6.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (band.withTrimmedBottom (4.0f).reduced (0.5f), 6.0f, 1.0f);
    const int t = proc.selectedTrack.load();
    g.setColour (col::track (t));
    g.fillRect (seqTools.getX(), seqTools.getY() + 2, 4, 20);
    g.setColour (col::cream);
    g.setFont (font (13.0f, true));
    g.drawText (T ("SECUENCIADOR · ") + trackName (t), seqTools.getX() + 10, seqTools.getY(), 220, 24, juce::Justification::centredLeft);
    g.setColour (col::dim);
    g.setFont (font (10.0f, true));
    g.drawText ("LARGO", seqTools.getX(), 658, 100, 26, juce::Justification::centredLeft);
    g.setColour (col::cream);
    g.setFont (font (14.0f, true));
    g.drawText (juce::String (proc.engine.bank.getLength (ctx.pattern(), t)), seqTools.getX() + 142, 658, 48, 26, juce::Justification::centred);
    g.setColour (col::dim);
    g.setFont (font (9.5f));
    g.drawFittedText (T ("Clic: paso on/off · clic derecho: acento\nArrastrá sobre teclas o barras para pintar"),
                      seqTools.getX(), 776, 222, 26, juce::Justification::topLeft, 2);
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
    lcd.setText (juce::String (m.bpm.load(), 1), hostPlaying ? "HOST" : (proc.engine.isInternalPlaying() ? "INTERNO" : "BPM"));
    if (playButton.getToggleState() != proc.engine.isInternalPlaying())
        playButton.setToggleState (proc.engine.isInternalPlaying(), juce::dontSendNotification);
    keys.repaint();
    const auto rev = proc.engine.bank.revision.load();
    if (rev != lastRevision) { lastRevision = rev; lane.repaint(); repaint (seqTools); }
    if (auto* pg = pages[currentPage]) pg->tick();
    repaint (headerArea.withTrimmedLeft (740).withWidth (250));
    if (proc.userPresetName.isEmpty() && presetBox.getSelectedId() != proc.getPresetIndex() + 1) refreshPresetBox();
}

// ============================================================================
// Editor
// ============================================================================
OgroEditor::OgroEditor (OgroProcessor& p) : AudioProcessorEditor (p), content (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);
    setResizable (true, true);
    setResizeLimits (Content::kW * 2 / 3, Content::kH * 2 / 3, Content::kW * 3 / 2, Content::kH * 3 / 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) Content::kW / (double) Content::kH);
    setSize (Content::kW, Content::kH);
}

OgroEditor::~OgroEditor() { setLookAndFeel (nullptr); }

void OgroEditor::paint (juce::Graphics& g) { g.fillAll (ogro::ui::col::body); }

void OgroEditor::resized()
{
    content.setBounds (0, 0, Content::kW, Content::kH);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) Content::kW));
}
