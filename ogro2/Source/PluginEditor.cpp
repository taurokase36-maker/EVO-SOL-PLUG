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
        "Filtro escalera de 4 polos · 5 formas, sub, acento y slide 303, legato",
        "Polifónico: 2 sierras por nota, filtro con envolvente y chorus · en tono",
        "Pluck, lead, campana u órgano · poly o mono · síntesis limpia",
        "Samples de Orbhitz (HIT, PAD, SFX, CHD) · editables y en tono",
    };
    return d[juce::jlimit (0, kTracks - 1, t)];
}

juce::String upperName (int t, int p) { return juce::String::fromUTF8 (soundParamInfo (t, p).name); }
juce::String trackLabel (int t) { return t == Melody ? T ("MELODÍA") : juce::String (trackName (t)); }

// Rótulo de tarjeta violeta en coordenadas locales
void cardTitle (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& s)
{
    g.setColour (col::violetText);
    drawSpaced (g, upper (s), r.toFloat().withHeight (18.0f).translated (12.0f, 8.0f), labelFont (13.0f), 0.08f,
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
          ring (p, P::track (0, P::Ring), "RING"),
          envA (p, P::track (Bass, P::EnvA), "ATAQUE"),
          envS (p, P::track (Bass, P::EnvS), "SUSTAIN"),
          envR (p, P::track (Bass, P::EnvR), "RELEASE"),
          chordType (p, P::sound (Chord, 0), "TIPO"),
          melModel (p, P::sound (Melody, 0), "MODELO"),
          bassOsc (p, P::BassOsc, "FORMA"),
          bassSub (p, P::BassSub, "SUB"),
          bassAccent (p, P::BassAccent, "ACENTO"),
          bassLegato (p, P::BassLegato, "LEGATO", col::ledGreen),
          chordMode (p, P::ChordMode, "MODO"),
          chordInv (p, P::ChordInv, T ("INVERSIÓN")),
          chordSpread (p, P::ChordSpread, "APERTURA"),
          chordStrum (p, P::ChordStrum, "STRUM"),
          chordLead (p, P::ChordLead, "VOICE LEAD.", col::ledGreen),
          melVoices (p, P::MelVoices, "VOCES"),
          melOctave (p, P::MelOctave, "OCTAVA"),
          texSample (p, 0, "SAMPLE", false),
          texCat (p, P::TexCat, T ("CATEGORÍA")),
          texRand (p, P::TexRandStep, "POR PASO", col::ledGreen),
          texFollow (p, P::TexFollow, "EN TONO", col::ledGreen),
          texRev (p, P::TexReverse, "REVERSA", col::orange),
          chordSource (p, P::ChordSource, "FUENTE"),
          chordSample (p, 1, "SAMPLE CHD", true),
          chordRand (p, P::ChordRandStep, "POR PASO", col::ledGreen),
          chordLevel (p, P::ChordSampleLevel, "NIVEL SAMPLE"),
          stripType (p, P::track (0, P::StripType), "TIPO DE DRIVE"),
          stripCompType (p, P::track (0, P::StripCompType), "COMP")
    {
        // Tira analógica (pestaña TIRA)
        static const std::pair<int, const char*> stripKnobs[] = { { P::StripDrive, "DRIVE" }, { P::StripBody, "CUERPO" }, { P::StripBodyHz, "FREC." },
                                                                  { P::StripNoise, "RUIDO" }, { P::StripTrans, "TRANSIENTE" }, { P::StripBright, "BRILLO" },
                                                                  { P::StripComp, "COMP" } };
        for (int k = 0; k < 7; ++k)
        {
            auto* kn = strip.add (new Knob (p, P::track (0, stripKnobs[k].first), T (stripKnobs[k].second), Theme::Violet, stripKnobs[k].first == P::StripTrans));
            kn->setRing (juce::Colour (0xfff2a466));
            kn->isLocked = [this, k] { return proc.isLocked (track, kSoundParams + k); };
            kn->onLockToggle = [this, k] { proc.toggleLock (track, kSoundParams + k); };
            addChildComponent (kn);
        }
        addChildComponent (stripType);
        addChildComponent (stripCompType);
        for (auto* b : { &tabSound, &tabStrip })
        {
            styleKey (*b, col::keyCream);
            b->setColour (juce::TextButton::buttonOnColourId, col::violet);
            b->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
            addAndMakeVisible (b);
        }
        tabSound.setButtonText ("SONIDO");
        tabStrip.setButtonText ("TIRA");
        tabSound.setTooltip (T ("Las perillas de sonido de la pista"));
        tabStrip.setTooltip (T ("Tira analógica de la pista: drive (cinta, válvula, transistor, diodo), cuerpo, ruido, transiente, brillo y compresor"));
        tabSound.onClick = [this] { tab = 0; trackChanged (track); };
        tabStrip.onClick = [this] { tab = 1; trackChanged (track); };
        diceButton.setButtonText ("VARIAR");
        styleKey (diceButton, col::keyBlue);
        diceButton.setTooltip (T ("Dado de sonido: mueve al azar las perillas de esta pista que no tienen candado (clic en el candadito de cada perilla). "
                                  "Clic derecho: más fuerte. Se deshace con Cmd+Z de Ableton."));
        diceButton.onClick = [this] {
            const bool strong = juce::ModifierKeys::currentModifiers.isPopupMenu() || juce::ModifierKeys::currentModifiers.isShiftDown();
            proc.rollSoundDice (track, strong ? 0.6f : 0.25f);
        };
        diceButton.setTriggeredOnMouseDown (false);
        addAndMakeVisible (diceButton);
        for (int i = 0; i < kSoundParams; ++i)
        {
            auto* kn = sound.add (new Knob (p, P::sound (0, i), upperName (0, i)));
            kn->isLocked = [this, i] { return proc.isLocked (track, i); };
            kn->onLockToggle = [this, i] { proc.toggleLock (track, i); };
            addAndMakeVisible (kn);
        }
        for (auto* c : std::initializer_list<juce::Component*> { &level, &pan, &dly, &rev, &pump, &ring, &audition })
            addAndMakeVisible (c);
        for (auto* c : std::initializer_list<juce::Component*> { &envA, &envS, &envR, &chordType, &melModel, &bassOsc, &bassSub, &bassAccent,
                                                                 &bassLegato, &chordMode, &chordInv, &chordSpread, &chordStrum, &chordLead,
                                                                 &melVoices, &melOctave, &texSample, &texDice, &texCat, &texRand, &texFollow,
                                                                 &texRev, &texFilter, &chordSource, &chordSample, &chordDice, &chordRand, &chordLevel,
                                                                 &tabVoicing, &tabSample })
            addChildComponent (c);
        for (auto* b : { &texDice, &chordDice }) { b->setButtonText ("DADO"); styleKey (*b, col::orange); }
        texDice.setTooltip (T ("Un sample al azar de la categoría elegida"));
        chordDice.setTooltip (T ("Un acorde de Orbhitz (CHD) al azar"));
        texDice.onClick = [this] { proc.rollSampleDice (0); texSample.refresh(); };
        chordDice.onClick = [this] { proc.rollSampleDice (1); chordSample.refresh(); };
        texRand.button.setTooltip (T ("Cada golpe elige un sample distinto de la categoría (sale de la semilla: con AZAR FIJO se repite igual)"));
        texFollow.button.setTooltip (T ("El sample se transpone a la tonalidad y la NOTA de cada paso mueve la altura (los samples sin altura clara no se transponen)"));
        chordRand.button.setTooltip (T ("Cada acorde elige un sample CHD; en modo diatónico prefiere menores o mayores según el grado"));
        chordSource.box.setTooltip (T ("Síntesis: las sierras de siempre. Sample: un acorde de Orbhitz llevado a la raíz de cada paso. Capas: los dos juntos."));
        texFilter.setClickingTogglesState (false);
        styleKey (texFilter, col::keyCream);
        texFilter.setTooltip (T ("Tipo de filtro: LADDER (4 polos, con carácter) o SVF (12 dB, limpio)"));
        texFilter.onClick = [this] { proc.setValue (P::TexFilter, proc.value (P::TexFilter) < 0.5f ? 1.0f : 0.0f); };
        for (auto* b : { &tabVoicing, &tabSample })
        {
            styleKey (*b, col::keyCream);
            b->setColour (juce::TextButton::buttonOnColourId, col::violet);
            b->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        }
        tabVoicing.setButtonText ("VOICING");
        tabSample.setButtonText ("SAMPLE");
        tabVoicing.onClick = [this] { chordTab = 0; trackChanged (track); };
        tabSample.onClick = [this] { chordTab = 1; trackChanged (track); };
        level.setTravel (420);
        for (auto* k : { &dly, &rev, &pump }) k->setRing (juce::Colour (0xfff2a466));
        ring.slider.setTooltip (T ("Cuánto pasa esta pista por el ring mod (la portadora se ajusta en el panel FX)"));
        envS.slider.setTooltip (T ("Con SUSTAIN en 0 la nota cae sola (como en la 1.0). Con sustain, dura lo que el paso o la nota MIDI y después suelta en RELEASE."));
        audition.setButtonText ("PROBAR");
        styleKey (audition, col::orange);
        audition.onClick = [this] { proc.engine.audition (track); };
        audition.setTooltip (T ("Dispara el sonido para escucharlo (también con el transporte parado)"));
        pump.slider.setTooltip (T ("Cuánto se agacha esta pista con cada kick (sidechain fantasma, aunque el kick esté muteado)"));
        bassOsc.box.setTooltip (T ("Clásica: la perilla ONDA va de sierra a cuadrada y sub (como la 1.0). Las otras formas usan ONDA como timbre: ancho del pulso, desafinación, brillo o índice de FM."));
        bassAccent.box.setTooltip (T ("303: los acentos suben volumen y filtro, cierran el filtro más rápido y se acumulan si van seguidos; el slide tarda siempre lo mismo"));
        bassLegato.button.setTooltip (T ("Si una nota empieza mientras la anterior sigue (gate largo o nota MIDI apretada), se liga sin volver a disparar"));
        chordMode.box.setTooltip (T ("Diatónico: el acorde se arma con las notas de la escala (siempre en tono). Paralelo: el mismo acorde se transpone entero (chord memory, como la 1.0)."));
        chordLead.button.setTooltip (T ("Elige la inversión que menos se mueve respecto del acorde anterior"));
        melVoices.box.setTooltip (T ("Poly: hasta 4 notas a la vez. Mono: una sola. Mono legato: las notas que se pisan se ligan con GLIDE."));
        for (auto* c : { &chordType, &melModel }) c->box.getProperties().set ("maxFont", 20.0);
        trackChanged (0);
    }
    juce::String title() const override { return trackLabel (track); }
    void setTab (int t) override { tab = juce::jlimit (0, 1, t); trackChanged (track); }
    void tick() override
    {
        for (auto* k : sound) k->repaint (k->lockArea().toNearestInt());
        if (track == Texture || track == Chord)
        {
            texSample.refresh();
            chordSample.refresh();
            texFilter.setButtonText (proc.value (P::TexFilter) < 0.5f ? "LADDER" : "SVF");
            const auto st = proc.sampleStatus (track == Texture ? 0 : 1);
            if (st != status) { status = st; repaint (0, 0, getWidth(), 34); }
        }
    }

    void trackChanged (int t) override
    {
        track = t;
        for (int i = 0; i < kSoundParams; ++i) sound[i]->bind (P::sound (t, i), upperName (t, i));
        static const std::pair<int, const char*> stripFields[] = { { P::StripDrive, "DRIVE" }, { P::StripBody, "CUERPO" }, { P::StripBodyHz, "FREC." },
                                                                   { P::StripNoise, "RUIDO" }, { P::StripTrans, "TRANSIENTE" }, { P::StripBright, "BRILLO" },
                                                                   { P::StripComp, "COMP" } };
        for (int k = 0; k < 7; ++k) strip[k]->bind (P::track (t, stripFields[k].first), T (stripFields[k].second));
        stripType.bind (P::track (t, P::StripType));
        stripCompType.bind (P::track (t, P::StripCompType));
        tabSound.setToggleState (tab == 0, juce::dontSendNotification);
        tabStrip.setToggleState (tab == 1, juce::dontSendNotification);
        level.bind (P::track (t, P::Level), "NIVEL");
        pan.bind (P::track (t, P::Pan), "PANEO");
        dly.bind (P::track (t, P::Delay), "DELAY");
        rev.bind (P::track (t, P::Reverb), "REVERB");
        pump.bind (P::track (t, P::Pump), "PUMP");
        ring.bind (P::track (t, P::Ring), "RING");
        hasEnv = t >= Bass;
        hasAttack = hasEnv && t != Chord;
        hasExtras = t >= Bass || tab == 1;
        envA.setVisible (hasAttack);
        envS.setVisible (hasEnv);
        envR.setVisible (hasEnv);
        if (hasEnv)
        {
            if (hasAttack) envA.bind (P::track (t, P::EnvA), "ATAQUE");
            envS.bind (P::track (t, P::EnvS), "SUSTAIN");
            envR.bind (P::track (t, P::EnvR), "RELEASE");
        }
        // El tipo de acorde y el modelo de la melodía son listas: se eligen en pantalla
        const bool snd = tab == 0;
        for (int i = 1; i < kSoundParams; ++i) sound[i]->setVisible (snd);
        sound[0]->setVisible (snd && t != Chord && t != Melody);
        chordType.setVisible (snd && t == Chord);
        melModel.setVisible (snd && t == Melody);
        for (auto* k : strip) k->setVisible (! snd);
        stripType.setVisible (! snd);
        stripCompType.setVisible (! snd);
        for (auto* c : std::initializer_list<juce::Component*> { &bassOsc, &bassSub, &bassAccent, &bassLegato }) c->setVisible (snd && t == Bass);
        for (auto* c : std::initializer_list<juce::Component*> { &chordMode, &chordInv, &chordSpread, &chordStrum, &chordLead }) c->setVisible (snd && t == Chord && chordTab == 0);
        for (auto* c : std::initializer_list<juce::Component*> { &chordSource, &chordSample, &chordDice, &chordRand, &chordLevel }) c->setVisible (snd && t == Chord && chordTab == 1);
        for (auto* c : std::initializer_list<juce::Component*> { &tabVoicing, &tabSample }) c->setVisible (snd && t == Chord);
        tabVoicing.setToggleState (chordTab == 0, juce::dontSendNotification);
        tabSample.setToggleState (chordTab == 1, juce::dontSendNotification);
        for (auto* c : std::initializer_list<juce::Component*> { &melVoices, &melOctave }) c->setVisible (snd && t == Melody);
        for (auto* c : std::initializer_list<juce::Component*> { &texSample, &texDice, &texCat, &texRand, &texFollow, &texRev, &texFilter }) c->setVisible (snd && t == Texture);
        resized();
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
        drawSpaced (g, trackLabel (track), { 18.0f, 0.0f, 200.0f, 32.0f }, titleFont (19.0f), 0.1f, juce::Justification::centredLeft);
        const float nameW = (float) juce::GlyphArrangement::getStringWidthInt (titleFont (19.0f), trackLabel (track)) * 1.1f + 34.0f;
        g.setColour (col::violetValue);
        g.setFont (textFont (14.0f));
        const bool warn = (track == Texture || (track == Chord && proc.value (P::ChordSource) > 0.5f)) && status.isNotEmpty();
        if (warn) g.setColour (juce::Colour (0xffffb35c));
        g.setFont (textFont (13.0f));
        g.drawText (warn ? status : (tab == 1 ? T ("Tira analógica: todo en 0 = la pista suena sin procesar") : T (trackDescription (track))),
                    juce::Rectangle<float> (nameW, 0.0f, (float) getWidth() - nameW - 306.0f, 32.0f), juce::Justification::centredLeft, true);
        // Fila de controles propios (bajo, acordes, melodía)
        if (hasExtras)
        {
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.fillRoundedRectangle (extrasBox.toFloat(), 10.0f);
            if (tab == 1)
            {
                g.setColour (col::violetValue);
                g.setFont (textFont (13.0f));
                g.drawFittedText (T ("Candadito en cada perilla: el dado (VARIAR) no la mueve. La tira se puede automatizar, modular y poner en p-locks."),
                                  extrasBox.reduced (10, 4).withTrimmedLeft (290), juce::Justification::centredLeft, 3);
            }
            else if (track == Melody)
            {
                g.setColour (col::violetValue);
                g.setFont (textFont (13.0f));
                g.drawFittedText (T ("PLUCK: cuerda pulsada (TIMBRE apaga la cuerda) · LEAD: 2 sierras + sub · CAMPANA: FM (TIMBRE = relación, ENV = brillo) · ÓRGANO: 6 armónicos (TIMBRE = drawbars)"),
                                  extrasBox.reduced (10, 4).withTrimmedLeft (240), juce::Justification::centredLeft, 3);
            }
        }
        // Tarjetas de abajo
        paint::violetCard (g, mixBox.toFloat(), T ("Mezcla"));
        paint::violetCard (g, sendBox.toFloat(), T ("Envíos"));
        if (hasEnv) paint::violetCard (g, envBox.toFloat(), T ("Envolvente"));
        paint::violetCard (g, ringBox.toFloat(), T ("Ring"));
        // Traza de cobre: camino de la señal hacia los efectos
        const float ty = (float) dly.getBounds().getCentreY() - 4.0f;
        paint::copperTrace (g, { (float) dly.getBounds().getCentreX(), ty }, { (float) pump.getBounds().getCentreX(), ty }, 4.0f);
        if (! hasEnv)
        {
            g.setColour (col::violetValue);
            g.setFont (textFont (13.0f));
            g.drawFittedText (T ("Cuánto pasa por el ring modulator. La portadora se ajusta en el panel FX."),
                              ringBox.reduced (12).withTrimmedTop (34).withTrimmedLeft (84), juce::Justification::topLeft, 6);
        }
    }
    void resized() override
    {
        audition.setBounds (getWidth() - 96, 0, 96, 32);
        diceButton.setBounds (getWidth() - 172, 0, 72, 32);
        tabStrip.setBounds (getWidth() - 232, 2, 56, 28);
        tabSound.setBounds (getWidth() - 302, 2, 66, 28);
        // Tira: 7 perillas y las dos listas abajo
        {
            const int w7 = getWidth() / 7;
            for (int k = 0; k < 7; ++k) strip[k]->setBounds (k * w7, 42, w7, 100);
            stripType.setBounds (10, 152, 150, 50);
            stripCompType.setBounds (170, 152, 100, 50);
        }
        const int kh = hasExtras ? 100 : 104;
        if (track == Chord || track == Melody)
        {
            // La lista (tipo / modelo) ocupa más lugar que una perilla
            const int cw = 126, w7 = (getWidth() - cw) / (kSoundParams - 1);
            for (int i = 1; i < kSoundParams; ++i) sound[i]->setBounds (cw + (i - 1) * w7, 42, w7, kh);
            chordType.setBounds (4, 62, cw - 10, 50);
            melModel.setBounds (4, 62, cw - 10, 50);
        }
        else
        {
            const int w = getWidth() / kSoundParams;
            for (int i = 0; i < kSoundParams; ++i) sound[i]->setBounds (i * w, 42, w, kh);
        }
        // Controles propios de bajo, acordes y melodía
        extrasBox = { 0, 148, getWidth(), 58 };
        {
            const int ey = extrasBox.getY() + 4, eh = 50;
            auto place = [ey, eh] (juce::Component& c, int x, int cw) { c.setBounds (x, ey, cw, eh); };
            place (bassOsc, 10, 150); place (bassSub, 172, 136); place (bassAccent, 320, 150);
            bassLegato.setBounds (482, ey + 18, getWidth() - 492, 30);
            // Acordes: pestañas VOICING / SAMPLE a la izquierda
            tabVoicing.setBounds (6, ey, 64, 25);
            tabSample.setBounds (6, ey + 26, 64, 25);
            place (chordMode, 76, 104); place (chordInv, 184, 112); place (chordSpread, 300, 96); place (chordStrum, 400, 86);
            chordLead.setBounds (490, ey + 18, getWidth() - 496, 30);
            place (chordSource, 76, 104); place (chordSample, 184, 160);
            chordDice.setBounds (348, ey + 18, 52, 30);
            chordRand.setBounds (404, ey + 18, 96, 30);
            place (chordLevel, 506, getWidth() - 512);
            place (melVoices, 10, 128); place (melOctave, 146, 84);
            // Texturas
            place (texSample, 6, 140);
            texDice.setBounds (150, ey + 18, 48, 30);
            place (texCat, 202, 72);
            texRand.setBounds (278, ey + 18, 88, 30);
            texFollow.setBounds (370, ey + 18, 84, 30);
            texRev.setBounds (458, ey + 18, 84, 30);
            texFilter.setBounds (546, ey + 18, getWidth() - 546, 30);
        }
        const int y = hasExtras ? 214 : 162, h = getHeight() - y;
        const int ky = y + (hasExtras ? 30 : 40), kh2 = hasExtras ? h - 34 : 112;
        if (hasEnv)
        {
            mixBox = { 0, y, 150, h };
            sendBox = { 158, y, 210, h };
            envBox = { 376, y, 152, h };
            ringBox = { 536, y, getWidth() - 536, h };
            level.setBounds (mixBox.getX() + 4, ky, 70, kh2);
            pan.setBounds (mixBox.getX() + 76, ky, 70, kh2);
            dly.setBounds (sendBox.getX() + 4, ky, 67, kh2);
            rev.setBounds (sendBox.getX() + 71, ky, 67, kh2);
            pump.setBounds (sendBox.getX() + 138, ky, 67, kh2);
            if (hasAttack)
            {
                envA.setBounds (envBox.getX() + 2, ky, 50, kh2);
                envS.setBounds (envBox.getX() + 51, ky, 50, kh2);
                envR.setBounds (envBox.getX() + 100, ky, 50, kh2);
            }
            else
            {
                envS.setBounds (envBox.getX() + 6, ky, 70, kh2);
                envR.setBounds (envBox.getX() + 76, ky, 70, kh2);
            }
            ring.setBounds (ringBox.getX() + 4, ky, ringBox.getWidth() - 8, kh2);
        }
        else
        {
            mixBox = { 0, y, 196, h };
            sendBox = { 208, y, 236, h };
            ringBox = { 456, y, getWidth() - 456, h };
            level.setBounds (mixBox.getX() + 8, ky, 86, kh2);
            pan.setBounds (mixBox.getX() + 100, ky, 86, kh2);
            dly.setBounds (sendBox.getX() + 6, ky, 74, kh2);
            rev.setBounds (sendBox.getX() + 81, ky, 74, kh2);
            pump.setBounds (sendBox.getX() + 156, ky, 74, kh2);
            ring.setBounds (ringBox.getX() + 6, ky, 74, kh2);
        }
    }

private:
    Ogro2Processor& proc;
    int track = 0;
    juce::OwnedArray<Knob> sound;
    Knob level, pan, dly, rev, pump, ring, envA, envS, envR;
    Choice chordType, melModel, bassOsc;
    HSlider bassSub;
    Choice bassAccent;
    Toggle bassLegato;
    Choice chordMode, chordInv;
    HSlider chordSpread, chordStrum;
    Toggle chordLead;
    Choice melVoices, melOctave;
    SampleChoice texSample;
    Choice texCat;
    Toggle texRand, texFollow, texRev;
    Choice chordSource;
    SampleChoice chordSample;
    Toggle chordRand;
    HSlider chordLevel;
    juce::TextButton texDice, chordDice, texFilter, tabVoicing, tabSample;
    int chordTab = 0;
    juce::OwnedArray<Knob> strip;
    Choice stripType, stripCompType;
    juce::TextButton tabSound, tabStrip, diceButton;
    int tab = 0;
    juce::String status;
    juce::TextButton audition;
    bool hasEnv = false, hasAttack = false, hasExtras = false;
    juce::Rectangle<int> mixBox, sendBox, envBox, ringBox, extrasBox;
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
          gr (T ("GR"), true, 18.0f),
          ringOn (p, P::RingOn, "RING", col::ledGreen), ringFreq (p, P::RingFreq, "FREC."), ringWave (p, P::RingWave, ""),
          ringMix (p, P::RingMix, "MEZCLA"), ringSpread (p, P::RingSpread, T ("ESTÉREO"))
    {
        for (auto* c : std::initializer_list<juce::Component*> { &dlyDiv, &dlyFb, &dlyTone, &dlyRet, &revSize, &revDecay, &revRet,
                                                                 &pumpRel, &busComp, &busCrudo, &gr, &ringOn, &ringFreq, &ringWave, &ringMix, &ringSpread })
            addAndMakeVisible (c);
        ringFreq.slider.setTooltip (T ("Frecuencia de la portadora del ring: grave = trémolo, agudo = campanas y metal. Cuánto pasa cada pista se elige en su inspector (RING)."));
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
        paint::violetCard (g, b5.toFloat(), T ("Ring mod · portadora"));
    }
    void resized() override
    {
        const int w = (getWidth() - 12) / 2, h = (getHeight() - 12) / 2;
        b1 = { 0, 0, w, h }; b2 = { w + 12, 0, w, h };
        b3 = { 0, h + 12, 104, h }; b4 = { 116, h + 12, 210, h }; b5 = { 338, h + 12, getWidth() - 338, h };
        dlyDiv.setBounds (b1.getX() + 12, b1.getY() + 36, 86, 50);
        int x = b1.getX() + 104;
        for (auto* k : { &dlyFb, &dlyTone, &dlyRet }) { k->setBounds (x, b1.getY() + 30, 64, h - 40); x += 64; }
        x = b2.getX() + 18;
        for (auto* k : { &revSize, &revDecay, &revRet }) { k->setBounds (x, b2.getY() + 30, 88, h - 40); x += 92; }
        pumpRel.setBounds (b3.getX() + 8, b3.getY() + 30, 88, h - 40);
        busComp.setBounds (b4.getX() + 4, b4.getY() + 30, 80, h - 40);
        busCrudo.setBounds (b4.getX() + 86, b4.getY() + 30, 80, h - 40);
        gr.setBounds (b4.getRight() - 32, b4.getY() + 34, 22, h - 44);
        ringOn.setBounds (b5.getX() + 10, b5.getY() + 34, 82, 28);
        ringWave.setBounds (b5.getX() + 10, b5.getY() + 70, 82, 30);
        const int kx = b5.getX() + 96, kw = (b5.getWidth() - 100) / 3;
        ringFreq.setBounds (kx, b5.getY() + 30, kw, h - 40);
        ringMix.setBounds (kx + kw, b5.getY() + 30, kw, h - 40);
        ringSpread.setBounds (kx + 2 * kw, b5.getY() + 30, kw, h - 40);
    }

private:
    Ogro2Processor& proc;
    Choice dlyDiv;
    Knob dlyFb, dlyTone, dlyRet, revSize, revDecay, revRet, pumpRel, busComp, busCrudo;
    Meter gr;
    Toggle ringOn;
    Knob ringFreq;
    Choice ringWave;
    Knob ringMix, ringSpread;
    juce::Rectangle<int> b1, b2, b3, b4, b5;
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
            addAndMakeVisible (srcs.add (new RouteChoice (p, RouteChoice::Kind::ModSource, s)));
            addAndMakeVisible (dsts.add (new RouteChoice (p, RouteChoice::Kind::ModDest, s)));
            auto* amt = amts.add (new juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox));
            amt->getProperties().set ("bipolar", true);
            amt->setColour (juce::Slider::trackColourId, col::violetRing);
            amt->setDoubleClickReturnValue (true, 0.0);
            addAndMakeVisible (amt);
            amtAtt.add (new APVTS::SliderAttachment (p.state, juce::String (paramDefs()[(size_t) P::mod (s, P::ModAmt)].id), *amt));
            amt->setTooltip (T ("Cantidad (doble clic = 0)"));
        }
    }
    juce::String title() const override { return T ("MODULACIÓN"); }
    void tick() override
    {
        for (auto* s : scopes) s->repaint();
        for (auto* c : srcs) c->refresh();
        for (auto* c : dsts) c->refresh();
    }

    void paint (juce::Graphics& g) override
    {
        paint::violetCard (g, lfoBox.toFloat(), T ("LFOs sincronizados"));
        paint::violetCard (g, matBox.toFloat(), T ("Matriz · fuente → destino → cantidad"));
        g.setColour (col::violetRing);
        for (int l = 0; l < P::kLfos; ++l)
            drawSpaced (g, juce::String (l + 1), { (float) lfoBox.getX() + 8.0f, (float) rowY (l), 16.0f, 54.0f }, titleFont (14.0f), 0.0f,
                        juce::Justification::centred);
        g.setColour (col::violetValue);
        g.setFont (labelFont (12.0f));
        for (int s = 0; s < P::kSlots; ++s)
            g.drawText (juce::String (s + 1), juce::Rectangle<int> (matBox.getX() + 6, slotY (s), 18, 21), juce::Justification::centred);
        g.setFont (textFont (12.0f));
        g.drawText (T ("MOVIMIENTO escala LFOs y carriles (las macros como fuente no)."),
                    juce::Rectangle<int> (matBox.getX() + 12, matBox.getBottom() - 22, matBox.getWidth() - 24, 18), juce::Justification::centredLeft);
    }
    void resized() override
    {
        lfoBox = { 0, 0, 204, getHeight() };
        matBox = { 214, 0, getWidth() - 214, getHeight() };
        for (int l = 0; l < P::kLfos; ++l)
        {
            const int y = rowY (l);
            shapes[l]->setBounds (lfoBox.getX() + 28, y, 104, 26);
            rates[l]->setBounds (lfoBox.getX() + 28, y + 30, 104, 26);
            scopes[l]->setBounds (lfoBox.getX() + 138, y + 4, 58, 48);
        }
        for (int s = 0; s < P::kSlots; ++s)
        {
            const int y = slotY (s);
            auto r = juce::Rectangle<int> (matBox.getX() + 26, y, matBox.getWidth() - 36, 21);
            srcs[s]->setBounds (r.removeFromLeft (100));
            r.removeFromLeft (4);
            dsts[s]->setBounds (r.removeFromLeft (150));
            r.removeFromLeft (8);
            amts[s]->setBounds (r);
        }
    }

