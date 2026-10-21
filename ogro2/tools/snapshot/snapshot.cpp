// Abre el editor de OGRO 2 fuera de un host y guarda una captura PNG
// Uso: snapshot salida.png [escala] [pista] [panel] [preset]
#include "PluginProcessor.h"
#include "PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File out (argc > 1 ? juce::String (argv[1]) : juce::String ("editor.png"));
    const float scale = argc > 2 ? (float) std::atof (argv[2]) : 1.0f;
    Ogro2Processor proc;
    proc.prepareToPlay (44100.0, 512);
    if (argc > 5) proc.loadPreset (std::atoi (argv[5]));
    if (argc > 3) proc.selectedTrack = std::atoi (argv[3]);
    std::unique_ptr<Ogro2Editor> ed (dynamic_cast<Ogro2Editor*> (proc.createEditor()));
    if (argc > 4) ed->getContent().setPage (std::atoi (argv[4]));
    ed->getContent().refreshNow();
    const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat png;
    const bool ok = stream.openedOk() && png.writeImageToStream (img, stream);
    std::printf ("%s %dx%d -> %s\n", ok ? "OK" : "FALLA", img.getWidth(), img.getHeight(), out.getFullPathName().toRawUTF8());
    return ok ? 0 : 1;
}
