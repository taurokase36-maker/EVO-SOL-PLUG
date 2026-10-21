#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace ogro2;

static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> out;
    for (const auto& d : paramDefs())
    {
        if (! d.exists) continue;
        const juce::ParameterID pid { d.id, d.version };
        const auto name = juce::String::fromUTF8 (d.name.c_str());
        auto toText = [fn = d.text] (float v, int) { return juce::String::fromUTF8 (fn (v).c_str()); };
        switch (d.kind)
        {
            case Kind::Float:
            {
                juce::NormalisableRange<float> range;
                if (d.log)
                {
                    range = juce::NormalisableRange<float> (
                        d.min, d.max,
                        [] (float a, float b, float n) { return a * std::pow (b / a, n); },
                        [] (float a, float b, float v) { return std::log (juce::jmax (v, a) / a) / std::log (b / a); });
                }
                else
                    range = juce::NormalisableRange<float> (d.min, d.max);
                out.push_back (std::make_unique<juce::AudioParameterFloat> (
                    pid, name, range, d.def, juce::AudioParameterFloatAttributes().withStringFromValueFunction (toText)));
                break;
            }
            case Kind::Choice:
            {
                juce::StringArray choices;
                for (const auto& c : d.choices) choices.add (juce::String::fromUTF8 (c.c_str()));
                out.push_back (std::make_unique<juce::AudioParameterChoice> (pid, name, choices, (int) d.def));
                break;
            }
            case Kind::Int:
                out.push_back (std::make_unique<juce::AudioParameterInt> (
                    pid, name, (int) d.min, (int) d.max, (int) d.def,
                    juce::AudioParameterIntAttributes().withStringFromValueFunction ([fn = d.text] (int v, int) { return juce::String::fromUTF8 (fn ((float) v).c_str()); })));
                break;
            case Kind::Bool:
                out.push_back (std::make_unique<juce::AudioParameterBool> (pid, name, d.def > 0.5f));
                break;
        }
    }
    return { out.begin(), out.end() };
}

juce::AudioProcessor::BusesProperties Ogro2Processor::createBuses()
{
    auto buses = BusesProperties().withOutput ("Principal", juce::AudioChannelSet::stereo(), true);
    static const char* names[] = { "KICK", "CLAP", "RIM", "HAT C", "HAT A", "PERC", "BAJO", "ACORDES", "MELODIA", "TEXTURAS", "EXTRA 1", "EXTRA 2" };
    for (int i = 0; i < Ogro2Processor::kAuxOutputs; ++i)
        buses = buses.withOutput (names[i], juce::AudioChannelSet::stereo(), false);
    return buses;
}

Ogro2Processor::Ogro2Processor()
    : AudioProcessor (createBuses()),
      state (*this, nullptr, "PARAMS", createLayout())
{
    const auto& defs = paramDefs();
    params.assign (defs.size(), nullptr);
    raw.assign (defs.size(), nullptr);
    values.resize (defs.size());
    for (size_t i = 0; i < defs.size(); ++i)
    {
        values[i] = defs[i].def;
        if (! defs[i].exists) continue;
        params[i] = state.getParameter (defs[i].id);
        raw[i] = state.getRawParameterValue (defs[i].id);
        jassert (params[i] != nullptr && raw[i] != nullptr);
    }
    midiTriggers.reserve (512);
    // Una instancia nueva arranca con el preset "Inicial" completo (valores, ruteo y patrones)
    const auto st = factoryState (factoryPresets()[0]);
    applyValues (st.values);
    applyRouting (engine.routing, st);
    factoryPresets()[0].build (engine.bank);
    engine.prepare (44100.0, 512);
    setLatencySamples (engine.getLatencySamples());

    // Ajustes globales (carpeta de samples) y pool de samples compartido
    juce::PropertiesFile::Options opt;
    opt.applicationName = "OGRO 2";
    opt.filenameSuffix = ".settings";
    opt.folderName = "OGRO 2";
    opt.osxLibrarySubFolder = "Application Support";
    settings = std::make_unique<juce::PropertiesFile> (opt);
    const auto folder = getSampleFolder();
    if (SamplePool::get().currentFolder() != folder.getFullPathName().toStdString())
        SamplePool::get().scan (folder.getFullPathName().toStdString());
    startTimer (300);
}