private:
    int rowY (int l) const { return 32 + l * 72; }
    int slotY (int s) const { return 32 + s * 23; }
    Ogro2Processor& proc;
    juce::OwnedArray<Choice> shapes, rates;
    juce::OwnedArray<RouteChoice> srcs, dsts;
    juce::OwnedArray<LfoScope> scopes;
    juce::OwnedArray<juce::Slider> amts;
    juce::OwnedArray<APVTS::SliderAttachment> amtAtt;
    juce::Rectangle<int> lfoBox, matBox;
};

// ---------------------------------------------------------------------------
// GROOVE: plantillas, humanización, swing por pista y polirritmos
// ---------------------------------------------------------------------------
class GroovePage : public Page
{
public:
    explicit GroovePage (Ogro2Processor& p)
        : proc (p),
          groove (p, P::Groove, "PLANTILLA"),
          amount (p, P::GrooveAmt, "CANTIDAD"),
          humanize (p, P::Humanize, "HUMANIZAR"),
          swing (p, P::Swing, "SWING"),
          poly (p, P::PolyMode, "POLIRRITMOS"),
          genre (p, P::Genre, T ("INVENTAR SEGÚN")),
          mutate (p, P::Mutate, "MUTAR")
    {
        for (auto* c : std::initializer_list<juce::Component*> { &groove, &amount, &humanize, &swing, &poly, &genre, &mutate })
            addAndMakeVisible (c);
        amount.setTitleSize (11.0f);
        humanize.setTitleSize (11.0f);
        groove.box.setTooltip (T ("Plantilla de tiempo y dinámica: MPC 60 acentúa, Shuffle 909 va en saltos, Suelto (Villalobos) "
                                  "corre cada semicorchea un poquito, Empuje (garage) adelanta los contratiempos"));
        humanize.slider.setTooltip (T ("Corre cada golpe unos milisegundos al azar (sale de la semilla: con AZAR FIJO se repite igual)"));
        poly.box.setTooltip (T ("Desde el patrón: las pistas de largo distinto arrancan juntas cada vez que entra el patrón. "
                                "Continuos: siguen girando desde el principio del tema (como la 1.0)"));
        for (int t = 0; t < kTracks; ++t)
        {
            auto* o = own.add (new Toggle (p, P::track (t, P::SwingOwn), "PROPIO", col::track (t)));
            o->button.setTooltip (T ("Esta pista usa su propio swing en vez del general"));
            addAndMakeVisible (o);
            auto* a = amts.add (new juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight));
            a->setTextBoxStyle (juce::Slider::TextBoxRight, true, 46, 20);
            a->setColour (juce::Slider::trackColourId, col::track (t));
            a->setColour (juce::Slider::textBoxTextColourId, col::violetValue);
            a->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            addAndMakeVisible (a);
            att.add (new APVTS::SliderAttachment (p.state, juce::String (paramDefs()[(size_t) P::track (t, P::SwingAmt)].id), *a));
        }
    }
    juce::String title() const override { return "GROOVE"; }
    void tick() override
    {
        for (int t = 0; t < kTracks; ++t) amts[t]->setEnabled (proc.value (P::track (t, P::SwingOwn)) > 0.5f);
    }
    void paint (juce::Graphics& g) override
    {
        paint::violetCard (g, tplBox.toFloat(), T ("Plantilla de groove"));
        paint::violetCard (g, genBox.toFloat(), T ("Polirritmos e INVENTAR"));
        paint::violetCard (g, swBox.toFloat(), T ("Swing por pista"));
        g.setFont (labelFont (12.0f));
        for (int t = 0; t < kTracks; ++t)
        {
            g.setColour (col::track (t));
            g.fillRoundedRectangle ((float) swBox.getX() + 12.0f, (float) rowY (t) + 6.0f, 4.0f, 14.0f, 2.0f);
            g.setColour (col::violetText);
            g.drawText (trackLabel (t), juce::Rectangle<int> (swBox.getX() + 22, rowY (t), 70, 26), juce::Justification::centredLeft);
        }
    }
    void resized() override
    {
        tplBox = { 0, 0, 300, 160 };
        genBox = { 0, 172, 300, getHeight() - 172 };
        swBox = { 312, 0, getWidth() - 312, getHeight() };
        groove.setBounds (12, 34, 150, 52);
        swing.setBounds (14, 92, 136, 26);
        amount.setBounds (166, 30, 64, 120);
        humanize.setBounds (230, 30, 64, 120);
        poly.setBounds (12, genBox.getY() + 34, 180, 52);
        genre.setBounds (12, genBox.getY() + 92, 180, 52);
        mutate.setBounds (198, genBox.getY() + 30, 92, genBox.getHeight() - 36);
        for (int t = 0; t < kTracks; ++t)
        {
            own[t]->setBounds (swBox.getX() + 92, rowY (t), 78, 26);
            amts[t]->setBounds (swBox.getX() + 176, rowY (t), swBox.getWidth() - 186, 26);
        }
    }

