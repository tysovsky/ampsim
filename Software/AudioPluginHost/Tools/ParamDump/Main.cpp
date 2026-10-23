/*
    ParamDump: loads a VST3 plugin and writes its parameters to a tab-separated file,
    for working out what to put in its definition file in PluginUIDefinitions.

    Usage: ParamDump <plugin.vst3> [output.tsv] [--all]

    For each parameter it writes the index, ID, name, unit label, default and current
    values (normalised 0-1), number of steps, whether it's discrete or boolean, the
    current value as text, and for discrete parameters the text of every step
    (e.g. "Roses | Cherubs | Pink"). Those step texts are what the definition files'
    <Set value="..."/> entries are matched against.

    The MIDI CC parameters that VST3 plugins expose for MIDI learn ("MIDI CC 0|1" and
    so on, often thousands of them) are left out unless --all is given.
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>

using namespace juce;

int main (int argc, char* argv[])
{
    StringArray args;

    for (int i = 1; i < argc; ++i)
        args.add (String::fromUTF8 (argv[i]));

    const bool includeMidiCCs = args.contains ("--all");
    args.removeString ("--all");

    if (args.isEmpty())
    {
        std::cerr << "usage: ParamDump <plugin.vst3> [output.tsv] [--all]\n";
        return 1;
    }

    ScopedJuceInitialiser_GUI juceInit;

    AudioPluginFormatManager formats;
    formats.addFormat (new VST3PluginFormat());

    const File pluginFile (File::getCurrentWorkingDirectory().getChildFile (args[0]));
    OwnedArray<PluginDescription> types;
    formats.getFormat (0)->findAllTypesForFile (types, pluginFile.getFullPathName());

    if (types.isEmpty())
    {
        std::cerr << "No VST3 plugin found in " << pluginFile.getFullPathName() << "\n";
        return 1;
    }

    String error;
    auto instance = formats.createPluginInstance (*types[0], 48000.0, 512, error);

    if (instance == nullptr)
    {
        std::cerr << "Couldn't load " << types[0]->name << ": " << error << "\n";
        return 1;
    }

    const auto outFile = args.size() > 1 ? File::getCurrentWorkingDirectory().getChildFile (args[1])
                                         : File::getCurrentWorkingDirectory().getChildFile (instance->getName() + " parameters.tsv");
    outFile.deleteFile();
    FileOutputStream out (outFile);

    if (out.failedToOpen())
    {
        std::cerr << "Couldn't write " << outFile.getFullPathName() << "\n";
        return 1;
    }

    out << "index\tid\tname\tlabel\tdefault\tvalue\tsteps\tdiscrete\tboolean\ttext\tstepTexts\n";

    int numWritten = 0;

    for (auto* p : instance->getParameters())
    {
        const auto name = p->getName (128);

        if (! includeMidiCCs && name.startsWith ("MIDI CC "))
            continue;

        String id;

        if (auto* hosted = dynamic_cast<HostedAudioProcessorParameter*> (p))
            id = hosted->getParameterID();

        const int steps = p->getNumSteps();
        String stepTexts;

        if (p->isDiscrete() && steps > 1 && steps <= 64)
            for (int i = 0; i < steps; ++i)
                stepTexts << (i > 0 ? " | " : "") << p->getText ((float) i / (float) (steps - 1), 64);

        out << p->getParameterIndex() << "\t" << id << "\t" << name << "\t" << p->getLabel() << "\t"
            << p->getDefaultValue() << "\t" << p->getValue() << "\t" << steps << "\t"
            << (int) p->isDiscrete() << "\t" << (int) p->isBoolean() << "\t"
            << p->getCurrentValueAsText() << "\t" << stepTexts << "\n";

        ++numWritten;
    }

    out.flush();

    std::cout << instance->getName() << ": wrote " << numWritten << " of "
              << instance->getParameters().size() << " parameters to " << outFile.getFullPathName() << "\n";
    return 0;
}