// ---------------------------------------------------------------------------
// Samples
// ---------------------------------------------------------------------------
juce::File Ogro2Processor::defaultSampleFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("OGRO Samples").getChildFile ("Orbhitz");
}

juce::File Ogro2Processor::orbhitzSource()
{
    const auto d = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Downloads/Orbhits-MAC-WIN/Orbhits.instruments");
    return d.isDirectory() ? d : juce::File();
}

juce::File Ogro2Processor::getSampleFolder() const
{
    // Tests y capturas: carpeta de prueba (no se toca la librería del usuario)
    if (const char* env = std::getenv ("OGRO2_SAMPLES")) return juce::File (juce::String::fromUTF8 (env));
    const auto saved = settings != nullptr ? settings->getValue ("carpetaSamples") : juce::String();
    if (saved.isNotEmpty() && juce::File (saved).isDirectory()) return juce::File (saved);
    if (defaultSampleFolder().isDirectory()) return defaultSampleFolder();
    // Si la librería todavía está en Descargas, la usamos ahí mismo (se puede copiar desde el menú)
    if (orbhitzSource().isDirectory()) return orbhitzSource();
    return defaultSampleFolder();
}

void Ogro2Processor::setSampleFolder (const juce::File& dir)
{
    if (settings != nullptr) { settings->setValue ("carpetaSamples", dir.getFullPathName()); settings->saveIfNeeded(); }
    SamplePool::get().scan (dir.getFullPathName().toStdString());
    lastPoolSize = -1;
    refreshSamples();
}

void Ogro2Processor::copyOrbhitz (std::function<void (bool, juce::String)> done)
{
    const auto src = orbhitzSource();
    if (! src.isDirectory() || copying.exchange (true)) return;
    const auto dst = defaultSampleFolder();
    juce::Thread::launch ([this, src, dst, done, alive = alive] {
        bool ok = dst.createDirectory().wasOk();
        int copied = 0;
        for (const auto& f : src.findChildFiles (juce::File::findFiles, false, "*.mse"))
        {
            if (categoryOf (f.getFileName().toStdString()) < 0) continue;   // solo CHD, PAD, HIT y SFX
            const auto target = dst.getChildFile (f.getFileName());
            if (! target.existsAsFile()) ok &= f.copyFileTo (target);
            ++copied;
        }
        const auto manifest = src.getChildFile ("ogro2_samples.json");
        if (manifest.existsAsFile() && ! dst.getChildFile ("ogro2_samples.json").existsAsFile()) manifest.copyFileTo (dst.getChildFile ("ogro2_samples.json"));
        juce::MessageManager::callAsync ([this, ok, copied, dst, done, alive] {
            if (! alive->load()) return;   // el plugin se cerró mientras copiaba
            copying = false;
            if (ok) setSampleFolder (dst);
            if (done) done (ok, ok ? juce::String (copied) + " samples copiados a " + dst.getFullPathName() : juce::String ("No se pudo copiar a ") + dst.getFullPathName());
        });
    });
}

void Ogro2Processor::setSampleName (int slot, const juce::String& name)
{
    slot = juce::jlimit (0, 1, slot);
    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        sampleNames[slot] = name;
    }
    engine.setSelectedSample (slot, name.isEmpty() ? -1 : SamplePool::get().find (name.toStdString()));
    notifyStateChanged();
}

juce::String Ogro2Processor::getSampleName (int slot) const
{
    const juce::SpinLock::ScopedLockType lock (sampleLock);
    return sampleNames[juce::jlimit (0, 1, slot)];
}

juce::String Ogro2Processor::sampleStatus (int slot) const
{
    const auto name = getSampleName (slot);
    auto& pool = SamplePool::get();
    if (pool.samplesInFolder() == 0)
        return juce::String::fromUTF8 ("No hay samples en la carpeta: menú ≡ > Carpeta de samples");
    if (name.isEmpty()) return {};
    const int i = pool.find (name.toStdString());
    if (i < 0) return juce::String::fromUTF8 ("Falta «") + name + juce::String::fromUTF8 ("» en la carpeta de samples");
    const auto* e = pool.entry (i);
    if (e->failed.load()) return name + ": " + juce::String::fromUTF8 (e->error.c_str());
    if (! e->ready.load()) return juce::String::fromUTF8 ("Cargando ") + name + "...";
    return {};
}