private:
    int rowY (int t) const { return 32 + t * 29; }
    Ogro2Processor& proc;
    Choice groove;
    Knob amount, humanize;
    struct SwingSlider : juce::Component
    {
        SwingSlider (Ogro2Processor& p, int idx, const char*) : sl (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight)
        {
            sl.setTextBoxStyle (juce::Slider::TextBoxRight, true, 46, 20);
            sl.setColour (juce::Slider::trackColourId, col::violetRing);
            sl.setColour (juce::Slider::textBoxTextColourId, col::violetValue);
            sl.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            addAndMakeVisible (sl);
            att = std::make_unique<APVTS::SliderAttachment> (p.state, juce::String (paramDefs()[(size_t) idx].id), sl);
        }
        void paint (juce::Graphics& g) override
        {
            g.setColour (col::violetText);
            drawSpaced (g, "SWING", getLocalBounds().removeFromLeft (52).toFloat(), labelFont (12.0f), 0.06f, juce::Justification::centredLeft);
        }
        void resized() override { sl.setBounds (getLocalBounds().withTrimmedLeft (52)); }
        juce::Slider sl;
        std::unique_ptr<APVTS::SliderAttachment> att;
    } swing;
    Choice poly, genre;
    Knob mutate;
    juce::OwnedArray<Toggle> own;
    juce::OwnedArray<juce::Slider> amts;
    juce::OwnedArray<APVTS::SliderAttachment> att;
    juce::Rectangle<int> tplBox, genBox, swBox;
};

