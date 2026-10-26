#include "SoundLibrary.h"
#include "../UI/MainHostWindow.h"

namespace
{
    const String fileExtension = ".sound";

    // A "view." property, as those are saved with the chain
    const Identifier currentSoundProperty { "view.sound" };
}

SoundLibrary::SoundLibrary (const String& plugin, const String& module)
    : pluginName (plugin), moduleName (module)
{
    const auto folderName = makeLegalName (module.isNotEmpty() ? plugin + " - " + module : plugin);

    folder = getAppProperties().getUserSettings()->getFile().getSiblingFile ("Sounds").getChildFile (folderName);
}

SoundLibrary SoundLibrary::forBlock (const AudioProcessorGraph::Node& node)
{
    auto* plugin = dynamic_cast<AudioPluginInstance*> (node.getProcessor());

    return { plugin != nullptr ? plugin->getPluginDescription().name : String(),
             node.properties["module"].toString() };
}

StringArray SoundLibrary::getNames() const
{
    StringArray names;

    for (auto& file : folder.findChildFiles (File::findFiles, false, "*" + fileExtension))
        names.add (file.getFileNameWithoutExtension());

    names.sortNatural();
    return names;
}

bool SoundLibrary::contains (const String& name) const
{
    return name.isNotEmpty() && getFile (name).existsAsFile();
}

bool SoundLibrary::save (AudioProcessor& processor, const String& name) const
{
    if (makeLegalName (name).isEmpty() || folder.createDirectory().failed())
        return false;

    MemoryBlock state;
    processor.getStateInformation (state);

    XmlElement sound ("SOUND");
    sound.setAttribute ("plugin", pluginName);

    if (moduleName.isNotEmpty())
        sound.setAttribute ("module", moduleName);

    sound.setAttribute ("saved", Time::getCurrentTime().toISO8601 (true));
    sound.createNewChildElement ("STATE")->addTextElement (state.toBase64Encoding());

    return sound.writeTo (getFile (name));
}

bool SoundLibrary::load (AudioProcessor& processor, const String& name) const
{
    const auto sound = XmlDocument::parse (getFile (name));
    auto* stateElement = sound != nullptr && sound->hasTagName ("SOUND") ? sound->getChildByName ("STATE") : nullptr;

    MemoryBlock state;

    if (stateElement == nullptr || ! state.fromBase64Encoding (stateElement->getAllSubText()) || state.isEmpty())
        return false;

    processor.setStateInformation (state.getData(), (int) state.getSize());
    return true;
}

bool SoundLibrary::rename (const String& name, const String& newName) const
{
    const auto file = getFile (name);
    const auto renamed = getFile (newName);

    if (makeLegalName (newName).isEmpty() || ! file.existsAsFile())
        return false;

    // Only a change of capitals, which Windows sees as the same file
    if (renamed.getFullPathName().equalsIgnoreCase (file.getFullPathName()))
        return file.moveFileTo (renamed);

    return ! renamed.exists() && file.moveFileTo (renamed);
}

bool SoundLibrary::remove (const String& name) const
{
    const auto file = getFile (name);
    return file.existsAsFile() && file.moveToTrash();
}

Time SoundLibrary::getSaveTime (const String& name) const
{
    return getFile (name).getLastModificationTime();
}

String SoundLibrary::getUnusedName (const String& name) const
{
    const auto legalName = makeLegalName (name);

    if (! contains (legalName))
        return legalName;

    // Counting on from a number it already ends with, e.g. "Pad 2" goes to "Pad 3"
    auto base = legalName;
    int number = 2;

    if (const auto lastWord = legalName.fromLastOccurrenceOf (" ", false, false); lastWord.containsOnly ("0123456789") && lastWord.isNotEmpty())
    {
        base = legalName.upToLastOccurrenceOf (" ", false, false);
        number = lastWord.getIntValue() + 1;
    }

    while (contains (base + " " + String (number)))
        ++number;

    return base + " " + String (number);
}

String SoundLibrary::getCurrentSound (const AudioProcessorGraph::Node& node)
{
    return node.properties[currentSoundProperty].toString();
}

void SoundLibrary::setCurrentSound (AudioProcessorGraph::Node& node, const String& name)
{
    if (name.isEmpty())
        node.properties.remove (currentSoundProperty);
    else
        node.properties.set (currentSoundProperty, name);
}

String SoundLibrary::makeLegalName (const String& name)
{
    return File::createLegalFileName (name.trim()).trim();
}

File SoundLibrary::getFile (const String& name) const
{
    return folder.getChildFile (makeLegalName (name) + fileExtension);
}