void Ogro2Processor::rollSampleDice (int slot)
{
    auto& pool = SamplePool::get();
    const int cat = slot == 0 ? (int) value (P::TexCat) - 1 : CatChd;
    std::vector<int> pick;
    for (int i = 0; i < pool.size(); ++i)
        if (const auto* e = pool.entry (i))
            if ((cat < 0 || e->category == cat) && e->ready.load()) pick.push_back (i);
    if (pick.empty()) return;
    const int i = pick[(size_t) juce::Random::getSystemRandom().nextInt ((int) pick.size())];
    setSampleName (slot, juce::String::fromUTF8 (pool.entry (i)->name.c_str()));
}

// Hilo de mensajes: los nombres se resuelven a índices del pool cuando el pool crece, y los
// candidatos del "azar por paso" siguen el filtro de categoría
void Ogro2Processor::refreshSamples()
{
    auto& pool = SamplePool::get();
    for (int slot = 0; slot < 2; ++slot)
    {
        const auto name = getSampleName (slot);
        engine.setSelectedSample (slot, name.isEmpty() ? -1 : pool.find (name.toStdString()));
    }
    const int cat = (int) value (P::TexCat) - 1;
    std::vector<int> tex, chd;
    for (int i = 0; i < pool.size(); ++i)
        if (const auto* e = pool.entry (i))
        {
            if (e->path.rfind (pool.currentFolder(), 0) != 0) continue;   // solo los de la carpeta actual
            if (cat < 0 || e->category == cat) tex.push_back (i);
            if (e->category == CatChd) chd.push_back (i);
        }
    engine.setCandidates (Engine::SlotTexture, tex);
    engine.setCandidates (Engine::SlotChord, chd);
    lastPoolSize = pool.size();
    lastCat = cat;
}

void Ogro2Processor::timerCallback()
{
    if (SamplePool::get().size() != lastPoolSize || (int) value (P::TexCat) - 1 != lastCat) refreshSamples();
}

bool Ogro2Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (! layouts.inputBuses.isEmpty()) return false;
    const auto main = layouts.getMainOutputChannelSet();
    if (main != juce::AudioChannelSet::stereo() && main != juce::AudioChannelSet::mono()) return false;
    for (int b = 1; b < layouts.outputBuses.size(); ++b)
    {
        const auto& set = layouts.outputBuses.getReference (b);
        if (! set.isDisabled() && set != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

void Ogro2Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, juce::jmax (32, samplesPerBlock));
    setLatencySamples (engine.getLatencySamples());
}

void Ogro2Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0) return;
    engine.setOffline (isNonRealtime());   // calidad AUTO: ALTA al congelar o exportar

    for (size_t i = 0; i < raw.size(); ++i) if (raw[i] != nullptr) values[i] = raw[i]->load (std::memory_order_relaxed);
    engine.setParams (values.data(), (int) values.size());

    HostInfo host;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            host.valid = true;
            host.playing = pos->getIsPlaying();
            if (auto bpm = pos->getBpm()) host.bpm = *bpm;
            else host.bpm = values[P::Tempo];
            if (auto ppq = pos->getPpqPosition()) host.ppq = *ppq;
            else host.playing = false;
            host.looping = pos->getIsLooping();
            if (auto loop = pos->getLoopPoints())
            {
                host.loopStart = loop->ppqStart;
                host.loopEnd = loop->ppqEnd;
            }
            else host.looping = false;
        }

    // MIDI: C1..G1 (36..43) disparan las 8 pistas de la 1.0; G#1 = MELODÍA, A1 = TEXTURAS.
    // El note-off cierra el gate de bajo, acordes, melodía y texturas.
    midiTriggers.clear();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int note = m.getNoteNumber();
        if (note < 36 || note >= 36 + kTracks) continue;
        if (m.isNoteOn()) midiTriggers.push_back ({ meta.samplePosition, note - 36, m.getFloatVelocity(), false });
        else if (m.isNoteOff()) midiTriggers.push_back ({ meta.samplePosition, note - 36, 0.0f, true });
    }
    midi.clear();

    // Salidas separadas: por ahora en silencio (llegan en la fase 7)
    for (int b = 1; b < getBusCount (false); ++b)
    {
        auto bus = getBusBuffer (buffer, false, b);
        bus.clear();
    }

    auto mainBus = getBusBuffer (buffer, false, 0);
    auto* l = mainBus.getWritePointer (0);
    auto* r = mainBus.getNumChannels() > 1 ? mainBus.getWritePointer (1) : nullptr;
    if (r != nullptr)
        engine.process (host, midiTriggers.data(), (int) midiTriggers.size(), l, r, n);
    else
    {
        // Salida mono: renderizamos en estéreo en tramos y sumamos
        float right[512];
        for (int start = 0; start < n; start += 512)
        {
            const int len = juce::jmin (512, n - start);
            engine.process (host, midiTriggers.data(), (int) midiTriggers.size(), l + start, right, len);
            for (int i = 0; i < len; ++i) l[start + i] = 0.5f * (l[start + i] + right[i]);
            if (host.playing) host.ppq += (double) len * host.bpm / 60.0 / getSampleRate();
            for (auto& t : midiTriggers) t.sample -= len;
        }
    }
}