// Cadena de patrones: fichas [patrón x repeticiones]; clic en la letra o en las vueltas para cambiarlas
class ChainStrip : public juce::Component
{
public:
    explicit ChainStrip (Ogro2Processor& p) : proc (p) {}
    void paint (juce::Graphics& g) override
    {
        const auto chain = proc.engine.bank.getChain();
        const int playing = proc.engine.meters.pattern.load();
        const bool on = proc.value (P::Chain) > 0.5f && proc.engine.meters.playing.load();
        for (int i = 0; i <= (int) chain.size() && i < kVisible; ++i)
        {
            const auto r = chip (i).toFloat();
            if (i == (int) chain.size())
            {
                g.setColour (col::violetText.withAlpha (0.5f));
                g.drawRoundedRectangle (r.reduced (1.0f), 6.0f, 1.2f);
                g.setFont (titleFont (16.0f));
                g.drawText ("+", r, juce::Justification::centred);
                continue;
            }
            const bool hot = on && chain[(size_t) i].first == playing;
            g.setColour (hot ? col::violet : juce::Colours::white.withAlpha (0.1f));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (col::violetEdge);
            g.drawRoundedRectangle (r, 6.0f, 1.0f);
            g.setColour (juce::Colours::white);
            g.setFont (titleFont (15.0f));
            g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + chain[(size_t) i].first)), r.withTrimmedRight (r.getWidth() * 0.45f),
                        juce::Justification::centred);
            g.setColour (col::violetValue);
            g.setFont (labelFont (11.0f));
            g.drawText ("x" + juce::String (chain[(size_t) i].second), r.withTrimmedLeft (r.getWidth() * 0.5f), juce::Justification::centred);
        }
        if (chain.empty())
        {
            g.setColour (col::violetValue);
            g.setFont (textFont (13.0f));
            g.drawFittedText (T ("Tocá + para sumar el patrón que está sonando. Con la cadena encendida, OGRO toca los patrones en orden según la posición del tema."),
                              getLocalBounds().withTrimmedTop (40), juce::Justification::topLeft, 3);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        auto chain = proc.engine.bank.getChain();
        for (int i = 0; i <= (int) chain.size() && i < kVisible; ++i)
        {
            const auto r = chip (i);
            if (! r.contains (e.getPosition())) continue;
            if (i == (int) chain.size())
            {
                proc.pushUndo();
                chain.push_back ({ (int) proc.value (P::Pattern), 1 });
                proc.engine.bank.setChain (chain);
                repaint();
                return;
            }
            const bool onRepeats = e.x > r.getX() + r.getWidth() / 2;
            juce::PopupMenu m;
            if (onRepeats)
                for (int n : { 1, 2, 3, 4, 6, 8, 12, 16 }) m.addItem (100 + n, "x" + juce::String (n), true, chain[(size_t) i].second == n);
            else
                for (int k = 0; k < kPatterns; ++k)
                    m.addItem (1 + k, T ("Patrón ") + juce::String::charToString ((juce::juce_wchar) ('A' + k)), true, chain[(size_t) i].first == k);
            m.addSeparator();
            m.addItem (999, T ("Quitar de la cadena"));
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this, i] (int res) {
                if (res <= 0) return;
                auto c = proc.engine.bank.getChain();
                if (i >= (int) c.size()) return;
                proc.pushUndo();
                if (res == 999) c.erase (c.begin() + i);
                else if (res > 100) c[(size_t) i].second = res - 100;
                else c[(size_t) i].first = res - 1;
                proc.engine.bank.setChain (c);
                repaint();
            });
            return;
        }
    }

private:
    static constexpr int kVisible = 16;
    juce::Rectangle<int> chip (int i) const
    {
        const int cols = 6, w = (getWidth() - (cols - 1) * 6) / cols;
        return { (i % cols) * (w + 6), (i / cols) * 36, w, 30 };
    }
    Ogro2Processor& proc;
};

