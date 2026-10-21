// OGRO 2 — tests del plugin con JUCE (sin host): carga de presets sin gestos, estado
// guardado y recuperado, importar presets de OGRO 1.0, sesiones de la fase 0 y salidas.
#include "PluginProcessor.h"
#include "../../../ogro/Source/engine/Presets.h"

using namespace ogro2;

namespace
{
bool report (const char* name, bool ok, const juce::String& detail)
{
    std::printf ("[%s] %s  %s\n", name, ok ? "OK   " : "FALLA", detail.toRawUTF8());
    return ok;
}

// Cuenta lo que el plugin le avisa al host
struct HostSpy : juce::AudioProcessorListener
{
    int changes = 0, gestures = 0, other = 0;
    void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override { ++changes; }
    void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int) override { ++gestures; }
    void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override { ++other; }
};

// Preset de fábrica de la 1.0 escrito como lo guarda OGRO 1.0 (.ogro)
std::unique_ptr<juce::XmlElement> v1PresetXml (int index, const juce::String& tag = "OGRO")
{
    const auto& p = ogro::factoryPresets()[(size_t) index];
    const auto& defs = ogro::paramDefs();
    std::vector<float> v (defs.size());
    for (size_t i = 0; i < defs.size(); ++i) v[i] = defs[i].def;
    for (const auto& [id, val] : p.values) v[(size_t) ogro::paramIndex (id)] = val;
    auto xml = std::make_unique<juce::XmlElement> (tag);
    for (size_t i = 0; i < defs.size(); ++i)
    {
        auto* e = xml->createNewChildElement ("PARAM");
        e->setAttribute ("id", juce::String (defs[i].id));
        e->setAttribute ("value", (double) v[i]);
    }
    ogro::PatternBank bank;
    p.build (bank);
    xml->setAttribute ("preset", index);
    xml->setAttribute ("userPreset", "");
    xml->setAttribute ("patterns", juce::String (bank.serialise()));
    return xml;
}