void Ogro2Processor::setValue (int index, float physical)
{
    if (auto* p = params[(size_t) index])
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (physical));
        p->endChangeGesture();
    }
}

// Aplica muchos valores de una vez (preset): sin gestos, como cuando Ableton abre la
// sesión. Así no se llena el historial de deshacer ni se escribe automatización.
void Ogro2Processor::applyValues (const std::vector<float>& v)
{
    for (size_t i = 0; i < params.size() && i < v.size(); ++i)
        if (auto* p = params[i])
        {
            const float norm = p->convertTo0to1 (v[i]);
            if (std::abs (p->getValue() - norm) > 1.0e-7f) p->setValueNotifyingHost (norm);
        }
}

std::string Ogro2Processor::bankText() const
{
    const auto& defs = paramDefs();
    return engine.bank.serialise ([&defs] (int p) { return p >= 0 && p < (int) defs.size() && defs[(size_t) p].exists ? defs[(size_t) p].id : std::string(); });
}

void Ogro2Processor::pushUndo()
{
    auto now = bankText();
    if (! undoStack.empty() && undoStack.back() == now) return;
    undoStack.push_back (std::move (now));
    if (undoStack.size() > 64) undoStack.erase (undoStack.begin());
    redoStack.clear();
}

bool Ogro2Processor::undo()
{
    auto now = bankText();
    // Lo último guardado puede ser igual a lo actual (no se editó nada después)
    while (! undoStack.empty() && undoStack.back() == now) undoStack.pop_back();
    if (undoStack.empty()) return false;
    redoStack.push_back (std::move (now));
    engine.bank.deserialise (undoStack.back(), [] (const std::string& id) { return paramIndex (id); });
    undoStack.pop_back();
    notifyStateChanged();
    return true;
}

bool Ogro2Processor::redo()
{
    if (redoStack.empty()) return false;
    undoStack.push_back (bankText());
    engine.bank.deserialise (redoStack.back(), [] (const std::string& id) { return paramIndex (id); });
    redoStack.pop_back();
    notifyStateChanged();
    return true;
}

void Ogro2Processor::syncPatternLength (int pattern)
{
    int longest = 1;
    for (int t = 0; t < kTracks; ++t) longest = juce::jmax (longest, engine.bank.getLength (pattern, t));
    // Los polirritmos cortos no achican el patrón por debajo de un compás
    engine.bank.setPatternLength (pattern, juce::jmax (16, longest));
}

// ---------------------------------------------------------------------------
// Escenas A/B y dado de sonido
// ---------------------------------------------------------------------------
void Ogro2Processor::storeScene (int which)
{
    std::vector<float> v (paramDefs().size());
    for (size_t i = 0; i < v.size(); ++i) v[i] = value ((int) i);
    engine.setSnapshot (which, v);
    notifyStateChanged();
}

void Ogro2Processor::recallScene (int which)
{
    if (! engine.hasSnapshot (which)) return;
    for (int i = 0; i < (int) paramDefs().size(); ++i)
        if (Engine::isScenic (i)) setValue (i, engine.snapshotValue (which, i));
}