// ---------------------------------------------------------------------------
// ARREGLO: cadena de patrones, cambio de patrón, fills y efectos de fin de compás
// ---------------------------------------------------------------------------
class ArrangePage : public Page
{
public:
    explicit ArrangePage (Ogro2Processor& p)
        : proc (p),
          chainOn (p, P::Chain, "CADENA", col::ledGreen),
          chain (p),
          change (p, P::PatChange, T ("CAMBIO DE PATRÓN")),
          fill (p, P::Fill, "FILL"),
          fillPat (p, P::FillPat, T ("FILL: PATRÓN")),
          every (p, P::BfxEvery, "FX CADA"),
          len (p, P::BfxLen, "LARGO"),
          rate (p, P::BfxStutterRate, "STUTTER")
    {
        for (auto* c : std::initializer_list<juce::Component*> { &chainOn, &chain, &change, &fill, &fillPat, &every, &len, &rate, &amount, &fireNow, &clearChain })
            addAndMakeVisible (c);
        chainOn.button.setTooltip (T ("Encendida: los patrones se tocan en el orden de la cadena según la posición del tema (sirve para exportar el tema entero)"));
        change.box.setTooltip (T ("Cuándo entra el patrón que elegís con A-H"));
        fill.box.setTooltip (T ("Cada cuántos compases hay un compás de fill. Los pasos con condición FILL suenan solo ahí."));
        fillPat.box.setTooltip (T ("En el compás de fill: solo condiciones, o tocar otro patrón entero"));
        every.box.setTooltip (T ("Cada cuántos compases se tiran los efectos de fin de compás"));
        len.box.setTooltip (T ("Cuántos pasos antes del uno del compás siguiente arrancan"));
        amount.setSliderStyle (juce::Slider::LinearHorizontal);
        amount.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        amount.setColour (juce::Slider::trackColourId, col::orange);
        amountAtt = std::make_unique<APVTS::SliderAttachment> (p.state, juce::String (paramDefs()[P::BfxAmount].id), amount);
        amount.setTooltip (T ("Intensidad de todos los efectos de fin de compás"));
        fireNow.setButtonText ("TIRAR FX");
        styleKey (fireNow, col::orange);
        fireNow.setTooltip (T ("Dispara ahora los efectos de fin de compás (según su probabilidad), por el largo elegido"));
        fireNow.onClick = [this] { proc.engine.fireBarFxNow(); };
        clearChain.setButtonText ("BORRAR");
        styleKey (clearChain, col::keyCream);
        clearChain.setTooltip (T ("Vacía la cadena"));
        clearChain.onClick = [this] { proc.pushUndo(); proc.engine.bank.setChain ({}); chain.repaint(); };
        for (int k = 0; k < P::kBfx; ++k)
        {
            auto* pr = probs.add (new juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox));
            pr->setColour (juce::Slider::trackColourId, col::violetRing);
            pr->setTooltip (T ("Probabilidad de ") + T (bfxName (k)) + T (" en cada final"));
            addAndMakeVisible (pr);
            probAtt.add (new APVTS::SliderAttachment (p.state, juce::String (paramDefs()[(size_t) P::bfx (k, P::BfxProb)].id), *pr));
            auto* tg = targets.add (new Choice (p, P::bfx (k, P::BfxTarget), ""));
            tg->box.setTooltip (T ("Dónde actúa: todo el master, el bus de batería, bajo y acordes o una pista"));
            addAndMakeVisible (tg);
        }
    }
    juce::String title() const override { return "ARREGLO"; }
    void tick() override
    {
        chain.repaint();
        const uint32_t on = proc.engine.meters.barFx.load();
        if (on != lastOn) { lastOn = on; repaint (fxBox); }
    }
    void paint (juce::Graphics& g) override
    {
        paint::violetCard (g, chainBox.toFloat(), T ("Cadena de patrones"));
        paint::violetCard (g, fillBox.toFloat(), T ("Cambio y fills"));
        paint::violetCard (g, fxBox.toFloat(), T ("FX de fin de compás"));
        g.setColour (col::violetText);
        drawSpaced (g, "CANTIDAD", { (float) fxBox.getX() + 12.0f, (float) fxBox.getY() + 94.0f, 80.0f, 22.0f }, labelFont (12.0f), 0.06f,
                    juce::Justification::centredLeft);
        g.setFont (labelFont (12.0f));
        for (int k = 0; k < P::kBfx; ++k)
        {
            const auto r = juce::Rectangle<int> (fxBox.getX() + 12, fxRow (k), 104, 24);
            paint::led (g, { (float) r.getX(), (float) r.getCentreY() - 3.5f, 10.0f, 7.0f }, col::orange, (lastOn >> k) & 1u);
            g.setColour (col::violetText);
            g.drawText (T (bfxName (k)), r.withTrimmedLeft (16), juce::Justification::centredLeft, true);
        }
    }
    void resized() override
    {
        chainBox = { 0, 0, 300, 176 };
        fillBox = { 0, 188, 300, getHeight() - 188 };
        fxBox = { 312, 0, getWidth() - 312, getHeight() };
        chainOn.setBounds (150, 6, 140, 26);
        chain.setBounds (12, 40, 276, 104);
        clearChain.setBounds (214, 144, 74, 26);
        change.setBounds (12, fillBox.getY() + 32, 276, 50);
        fill.setBounds (12, fillBox.getY() + 86, 134, 50);
        fillPat.setBounds (154, fillBox.getY() + 86, 134, 50);
        const int x = fxBox.getX() + 12, w = fxBox.getWidth() - 24;
        every.setBounds (x, 30, 112, 50);
        len.setBounds (x + 118, 30, 92, 50);
        rate.setBounds (x + 216, 30, w - 216, 50);
        amount.setBounds (x + 76, 92, w - 76 - 96, 26);
        fireNow.setBounds (x + w - 90, 88, 90, 32);
        for (int k = 0; k < P::kBfx; ++k)
        {
            probs[k]->setBounds (x + 108, fxRow (k), w - 108 - 92, 24);
            targets[k]->setBounds (x + w - 88, fxRow (k), 88, 24);
        }
    }

private:
    int fxRow (int k) const { return 128 + k * 28; }
    Ogro2Processor& proc;
    Toggle chainOn;
    ChainStrip chain;
    Choice change, fill, fillPat, every, len, rate;
    juce::Slider amount;
    std::unique_ptr<APVTS::SliderAttachment> amountAtt;
    juce::TextButton fireNow, clearChain;
    juce::OwnedArray<juce::Slider> probs;
    juce::OwnedArray<APVTS::SliderAttachment> probAtt;
    juce::OwnedArray<Choice> targets;
    juce::Rectangle<int> chainBox, fillBox, fxBox;
    uint32_t lastOn = 0;
};

// ---------------------------------------------------------------------------
// ESCENAS: A/B con MORPH, deriva lenta, carácter y consola
// ---------------------------------------------------------------------------
class ScenesPage : public Page
{
public:
    explicit ScenesPage (Ogro2Processor& p)
        : proc (p),
          morphOn (p, P::MorphOn, "MORPH", col::ledGreen),
          morph (p, P::Morph, "MORPH  A  ->  B", Theme::Violet),
          drift (p, P::Drift, "DERIVA"),
          driftBars (p, P::DriftBars, "CICLO"),
          character (p, P::Character, T ("CARÁCTER")),
          console (p, P::Console, "CONSOLA", col::orange)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &morphOn, &morph, &drift, &driftBars, &character, &console })
            addAndMakeVisible (c);
        morph.setTitleSize (12.0f);
        morph.setTravel (520);
        static const char* names[] = { "GUARDAR A", "GUARDAR B", "IR A  A", "IR A  B" };
        for (int i = 0; i < 4; ++i)
        {
            auto* b = buttons.add (new juce::TextButton (names[i]));
            styleKey (*b, i < 2 ? col::orange : col::keyCream);
            b->onClick = [this, i] {
                if (i < 2) proc.storeScene (i);
                else proc.recallScene (i - 2);
                repaint();
            };
            addAndMakeVisible (b);
        }
        buttons[0]->setTooltip (T ("Guarda cómo están ahora todas las perillas (sonido, mezcla, tira, FX y master) como escena A"));
        buttons[1]->setTooltip (T ("Lo mismo como escena B"));
        buttons[2]->setTooltip (T ("Vuelve las perillas a la escena A"));
        buttons[3]->setTooltip (T ("Vuelve las perillas a la escena B"));
        morph.slider.setTooltip (T ("Con MORPH encendido, las perillas que son distintas en A y B se mueven entre las dos. Automatizalo en Ableton para "
                                    "transiciones largas. Las que son iguales en las dos siguen respondiendo a la perilla."));
        drift.slider.setTooltip (T ("Deriva lenta: las perillas de sonido de todas las pistas se mueven solas, muy despacio, como un equipo analógico "
                                    "que se calienta. Sale de la semilla: con AZAR FIJO cada render sale igual."));
        character.box.setTooltip (T ("ORIGINAL: el sonido de siempre (como la 1.0). LIMPIO: saturaciones sin aliasing, perillas suavizadas (sin "
                                     "escalones) y, con CALIDAD ALTA, osciladores todavía más limpios."));
        console.button.setTooltip (T ("Suma de consola: un poco de saturación y diafonía al sumar las pistas (sutil, apagada por defecto)"));
    }
    juce::String title() const override { return "ESCENAS"; }
    void tick() override
    {
        const int st = (proc.hasScene (0) ? 1 : 0) + (proc.hasScene (1) ? 2 : 0);
        if (st != lastState) { lastState = st; repaint(); }
    }
    void paint (juce::Graphics& g) override
    {
        paint::violetCard (g, sceneBox.toFloat(), T ("Escenas A / B y MORPH"));
        paint::violetCard (g, driftBox.toFloat(), "Deriva lenta");
        paint::violetCard (g, charBox.toFloat(), T ("Carácter y consola"));
        for (int w = 0; w < 2; ++w)
        {
            const auto r = juce::Rectangle<float> ((float) sceneBox.getX() + 14.0f + (float) w * 136.0f, (float) sceneBox.getY() + 118.0f, 126.0f, 16.0f);
            paint::led (g, { r.getX(), r.getCentreY() - 3.5f, 12.0f, 7.0f }, col::ledGreen, proc.hasScene (w));
            g.setColour (col::violetValue);
            g.setFont (textFont (12.0f));
            g.drawText (juce::String (w == 0 ? "A" : "B") + (proc.hasScene (w) ? T (": guardada") : T (": vacía")), r.withTrimmedLeft (18.0f),
                        juce::Justification::centredLeft);
        }
        g.setColour (col::violetValue);
        g.setFont (textFont (12.5f));
        g.drawFittedText (T ("ORIGINAL es el sonido de siempre. LIMPIO saca el aliasing de las saturaciones y suaviza las perillas."),
                          charBox.reduced (12).withTrimmedTop (92), juce::Justification::topLeft, 3);
    }
    void resized() override
    {
        sceneBox = { 0, 0, 300, getHeight() };
        driftBox = { 312, 0, getWidth() - 312, 150 };
        charBox = { 312, 162, getWidth() - 312, getHeight() - 162 };
        for (int i = 0; i < 4; ++i) buttons[i]->setBounds (sceneBox.getX() + 12 + (i % 2) * 138, sceneBox.getY() + 34 + (i / 2) * 40, 132, 34);
        morphOn.setBounds (sceneBox.getX() + 12, sceneBox.getY() + 146, 110, 30);
        morph.setBounds (sceneBox.getX() + 60, sceneBox.getY() + 180, 180, getHeight() - 186);
        drift.setBounds (driftBox.getX() + 10, driftBox.getY() + 28, 100, 116);
        driftBars.setBounds (driftBox.getX() + 122, driftBox.getY() + 50, 150, 52);
        character.setBounds (charBox.getX() + 12, charBox.getY() + 34, 140, 52);
        console.setBounds (charBox.getX() + 164, charBox.getY() + 52, 112, 32);
    }