bool sameAsFactoryV1 (Ogro2Processor& proc, int index, juce::String& why)
{
    const auto st = factoryState (factoryPresets()[(size_t) index], false);
    const auto& defs = paramDefs();
    for (size_t i = 0; i < defs.size(); ++i)
    {
        if (! defs[i].exists) continue;
        const float got = proc.value ((int) i);
        if (std::abs (got - st.values[i]) > 1.0e-4f * std::max (1.0f, std::abs (st.values[i])))
        {
            why = juce::String (defs[i].id) + " = " + juce::String (got) + " (esperado " + juce::String (st.values[i]) + ")";
            return false;
        }
    }
    for (int s = 0; s < P::kSlots; ++s)
        if (proc.engine.routing.src[s].load() != st.src[s] || proc.engine.routing.dst[s].load() != st.dst[s]) { why = "mod " + juce::String (s + 1); return false; }
    PatternBank ref;
    factoryPresets()[(size_t) index].build (ref);
    for (int p = 0; p < kPatterns; ++p)
        for (int t = 0; t < kTracks; ++t)
            for (int s = 0; s < kSteps; ++s)
                if (ref.get (p, t, s).pack() != proc.engine.bank.get (p, t, s).pack()) { why = "patron " + juce::String (p) + " pista " + juce::String (t); return false; }
    return true;
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    bool all = true;

    // ---- 1) Cargar un preset no manda gestos al host (no llena deshacer ni automatización) ----
    {
        Ogro2Processor proc;
        HostSpy spy;
        proc.addListener (&spy);
        proc.loadPreset (3);
        proc.removeListener (&spy);
        all &= report ("preset sin gestos", spy.gestures == 0 && spy.changes > 0,
                       juce::String (spy.changes) + " valores avisados, " + juce::String (spy.gestures) + " gestos");
    }

    // ---- 2) Estado: guardar y recuperar todo (valores, ruteo, patrones, p-locks, nombre) ----
    {
        Ogro2Processor a;
        a.loadPreset (5);
        a.setValue (P::sound (Bass, 2), 0.71f);
        a.setValue (P::track (Chord, P::EnvS), 0.4f);
        a.setValue (P::Seed, 1234.0f);
        a.setModSource (7, SrcLfo3);
        a.setModDest (7, P::sound (Chord, 2));
        a.setLaneDest (2, P::MFilter);
        Step s; s.on = true; s.vel = 99; s.nudge = 20;
        a.engine.bank.set (6, Perc, 30, s);
        a.engine.bank.setLock (6, Perc, 30, P::sound (Perc, 0), 0.33f);
        juce::MemoryBlock mb;
        a.getStateInformation (mb);
        Ogro2Processor b;
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        bool same = true;
        juce::String why;
        const auto& defs = paramDefs();
        for (size_t i = 0; i < defs.size(); ++i)
            if (defs[i].exists && std::abs (a.value ((int) i) - b.value ((int) i)) > 1.0e-5f) { same = false; why = defs[i].id; break; }
        for (int k = 0; k < P::kSlots; ++k)
            same &= a.engine.routing.src[k].load() == b.engine.routing.src[k].load() && a.engine.routing.dst[k].load() == b.engine.routing.dst[k].load();
        for (int l = 0; l < P::kMotion; ++l) same &= a.engine.routing.laneDst[l].load() == b.engine.routing.laneDst[l].load();
        same &= b.engine.bank.get (6, Perc, 30).pack() == s.pack();
        same &= std::abs (b.engine.bank.lockValue (6, Perc, 30, P::sound (Perc, 0)) - 0.33f) < 1.0e-3f;
        all &= report ("estado", same, "valores, ruteo MOD/MOTION, patrones y p-locks (" + juce::String ((int) mb.getSize()) + " bytes)" + why);
    }

    // ---- 3) Estado con parámetros desconocidos (de una versión futura) y faltantes ----
    {
        Ogro2Processor a;
        auto xml = std::make_unique<juce::XmlElement> ("OGRO2");
        xml->setAttribute ("stateVersion", 1);
        auto* params = xml->createNewChildElement ("PARAMS");
        auto* p1 = params->createNewChildElement ("PARAM");
        p1->setAttribute ("id", "bass.cutoff"); p1->setAttribute ("value", 0.9);
        auto* p2 = params->createNewChildElement ("PARAM");
        p2->setAttribute ("id", "param.del.futuro"); p2->setAttribute ("value", 0.5);
        juce::MemoryBlock mb;
        juce::AudioProcessor::copyXmlToBinary (*xml, mb);
        a.setStateInformation (mb.getData(), (int) mb.getSize());
        const bool ok = std::abs (a.value (P::sound (Bass, 2)) - 0.9f) < 1e-5f
                        && std::abs (a.value (P::sound (Kick, 0)) - paramDefs()[(size_t) P::sound (Kick, 0)].def) < 1e-5f;
        all &= report ("estado futuro", ok, "toma lo que conoce, ignora lo nuevo y lo que falta queda por defecto");
        // Migración: un estado de la versión 1 (fases 1 y 2) no tiene MODO TONAL ni modo de acordes:
        // quedan como sonaba (apagado, paralelo, acento clásico, polirritmos continuos)
        const bool migrated = a.value (P::Tonal) == 0.0f && a.value (P::ChordMode) == 1.0f && a.value (P::BassAccent) == 0.0f
                              && a.value (P::PolyMode) == 1.0f && a.value (P::ChordLead) == 0.0f;
        all &= report ("migracion", migrated, "estado v1: modo tonal apagado, acordes en paralelo, acento clasico, polirritmos continuos");
        // Un estado nuevo (v2) sin esos parámetros usa los valores por defecto de OGRO 2
        Ogro2Processor c;
        xml->setAttribute ("stateVersion", Ogro2Processor::kStateVersion);
        juce::MemoryBlock mb2;
        juce::AudioProcessor::copyXmlToBinary (*xml, mb2);
        c.setStateInformation (mb2.getData(), (int) mb2.getSize());
        all &= report ("migracion", c.value (P::Tonal) == 1.0f && c.value (P::ChordMode) == 0.0f, "estado v2: valores por defecto de OGRO 2");
    }

    // ---- Samples por nombre: viajan en el estado y si faltan la sesión abre igual y avisa ----
    {
        Ogro2Processor a;
        a.setSampleName (0, "HIT-QueNoEsta");
        a.setSampleName (1, "CHD-TampocoEsta");
        juce::MemoryBlock mb;
        a.getStateInformation (mb);
        Ogro2Processor b;
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        const bool ok = b.getSampleName (0) == "HIT-QueNoEsta" && b.getSampleName (1) == "CHD-TampocoEsta" && b.sampleStatus (0).isNotEmpty();
        all &= report ("samples", ok, "nombres guardados en el estado; aviso: " + b.sampleStatus (0));
    }

    // ---- 4) Importar un preset .ogro de OGRO 1.0: suena igual que la 1.0 ----
    {
        bool ok = true;
        juce::String why;
        for (int i = 0; i < 12 && ok; ++i)
        {
            Ogro2Processor proc;
            const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("ogro2_test_v1.ogro");
            v1PresetXml (i)->writeTo (file);
            ok &= proc.importV1Preset (file);
            ok &= sameAsFactoryV1 (proc, i, why);
            ok &= proc.getUserPresetName().endsWith ("(1.0)");
            file.deleteFile();
            if (! ok) why = "preset " + juce::String (i) + ": " + why;
        }
        all &= report ("importar 1.0", ok, "los 12 presets de la 1.0 (valores, MOD, patrones) " + why);
    }

    // ---- 5) Sesión guardada con la fase 0 de OGRO 2 (formato de la 1.0 con etiqueta OGRO2) ----
    {
        Ogro2Processor proc;
        juce::MemoryBlock mb;
        juce::AudioProcessor::copyXmlToBinary (*v1PresetXml (7, "OGRO2"), mb);
        proc.setStateInformation (mb.getData(), (int) mb.getSize());
        juce::String why;
        all &= report ("sesion fase 0", sameAsFactoryV1 (proc, 7, why), "se lee como un preset de la 1.0 " + why);
    }

    // ---- 6) Salidas: principal estéreo o mono, separadas estéreo o apagadas ----
    {
        Ogro2Processor proc;
        auto layout = proc.getBusesLayout();
        bool ok = proc.checkBusesLayoutSupported (layout);
        ok &= layout.outputBuses.size() == 1 + Ogro2Processor::kAuxOutputs;
        for (int b = 1; b < layout.outputBuses.size(); ++b) layout.outputBuses.getReference (b) = juce::AudioChannelSet::stereo();
        ok &= proc.checkBusesLayoutSupported (layout);
        layout.outputBuses.getReference (0) = juce::AudioChannelSet::mono();
        ok &= proc.checkBusesLayoutSupported (layout);
        layout.outputBuses.getReference (1) = juce::AudioChannelSet::create5point1();
        ok &= ! proc.checkBusesLayoutSupported (layout);
        all &= report ("salidas", ok, "principal + " + juce::String (Ogro2Processor::kAuxOutputs) + " salidas separadas");
    }

    // ---- 7) Procesar con todas las salidas activas y buffers raros ----
    {
        Ogro2Processor proc;
        auto layout = proc.getBusesLayout();
        for (int b = 1; b < layout.outputBuses.size(); ++b) layout.outputBuses.getReference (b) = juce::AudioChannelSet::stereo();
        proc.setBusesLayout (layout);
        proc.setPlayConfigDetails (0, proc.getTotalNumOutputChannels(), 48000.0, 2048);
        proc.prepareToPlay (48000.0, 2048);
        proc.engine.setInternalPlay (true);
        bool finite = true;
        float peak = 0;
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 42, 0.9f), 10);
        midi.addEvent (juce::MidiMessage::noteOff (1, 42), 300);
        for (int n : { 1, 7, 32, 333, 2048, 64 })
        {
            juce::AudioBuffer<float> buf (proc.getTotalNumOutputChannels(), n);
            buf.clear();
            for (int i = 0; i < 20; ++i)
            {
                proc.processBlock (buf, midi);
                for (int c = 0; c < buf.getNumChannels(); ++c)
                    for (int k = 0; k < n; ++k) { finite &= std::isfinite (buf.getSample (c, k)); if (c < 2) peak = std::max (peak, std::abs (buf.getSample (c, k))); }
                for (int c = 2; c < buf.getNumChannels(); ++c) finite &= buf.getMagnitude (c, 0, n) == 0.0f;
            }
        }
        all &= report ("procesar", finite && peak > 0.001f && peak < 1.0f, "13 salidas, buffers de 1 a 2048, MIDI on/off, pico " + juce::String (peak, 3));
    }

    std::printf (all ? "PLUGIN OK\n" : "PLUGIN CON FALLAS\n");
    return all ? 0 : 1;
}