void Ogro2Processor::toggleLock (int track, int knob)
{
    track = juce::jlimit (0, kTracks - 1, track);
    soundLocks[track].fetch_xor (1u << juce::jlimit (0, 31, knob));
    notifyStateChanged();
}

// Perillas que mueve el dado: las 8 de sonido (salvo listas y escalones) y las de la tira
static int diceParam (int track, int knob)
{
    if (knob < kSoundParams)
    {
        const int p = P::sound (track, knob);
        if (paramDefs()[(size_t) p].kind != Kind::Float) return -1;           // tipo de acorde, modelo
        if (knob == 0 && track == Bass) return -1;                            // octava del bajo
        return p;
    }
    static const int strip[] = { P::StripDrive, P::StripBody, P::StripBodyHz, P::StripNoise, P::StripTrans, P::StripBright, P::StripComp };
    const int k = knob - kSoundParams;
    return k >= 0 && k < 7 ? P::track (track, strip[k]) : -1;
}

void Ogro2Processor::rollSoundDice (int track, float amount)
{
    track = juce::jlimit (0, kTracks - 1, track);
    auto& rnd = juce::Random::getSystemRandom();
    const auto& defs = paramDefs();
    for (int knob = 0; knob < kSoundParams + 7; ++knob)
    {
        if (isLocked (track, knob)) continue;
        const int p = diceParam (track, knob);
        if (p < 0) continue;
        const auto& d = defs[(size_t) p];
        const float cur = d.toNorm (value (p));
        // La tira se mueve menos (y el ruido casi nada): que el dado no ensucie de más
        float span = amount;
        if (knob >= kSoundParams) span *= knob == kSoundParams + 3 ? 0.15f : 0.5f;
        float target = juce::jlimit (0.0f, 1.0f, cur + (rnd.nextFloat() * 2.0f - 1.0f) * span);
        if (knob >= kSoundParams && knob != kSoundParams + 2 && knob != kSoundParams + 4) target = juce::jmin (target, 0.7f);
        setValue (p, d.fromNorm (target));
    }
}

void Ogro2Processor::notifyStateChanged()
{
    updateHostDisplay (juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged (true));
}

void Ogro2Processor::setModSource (int slot, int src)
{
    if (slot < 0 || slot >= P::kSlots) return;
    engine.routing.src[slot].store (juce::jlimit (0, SrcCount - 1, src));
    notifyStateChanged();
}
void Ogro2Processor::setModDest (int slot, int p)
{
    if (slot < 0 || slot >= P::kSlots) return;
    engine.routing.dst[slot].store (p);
    notifyStateChanged();
}
void Ogro2Processor::setLaneDest (int lane, int p)
{
    if (lane < 0 || lane >= P::kMotion) return;
    engine.routing.laneDst[lane].store (p);
    notifyStateChanged();
}

juce::String Ogro2Processor::getUserPresetName() const
{
    const juce::SpinLock::ScopedLockType lock (nameLock);
    return userPresetName;
}

void Ogro2Processor::setUserPresetName (const juce::String& n)
{
    const juce::SpinLock::ScopedLockType lock (nameLock);
    userPresetName = n;
}

juce::File Ogro2Processor::presetFolder()
{
    auto d = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("OGRO 2 Presets");
    d.createDirectory();
    return d;
}

void Ogro2Processor::loadPreset (int index)
{
    const auto& list = factoryPresets();
    if (index < 0 || index >= (int) list.size()) return;
    const auto st = factoryState (list[(size_t) index]);
    applyValues (st.values);
    applyRouting (engine.routing, st);
    list[(size_t) index].build (engine.bank);
    currentPreset = index;
    setUserPresetName ({});
    notifyStateChanged();
}