private:
    Ogro2Processor& proc;
    Toggle morphOn;
    Knob morph, drift;
    Choice driftBars, character;
    Toggle console;
    juce::OwnedArray<juce::TextButton> buttons;
    juce::Rectangle<int> sceneBox, driftBox, charBox;
    int lastState = -1;
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
          quality (p, P::Quality, "CALIDAD"),
          grComp ("COMP", true, 12.0f), grLim ("LIMIT", true, 12.0f)
    {
        addAndMakeVisible (quality);
        quality.box.setTooltip (T ("LIVE: liviano para tocar. ALTA: interpolación sinc en los samples (y más calidad en las próximas versiones). AUTO: LIVE en vivo y ALTA cuando Ableton congela o exporta."));
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
        dynBox = { 162, h2 + 12, getWidth() - 162 - 168, h2 };
        texBox = { getWidth() - 156, h2 + 12, 156, h2 };
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
        texture.setBounds (texBox.getX() + 4, texBox.getY() + 30, 70, h2 - 38);
        quality.setBounds (texBox.getX() + 78, texBox.getY() + 54, 70, 50);
    }

private:
    Ogro2Processor& proc;
    Knob filter, reso, lowcut, body, pres, air, comp, color, warmth, parallel, texture;
    Choice quality;
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
        b->setTooltip (T ("Patrón ") + b->getButtonText() + T (" (entra según ARREGLO > Cambio de patrón: al compás, al final del patrón o ya)"));
        addAndMakeVisible (b);
    }
    playOrb.onClick = [this] { proc.engine.setInternalPlay (playOrb.getToggleState()); };
    playOrb.setTooltip (T ("Toca sin el transporte de Ableton (para probar). Con Ableton en play, OGRO sigue a Ableton."));
    addAndMakeVisible (keyDisplay);

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
    pages.add (new GroovePage (p));
    pages.add (new ArrangePage (p));
    pages.add (new ScenesPage (p));
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
    static const char* panelNames[] = { "FX", "MOD", "GROOVE", "ARREGLO", "ESCENAS", "MASTER" };
    static const char* panelTips[] = { "Delay, reverb, pump, drum bus y portadora del ring mod", "LFOs y matriz de modulación",
                                       "Plantillas de groove, humanizar, swing por pista y polirritmos",
                                       "Cadena de patrones, cambio de patrón, fills y efectos de fin de compás",
                                       "Escenas A/B con MORPH, deriva lenta, carácter (ORIGINAL / LIMPIO) y suma de consola",
                                       "Filtro DJ, EQ, compresión, textura y calidad del master" };
    for (int i = 0; i < 6; ++i)
    {
        auto* b = panelButtons.add (new juce::TextButton (panelNames[i]));
        styleKey (*b, col::keyCream);
        b->setColour (juce::TextButton::buttonOnColourId, col::ink);
        b->setTooltip (T (panelTips[i]));
        b->onClick = [this, i] { showPage (currentPage == i + 1 ? (int) PageTrack : i + 1); };
        addAndMakeVisible (b);
    }
    depthButton.setTooltip (T ("Recorrido de las macros: x2 (el doble que la 1.0) o x1 (como la 1.0, lo usan los presets importados)"));
    styleKey (depthButton, col::keyCream);
    depthButton.onClick = [this] { proc.setValue (P::MacroDepth, proc.value (P::MacroDepth) > 0.5f ? 0.0f : 1.0f); };
    addAndMakeVisible (depthButton);
    addAndMakeVisible (disc);
    ceiling = std::make_unique<Knob> (p, P::MCeiling, "TECHO", Theme::Cream);
    volume = std::make_unique<Knob> (p, P::MVol, "VOLUMEN", Theme::Cream);
    addAndMakeVisible (*ceiling);
    addAndMakeVisible (*volume);

    // ---- Groove ----
    swing = std::make_unique<Knob> (p, P::Swing, "SWING", Theme::Cream);
    addAndMakeVisible (*swing);
    randomButton.setClickingTogglesState (false);
    styleKey (randomButton, col::violet, juce::Colours::white);
    randomButton.onClick = [this] { proc.setValue (P::RandomMode, proc.value (P::RandomMode) < 0.5f ? 1.0f : 0.0f); };
    randomButton.setTooltip (T ("FIJO: el azar (probabilidades, S&H, VARIA, ruido) sale de la semilla y cada render sale igual. "
                               "VIVO: cambia cada vez, como en la 1.0."));
    addAndMakeVisible (randomButton);
    addAndMakeVisible (seedBox);
    styleKey (lenMinus, col::keyCream);
    styleKey (lenPlus, col::keyCream);
    lenMinus.onClick = [this] { proc.pushUndo(); auto& b = proc.engine.bank; b.setLength (ctx.pattern(), ctx.track(), b.getLength (ctx.pattern(), ctx.track()) - 1); lengthChanged(); };
    lenPlus.onClick = [this] { proc.pushUndo(); auto& b = proc.engine.bank; b.setLength (ctx.pattern(), ctx.track(), b.getLength (ctx.pattern(), ctx.track()) + 1); lengthChanged(); };
    lenMinus.setTooltip (T ("Largo de la pista: con largos distintos se arman polirritmos"));
    lenPlus.setTooltip (T ("Largo de la pista: con largos distintos se arman polirritmos"));
    inventButton.setButtonText ("INVENTAR");
    styleKey (inventButton, col::orange);
    inventButton.setTooltip (T ("Genera un patrón nuevo para esta pista, con sentido musical según el género (▾ para elegirlo o inventar todas)"));
    inventButton.onClick = [this] {
        proc.pushUndo();
        inventPattern (proc.engine.bank, ctx.pattern(), ctx.track(), (uint32_t) juce::Random::getSystemRandom().nextInt(), (int) proc.value (P::Genre),
                       proc.value (P::Tonal) > 0.5f);
        lengthChanged();
    };
    styleKey (inventMenu, col::orange);
    inventMenu.setTooltip (T ("Género de INVENTAR e inventar todas las pistas"));
    inventMenu.onClick = [this] { showInventMenu(); };
    mutateButton.setButtonText ("MUTAR");
    styleKey (mutateButton, col::keyBlue);
    mutateButton.setTooltip (T ("Cambia un poco el patrón de esta pista sin perder el esqueleto (cuánto: GROOVE > MUTAR). Clic derecho: todas las pistas."));
    mutateButton.onClick = [this] {
        proc.pushUndo();
        const auto seed = (uint32_t) juce::Random::getSystemRandom().nextInt();
        if (juce::ModifierKeys::currentModifiers.isPopupMenu() || juce::ModifierKeys::currentModifiers.isShiftDown())
            for (int t = 0; t < kTracks; ++t) mutatePattern (proc.engine.bank, ctx.pattern(), t, proc.value (P::Mutate), seed + (uint32_t) t, proc.value (P::Tonal) > 0.5f);
        else
            mutatePattern (proc.engine.bank, ctx.pattern(), ctx.track(), proc.value (P::Mutate), seed, proc.value (P::Tonal) > 0.5f);
    };
    mutateButton.setTriggeredOnMouseDown (false);
    clearButton.setButtonText ("LIMPIAR");
    copyButton.setButtonText ("COPIAR");
    pasteButton.setButtonText ("PEGAR");
    for (auto* b : { &clearButton, &copyButton, &pasteButton }) styleKey (*b, col::keyCream);
    clearButton.setTooltip (T ("Borra los pasos de esta pista en el patrón actual"));
    copyButton.setTooltip (T ("Copia el patrón completo (todas las pistas)"));
    pasteButton.setTooltip (T ("Pega el patrón copiado en el patrón actual"));
    clearButton.onClick = [this] {
        proc.pushUndo();
        for (int s = 0; s < kSteps; ++s) { proc.engine.bank.set (ctx.pattern(), ctx.track(), s, Step()); proc.engine.bank.clearLocks (ctx.pattern(), ctx.track(), s); }
    };
    copyButton.onClick = [this] { clipboardPattern = ctx.pattern(); };
    pasteButton.onClick = [this] {
        if (clipboardPattern < 0 || clipboardPattern == ctx.pattern()) return;
        proc.pushUndo();
        proc.engine.bank.copyPattern (clipboardPattern, ctx.pattern());
        lengthChanged();
    };
    for (auto* c : std::initializer_list<juce::Component*> { &lenMinus, &lenPlus, &inventButton, &inventMenu, &mutateButton, &clearButton, &copyButton, &pasteButton })
        addAndMakeVisible (c);

    // ---- Secuenciador ----
    addAndMakeVisible (lane);
    addAndMakeVisible (keys);
    static const char* modes[] = { "VEL", "NOTA", "PROB", "RATCH", "LIGA", "OCT", "NUDGE", "GATE", "COND", "P-LOCK", "MOTION", "TIPO" };
    static const char* modeTips[] = {
        "Velocidad de cada paso (arrastrá)",
        "Nota de cada paso en semitonos (bajo y acordes muestran la nota)",
        "Probabilidad de que suene cada paso",
        "Repeticiones dentro del paso (1 a 8)",
        "Ligado: bajo y acordes cambian de nota sin re-disparar (glide)",
        "Octava de cada paso (de -3 a +3)",
        "Corrimiento fino de cada paso: hasta medio paso antes o después",
        "Largo de la nota en pasos (AUTO = lo que dura el paso). Con SUSTAIN, el bajo y los acordes duran eso.",
        "Condición: que suene solo en el fill, en ciertas vueltas (1:2, 3:4...), si sonó el paso anterior... (clic en el paso)",
        "P-LOCK: un valor propio de una perilla en cada paso (elegí la perilla arriba). Clic derecho borra.",
        "MOTION: curvas de automatización dibujadas de 1 a 8 compases",
        "TIPO: acorde propio de cada paso en ACORDES (como un p-lock del tipo)",
    };
    for (int i = 0; i < 12; ++i)
    {
        auto* b = modeButtons.add (new juce::TextButton (modes[i]));
        styleKey (*b, col::keyCream);
        b->setColour (juce::TextButton::buttonOnColourId, col::ink);
        b->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        b->setTooltip (T (modeTips[i]));
        b->onClick = [this, i] { setLaneMode ((LaneMode) i); };
        addAndMakeVisible (b);
    }
    addChildComponent (lockChoice);
    for (auto* b : { &page1, &page2 })
    {
        styleKey (*b, col::keyCream);
        b->setColour (juce::TextButton::buttonOnColourId, col::ink);
        b->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        addAndMakeVisible (b);
    }
    page1.setTooltip (T ("Ver los pasos 1 a 16"));
    page2.setTooltip (T ("Ver los pasos 17 a 32 (alargá la pista con LARGO)"));
    page1.onClick = [this] { proc.seqPage = 0; lane.repaint(); keys.repaint(); };
    page2.onClick = [this] { proc.seqPage = 1; lane.repaint(); keys.repaint(); };
    for (auto* b : { &undoButton, &redoButton }) { styleKey (*b, col::keyCream); addAndMakeVisible (b); }
    undoButton.setTooltip (T ("Deshacer el último cambio de pasos (patrones, p-locks, cadena, MOTION)"));
    redoButton.setTooltip (T ("Rehacer"));
    undoButton.onClick = [this] { if (proc.undo()) { lane.repaint(); keys.repaint(); for (auto* c : motionCanvas) c->repaint(); } };
    redoButton.onClick = [this] { if (proc.redo()) { lane.repaint(); keys.repaint(); for (auto* c : motionCanvas) c->repaint(); } };
    styleKey (euclidButton, col::keyBlue);
    euclidButton.setTooltip (T ("Ritmo euclidiano: reparte N golpes lo más parejo posible en la pista"));
    euclidButton.onClick = [this] { showEuclid(); };
    addAndMakeVisible (euclidButton);

    // MOTION en el secuenciador
    static const char* shapeNames[] = { "RAMPA", "SENO", "TRI", "AZAR", "PLANO" };
    for (int l = 0; l < kLanes; ++l)
    {
        auto* b = motionLaneButtons.add (new juce::TextButton ("M" + juce::String (l + 1)));
        styleKey (*b, col::keyCream);
        b->setColour (juce::TextButton::buttonOnColourId, col::violet);
        b->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        b->setTooltip (T ("Carril MOTION ") + juce::String (l + 1));
        b->onClick = [this, l] { setMotionLane (l); };
        addChildComponent (b);
        addChildComponent (motionCanvas.add (new MotionCanvas (p, l)));
        addChildComponent (motionDest.add (new RouteChoice (p, RouteChoice::Kind::LaneDest, l)));
        addChildComponent (motionBars.add (new Choice (p, P::lane (l, P::LaneBars), "")));
        auto* d = motionDepth.add (new juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox));
        d->setColour (juce::Slider::trackColourId, juce::Colour (0xffb48cff));
        d->setTooltip (T ("Profundidad del carril"));
        motionDepthAtt.add (new APVTS::SliderAttachment (p.state, juce::String (paramDefs()[(size_t) P::lane (l, P::LaneDepth)].id), *d));
        addChildComponent (d);
    }
    for (int sh = 0; sh < 5; ++sh)
    {
        auto* b = motionShapes.add (new juce::TextButton (shapeNames[sh]));
        styleKey (*b, col::keyCream);
        b->onClick = [this, sh] {
            proc.pushUndo();
            const uint32_t seed = (uint32_t) juce::Random::getSystemRandom().nextInt();
            auto f = sh == 0 ? std::function<float (float)> (pb::rampUp)
                   : sh == 1 ? std::function<float (float)> (pb::sine)
                   : sh == 2 ? std::function<float (float)> (pb::tri)
                   : sh == 3 ? pb::steps (seed, 16)
                             : std::function<float (float)> ([] (float) { return 0.5f; });
            pb::lane (proc.engine.bank, motionLane, f);
            motionCanvas[motionLane]->repaint();
        };
        addChildComponent (b);
    }
    setLaneMode (LaneMode::Vel);

    refreshPreset();
    selectTrack (proc.selectedTrack);
    showPage (PageTrack);
    startTimerHz (30);
}