// ---------------------------------------------------------------------------
// Estado: <OGRO2 stateVersion=...> con PARAMS (por ID), PATTERNS y ROUTING (por ID)
// ---------------------------------------------------------------------------
std::unique_ptr<juce::XmlElement> Ogro2Processor::createStateXml()
{
    auto root = std::make_unique<juce::XmlElement> ("OGRO2");
    root->setAttribute ("stateVersion", kStateVersion);
    root->setAttribute ("pluginVersion", JucePlugin_VersionString);
    root->setAttribute ("preset", currentPreset.load());
    root->setAttribute ("userPreset", getUserPresetName());
    if (auto params = state.copyState().createXml()) root->addChildElement (params.release());

    const auto& defs = paramDefs();
    auto idOf = [&defs] (int p) { return p >= 0 && p < (int) defs.size() && defs[(size_t) p].exists ? defs[(size_t) p].id : std::string(); };
    auto* pat = root->createNewChildElement ("PATTERNS");
    pat->addTextElement (juce::String (engine.bank.serialise (idOf)));
    auto* routing = root->createNewChildElement ("ROUTING");
    for (int s = 0; s < P::kSlots; ++s)
    {
        auto* e = routing->createNewChildElement ("MOD");
        e->setAttribute ("slot", s);
        e->setAttribute ("src", modSource (engine.routing.src[s].load()).id);
        e->setAttribute ("dst", juce::String (idOf (engine.routing.dst[s].load())));
    }
    for (int l = 0; l < P::kMotion; ++l)
    {
        auto* e = routing->createNewChildElement ("MOTION");
        e->setAttribute ("lane", l);
        e->setAttribute ("dst", juce::String (idOf (engine.routing.laneDst[l].load())));
    }
    // Escenas A/B por ID y candados del dado
    for (int w = 0; w < 2; ++w)
        if (engine.hasSnapshot (w))
        {
            auto* sc = root->createNewChildElement ("SCENE");
            sc->setAttribute ("id", w == 0 ? "A" : "B");
            for (int i = 0; i < (int) defs.size(); ++i)
                if (Engine::isScenic (i))
                {
                    auto* e = sc->createNewChildElement ("P");
                    e->setAttribute ("id", juce::String (defs[(size_t) i].id));
                    e->setAttribute ("v", engine.snapshotValue (w, i));
                }
        }
    {
        juce::String locks;
        for (int t = 0; t < kTracks; ++t) locks << (t ? "," : "") << juce::String ((int) soundLocks[t].load());
        root->createNewChildElement ("LOCKS")->setAttribute ("v", locks);
    }
    // Samples por nombre (si en otra Mac falta alguno, la sesión abre igual y avisa)
    auto* smp = root->createNewChildElement ("SAMPLES");
    smp->setAttribute ("texture", getSampleName (0));
    smp->setAttribute ("chord", getSampleName (1));
    return root;
}