Content::~Content() { stopTimer(); }

void Content::refreshPreset()
{
    const auto& presets = factoryPresets();
    const auto user = proc.getUserPresetName();
    if (user.isNotEmpty())
        presetDisplay.setText (user, T ("preset propio"));
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
                   true, proc.getUserPresetName().isEmpty() && i == proc.getPresetIndex());
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetDisplay), [this] (int r) {
        if (r <= 0) return;
        proc.loadPreset (r - 1);
        refreshPreset();
        selectTrack (proc.selectedTrack);
    });
}

void Content::showFileMenu()
{
    juce::PopupMenu m;
    m.addItem (1, T ("Guardar preset propio..."));
    m.addItem (2, T ("Abrir preset propio..."));
    m.addSeparator();
    m.addItem (3, T ("Importar preset de OGRO 1.0..."));
    m.addSeparator();
    m.addSectionHeader (T ("SAMPLES  ·  ") + proc.getSampleFolder().getFullPathName().replace (juce::File::getSpecialLocation (juce::File::userHomeDirectory).getFullPathName(), "~"));
    m.addItem (4, T ("Carpeta de samples..."));
    const bool canCopy = Ogro2Processor::orbhitzSource().isDirectory() && ! Ogro2Processor::defaultSampleFolder().isDirectory();
    if (canCopy) m.addItem (5, T ("Copiar Orbhitz a Música > OGRO Samples > Orbhitz (solo CHD, PAD, HIT y SFX)"), ! proc.copying.load());
    m.addItem (6, T ("Ver el análisis de afinación (JSON editable)"), proc.getSampleFolder().getChildFile ("ogro2_samples.json").existsAsFile());
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (fileButton), [this] (int r) {
        if (r == 1)
        {
            chooser = std::make_unique<juce::FileChooser> ("Guardar preset de OGRO 2", Ogro2Processor::presetFolder().getChildFile ("Mi preset.ogro2"), "*.ogro2");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [this] (const juce::FileChooser& fc) {
                                      auto f = fc.getResult();
                                      if (f == juce::File()) return;
                                      if (! f.hasFileExtension ("ogro2")) f = f.withFileExtension ("ogro2");
                                      proc.saveUserPreset (f);
                                      refreshPreset();
                                  });
        }
        else if (r == 2)
        {
            chooser = std::make_unique<juce::FileChooser> ("Abrir preset de OGRO 2", Ogro2Processor::presetFolder(), "*.ogro2");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc) {
                                      const auto f = fc.getResult();
                                      if (f.existsAsFile() && proc.loadUserPreset (f)) { refreshPreset(); selectTrack (proc.selectedTrack); }
                                  });
        }
        else if (r == 4)
        {
            chooser = std::make_unique<juce::FileChooser> (T ("Carpeta con los samples (.mse de Orbhitz)"), proc.getSampleFolder());
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                                  [this] (const juce::FileChooser& fc) {
                                      const auto d = fc.getResult();
                                      if (d.isDirectory()) proc.setSampleFolder (d);
                                  });
        }
        else if (r == 5)
        {
            proc.copyOrbhitz ([] (bool ok, juce::String msg) {
                juce::AlertWindow::showMessageBoxAsync (ok ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon, "OGRO 2", msg);
            });
        }
        else if (r == 6)
            proc.getSampleFolder().getChildFile ("ogro2_samples.json").revealToUser();
        else if (r == 3)
        {
            const auto v1Dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("OGRO Presets");
            chooser = std::make_unique<juce::FileChooser> (T ("Importar preset de OGRO 1.0 (suena igual: el azar queda en VIVO)"),
                                                           v1Dir.isDirectory() ? v1Dir : Ogro2Processor::presetFolder(), "*.ogro");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc) {
                                      const auto f = fc.getResult();
                                      if (f.existsAsFile() && proc.importV1Preset (f)) { refreshPreset(); selectTrack (proc.selectedTrack); }
                                  });
        }
    });
}

void Content::setLaneMode (LaneMode m)
{
    lane.setMode (m);
    for (int k = 0; k < modeButtons.size(); ++k) modeButtons[k]->setToggleState (k == (int) m, juce::dontSendNotification);
    const bool motion = m == LaneMode::Motion;
    lane.setVisible (! motion);
    keys.setVisible (! motion);
    lockChoice.setVisible (m == LaneMode::Lock);
    for (auto* b : { &page1, &page2, &euclidButton }) b->setVisible (! motion);
    for (auto* b : motionLaneButtons) b->setVisible (motion);
    for (auto* b : motionShapes) b->setVisible (motion);
    setMotionLane (motionLane);
    repaint (seqCard);
}

void Content::setMotionLane (int l)
{
    motionLane = juce::jlimit (0, kLanes - 1, l);
    const bool motion = lane.getMode() == LaneMode::Motion;
    for (int k = 0; k < kLanes; ++k)
    {
        const bool v = motion && k == motionLane;
        motionCanvas[k]->setVisible (v);
        motionDest[k]->setVisible (v);
        motionBars[k]->setVisible (v);
        motionDepth[k]->setVisible (v);
        motionLaneButtons[k]->setToggleState (k == motionLane, juce::dontSendNotification);
    }
}

void Content::showInventMenu()
{
    juce::PopupMenu m;
    m.addItem (1, T ("Inventar todas las pistas"));
    m.addSeparator();
    m.addSectionHeader (T ("GÉNERO"));
    const int g = (int) proc.value (P::Genre);
    for (int i = 0; i < (int) genreNames().size(); ++i) m.addItem (100 + i, juce::String::fromUTF8 (genreNames()[(size_t) i].c_str()), true, i == g);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (inventMenu), [this] (int r) {
        if (r == 1)
        {
            proc.pushUndo();
            inventAll (proc.engine.bank, ctx.pattern(), (uint32_t) juce::Random::getSystemRandom().nextInt(), (int) proc.value (P::Genre),
                       proc.value (P::Tonal) > 0.5f);
            lengthChanged();
        }
        else if (r >= 100) proc.setValue (P::Genre, (float) (r - 100));
    });
}

void Content::showEuclid()
{
    auto panel = std::make_unique<EuclidPanel> (proc.engine.bank.getLength (ctx.pattern(), ctx.track()));
    panel->onApply = [this] (int pulses, int steps, int rot) {
        proc.pushUndo();
        euclidPattern (proc.engine.bank, ctx.pattern(), ctx.track(), pulses, steps, rot);
        lengthChanged();
    };
    juce::CallOutBox::launchAsynchronously (std::move (panel), euclidButton.getScreenBounds(), nullptr);
}