void Ogro2Processor::restoreFromXml (const juce::XmlElement& xml)
{
    // Presets y sesiones de OGRO 1.0 (o de la fase 0 de OGRO 2, que usaba el mismo formato)
    if (xml.hasTagName ("OGRO") || (xml.hasTagName ("OGRO2") && ! xml.hasAttribute ("stateVersion")))
    {
        applyV1 (xml, xml.getStringAttribute ("userPreset"));
        return;
    }
    if (! xml.hasTagName ("OGRO2")) return;
    // Migraciones: un estado guardado con una versión anterior no tiene los parámetros nuevos;
    // esos toman el valor que reproduce cómo sonaba (MODO TONAL apagado, acordes en paralelo...)
    const int savedVersion = xml.getIntAttribute ("stateVersion", 1);
    currentPreset = xml.getIntAttribute ("preset", 0);
    setUserPresetName (xml.getStringAttribute ("userPreset"));

    if (auto* p = xml.getChildByName (state.state.getType()))
    {
        // Los parámetros que no estén en el estado (agregados en una versión más nueva) quedan en su valor por defecto
        const auto& defs = paramDefs();
        std::vector<float> v (defs.size());
        for (size_t i = 0; i < defs.size(); ++i)
            v[i] = savedVersion < kStateVersion && defs[i].hasCompat() ? defs[i].compat : defs[i].def;
        for (auto* child : p->getChildIterator())
        {
            const int idx = paramIndex (child->getStringAttribute ("id").toStdString());
            if (idx >= 0) v[(size_t) idx] = (float) child->getDoubleAttribute ("value", defs[(size_t) idx].def);
        }
        applyValues (v);
    }
    if (auto* pat = xml.getChildByName ("PATTERNS"))
        engine.bank.deserialise (pat->getAllSubText().toStdString(), [] (const std::string& id) { return paramIndex (id); });
    engine.clearSnapshot (0);
    engine.clearSnapshot (1);
    for (auto* sc : xml.getChildWithTagNameIterator ("SCENE"))
    {
        const int w = sc->getStringAttribute ("id") == "B" ? 1 : 0;
        std::vector<float> v (paramDefs().size());
        for (size_t i = 0; i < v.size(); ++i) v[i] = value ((int) i);
        for (auto* e : sc->getChildWithTagNameIterator ("P"))
        {
            const int idx = paramIndex (e->getStringAttribute ("id").toStdString());
            if (idx >= 0) v[(size_t) idx] = (float) e->getDoubleAttribute ("v");
        }
        engine.setSnapshot (w, v);
    }
    for (auto& l : soundLocks) l.store (0);
    if (auto* lk = xml.getChildByName ("LOCKS"))
    {
        const auto parts = juce::StringArray::fromTokens (lk->getStringAttribute ("v"), ",", "");
        for (int t = 0; t < kTracks && t < parts.size(); ++t) soundLocks[t].store ((uint32_t) parts[t].getIntValue());
    }
    if (auto* smp = xml.getChildByName ("SAMPLES"))
    {
        setSampleName (0, smp->getStringAttribute ("texture"));
        setSampleName (1, smp->getStringAttribute ("chord"));
    }
    else { setSampleName (0, {}); setSampleName (1, {}); }
    engine.routing.clear();
    if (auto* routing = xml.getChildByName ("ROUTING"))
    {
        for (auto* e : routing->getChildWithTagNameIterator ("MOD"))
        {
            const int s = e->getIntAttribute ("slot", -1);
            if (s < 0 || s >= P::kSlots) continue;
            engine.routing.src[s].store (modSourceIndex (e->getStringAttribute ("src").toStdString()));
            engine.routing.dst[s].store (paramIndex (e->getStringAttribute ("dst").toStdString()));
        }
        for (auto* e : routing->getChildWithTagNameIterator ("MOTION"))
        {
            const int l = e->getIntAttribute ("lane", -1);
            if (l >= 0 && l < P::kMotion) engine.routing.laneDst[l].store (paramIndex (e->getStringAttribute ("dst").toStdString()));
        }
    }
}

// Formato de la 1.0: <OGRO> con <PARAM id=... value=...> y el atributo "patterns"
void Ogro2Processor::applyV1 (const juce::XmlElement& xml, const juce::String& name)
{
    std::vector<std::pair<std::string, float>> old;
    for (auto* child : xml.getChildWithTagNameIterator ("PARAM"))
        old.push_back ({ child->getStringAttribute ("id").toStdString(), (float) child->getDoubleAttribute ("value") });
    const auto st = v1::convert (old);
    applyValues (st.values);
    applyRouting (engine.routing, st);
    const auto pat = xml.getStringAttribute ("patterns");
    if (pat.isNotEmpty()) engine.bank.deserialiseV1 (pat.toStdString());
    currentPreset = xml.getIntAttribute ("preset", 0);
    setUserPresetName (name);
    notifyStateChanged();
}

void Ogro2Processor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createStateXml())
        copyXmlToBinary (*xml, destData);
}

void Ogro2Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        restoreFromXml (*xml);
}

bool Ogro2Processor::saveUserPreset (const juce::File& file)
{
    auto xml = createStateXml();
    if (! xml) return false;
    xml->setAttribute ("userPreset", file.getFileNameWithoutExtension());
    if (! xml->writeTo (file)) return false;
    setUserPresetName (file.getFileNameWithoutExtension());
    return true;
}

bool Ogro2Processor::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (! xml) return false;
    restoreFromXml (*xml);
    setUserPresetName (file.getFileNameWithoutExtension());
    notifyStateChanged();
    return true;
}

bool Ogro2Processor::importV1Preset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (! xml || ! xml->hasTagName ("OGRO")) return false;
    applyV1 (*xml, file.getFileNameWithoutExtension() + " (1.0)");
    return true;
}

juce::AudioProcessorEditor* Ogro2Processor::createEditor() { return new Ogro2Editor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new Ogro2Processor(); }