void Content::lengthChanged()
{
    proc.syncPatternLength (ctx.pattern());
    if (proc.engine.bank.getLength (ctx.pattern(), ctx.track()) <= kViewSteps) proc.seqPage = 0;
    lane.repaint();
    keys.repaint();
    repaint (grooveCard);
}

void Content::selectTrack (int t)
{
    proc.selectedTrack = t;
    lockChoice.trackChanged (t);
    if (proc.engine.bank.getLength (ctx.pattern(), t) <= kViewSteps) proc.seqPage = 0;
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
    keyDisplay.setBounds (1134, 30, 116, 58);

    // Pistas
    for (int t = 0; t < kTracks; ++t) rows[t]->setBounds (trackCard.getX() + 8, trackCard.getY() + 34 + t * 36, trackCard.getWidth() - 16, 35);

    // Ventana violeta
    auto inner = window.reduced (18, 14);
    for (auto* pg : pages) pg->setBounds (currentPage == PageTrack ? inner : inner);
    for (int k = 1; k < pages.size(); ++k) pages[k]->setBounds (inner.withTrimmedTop (42));
    pages[0]->setBounds (inner);
    backButton.setBounds (window.getRight() - 18 - 116, window.getY() + 14, 116, 32);

    // Columna derecha
    for (int m = 0; m < 4; ++m)
        macros[m]->setBounds (macroCard.getX() + 10 + (m % 2) * 148, macroCard.getY() + 40 + (m / 2) * 86, 146, 80);
    depthButton.setBounds (macroCard.getRight() - 52, macroCard.getY() + 8, 42, 24);
    const int bw = macroCard.getWidth() / 6;
    for (int i = 0; i < 6; ++i) panelButtons[i]->setBounds (macroCard.getX() + i * bw, 360, bw - 3, 32);
    disc.setBounds (masterCard.getX() + 6, masterCard.getY() + 4, 128, 128);
    ceiling->setBounds (masterCard.getX() + 142, masterCard.getY() + 14, 80, 110);
    volume->setBounds (masterCard.getX() + 224, masterCard.getY() + 14, 80, 110);

    // Groove
    swing->setBounds (grooveCard.getX() + 10, grooveCard.getY() + 36, 84, 104);
    lenMinus.setBounds (grooveCard.getX() + 108, grooveCard.getY() + 56, 32, 32);
    lenPlus.setBounds (grooveCard.getX() + 188, grooveCard.getY() + 56, 32, 32);
    randomButton.setBounds (grooveCard.getX() + 108, grooveCard.getY() + 108, 54, 30);
    seedBox.setBounds (grooveCard.getX() + 166, grooveCard.getY() + 108, 54, 28);
    inventButton.setBounds (grooveCard.getX() + 12, grooveCard.getY() + 150, 104, 40);
    inventMenu.setBounds (grooveCard.getX() + 118, grooveCard.getY() + 150, 26, 40);
    mutateButton.setBounds (grooveCard.getX() + 150, grooveCard.getY() + 150, grooveCard.getWidth() - 162, 40);
    const int third = (grooveCard.getWidth() - 24 - 8) / 3;
    clearButton.setBounds (grooveCard.getX() + 12, grooveCard.getY() + 198, third, 32);
    copyButton.setBounds (grooveCard.getX() + 16 + third, grooveCard.getY() + 198, third, 32);
    pasteButton.setBounds (grooveCard.getX() + 20 + 2 * third, grooveCard.getY() + 198, third, 32);

    // Secuenciador: modos a la izquierda (2 columnas), pantalla y teclas a la derecha
    const int sx = seqCard.getX(), sy = seqCard.getY();
    for (int i = 0; i < modeButtons.size(); ++i)
        modeButtons[i]->setBounds (sx + 12 + (i % 2) * 62, sy + 46 + (i / 2) * 31, 60, 29);
    const int gx = sx + 142, gw = seqCard.getRight() - 14 - gx;
    lane.setBounds (gx, sy + 48, gw, 84);
    keys.setBounds (gx, sy + 138, gw, 96);
    lockChoice.setBounds (sx + 300, sy + 12, 200, 28);
    page1.setBounds (sx + 704, sy + 10, 52, 30);
    page2.setBounds (sx + 758, sy + 10, 56, 30);
    undoButton.setBounds (sx + 822, sy + 10, 34, 30);
    redoButton.setBounds (sx + 858, sy + 10, 34, 30);
    euclidButton.setBounds (sx + 898, sy + 10, 64, 30);
    for (int l = 0; l < kLanes; ++l)
    {
        motionLaneButtons[l]->setBounds (sx + 300 + l * 38, sy + 10, 36, 30);
        motionDest[l]->setBounds (sx + 458, sy + 12, 150, 26);
        motionBars[l]->setBounds (sx + 614, sy + 12, 104, 26);
        motionDepth[l]->setBounds (sx + 762, sy + 12, 54, 26);
        motionCanvas[l]->setBounds (gx, sy + 48, gw, 150);
    }
    const int shw = (gw - 4 * 6) / 5;
    for (int sh = 0; sh < 5; ++sh) motionShapes[sh]->setBounds (gx + sh * (shw + 6), sy + 204, shw, 30);
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
    // Versión y build: así se ve de un vistazo qué versión cargó Ableton
#ifndef OGRO2_BUILD
 #define OGRO2_BUILD "local"
#endif
    g.setColour (col::dim.withAlpha (0.8f));
    g.setFont (textFont (10.0f));
    g.drawText ("v" + juce::String (JucePlugin_VersionString) + "  ·  " + juce::String (OGRO2_BUILD),
                juce::Rectangle<float> (plate.getX() + 2.0f, plate.getBottom() + 17.0f, 200.0f, 11.0f), juce::Justification::centredLeft);
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
    g.drawText (T ("al medio = sin cambio"), juce::Rectangle<int> (macroCard.getX() + 110, macroCard.getY() + 10, 140, 18),
                juce::Justification::centredLeft);

    // Título de la página en la ventana violeta (paneles)
    if (currentPage != PageTrack && currentPage < pages.size())
    {
        g.setColour (col::violetText);
        drawSpaced (g, upper (pages[currentPage]->title()), { (float) window.getX() + 18.0f, (float) window.getY() + 14.0f, 400.0f, 32.0f },
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
    drawSpaced (g, "LARGO", { (float) grooveCard.getX() + 108.0f, (float) grooveCard.getY() + 36.0f, 110.0f, 18.0f }, labelFont (12.0f), 0.08f,
                juce::Justification::centredLeft);
    drawSpaced (g, "AZAR", { (float) grooveCard.getX() + 108.0f, (float) grooveCard.getY() + 90.0f, 110.0f, 18.0f }, labelFont (12.0f), 0.08f,
                juce::Justification::centredLeft);
    auto lenR = juce::Rectangle<float> ((float) grooveCard.getX() + 144.0f, (float) grooveCard.getY() + 56.0f, 40.0f, 32.0f);
    paint::lcd (g, lenR, 6.0f);
    paint::glowText (g, juce::String (proc.engine.bank.getLength (ctx.pattern(), t)), lenR, lcdFont (28.0f), juce::Justification::centred);

    // Secuenciador
    g.setColour (col::track (t));
    g.fillRoundedRectangle ((float) seqCard.getX() + 14.0f, (float) seqCard.getY() + 18.0f, 5.0f, 14.0f, 2.5f);
    g.setColour (col::ink);
    drawSpaced (g, T ("SECUENCIA · ") + trackLabel (t), { (float) seqCard.getX() + 26.0f, (float) seqCard.getY() + 14.0f, 270.0f, 22.0f },
                titleFont (12.0f), 0.14f, juce::Justification::centredLeft);
    if (lane.getMode() == LaneMode::Motion)
    {
        g.setColour (col::label);
        drawSpaced (g, "PROF.", { (float) seqCard.getX() + 724.0f, (float) seqCard.getY() + 14.0f, 40.0f, 22.0f }, labelFont (11.0f), 0.06f,
                    juce::Justification::centredLeft);
    }
    else if (lane.getMode() == LaneMode::Lock)
    {
        g.setColour (col::label);
        g.setFont (textFont (12.0f));
        g.drawText (T ("clic derecho: borrar"), juce::Rectangle<int> (seqCard.getX() + 506, seqCard.getY() + 12, 190, 28), juce::Justification::centredLeft);
    }
    // Fill sonando
    if (proc.engine.meters.fill.load())
    {
        g.setColour (col::orange);
        drawSpaced (g, "FILL", { (float) seqCard.getX() + 640.0f, (float) seqCard.getY() + 14.0f, 50.0f, 22.0f }, titleFont (11.0f), 0.1f,
                    juce::Justification::centredLeft);
    }
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
    if (lane.getMode() == LaneMode::Motion)
    {
        motionCanvas[motionLane]->repaint();
        motionDest[motionLane]->refresh();
    }
    else
    {
        const bool longTrack = proc.engine.bank.getLength (ctx.pattern(), ctx.track()) > kViewSteps;
        page2.setEnabled (longTrack);
        if (! longTrack && proc.seqPage.load() != 0) proc.seqPage = 0;
        page1.setToggleState (proc.seqPage.load() == 0, juce::dontSendNotification);
        page2.setToggleState (proc.seqPage.load() == 1, juce::dontSendNotification);
    }
    undoButton.setEnabled (proc.canUndo());
    redoButton.setEnabled (proc.canRedo());
    const bool fillNow = m.fill.load();
    if (fillNow != lastFill) { lastFill = fillNow; repaint (seqCard.withHeight (44)); }
    const auto rev = proc.engine.bank.revision.load();
    if (rev != lastRevision)
    {
        lastRevision = rev;
        lane.repaint();
        repaint (grooveCard);
        stateDirty = true;
    }
    // Patrones editados: avisamos al host (como mucho dos veces por segundo)
    if (stateDirty && blink % 15 == 0) { proc.notifyStateChanged(); stateDirty = false; }
    randomButton.setButtonText (proc.value (P::RandomMode) < 0.5f ? "FIJO" : "VIVO");
    keyDisplay.refresh();
    depthButton.setButtonText (proc.value (P::MacroDepth) > 0.5f ? "x2" : "x1");
    seedBox.repaint();
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
