#include "SoundLibrary.h"
#include "../UI/MainHostWindow.h"

namespace
{
    const String fileExtension = ".sound";
    const String presetsFolderName = "Presets";

    // A "view." property, as those are saved with the chain
    const Identifier currentSoundProperty { "view.sound" };

    // Sounds are saved compressed, as a plugin's state can be half a megabyte; the first ones
    // weren't, and are read as they are
    std::unique_ptr<XmlElement> readSound (const File& file)
    {
        FileInputStream in (file);

        if (! in.openedOk())
            return {};

        if (in.readByte() == '<')
            return XmlDocument::parse (file);

        in.setPosition (0);
        GZIPDecompressorInputStream unzipped (in);
        return XmlDocument::parse (unzipped.readEntireStreamAsString());
    }

    bool writeSound (const XmlElement& sound, const File& file)
    {
        // Into a file of its own first, so a sound being saved over isn't lost if it fails
        TemporaryFile temp (file);

        {
            FileOutputStream out (temp.getFile());

            if (! out.openedOk())
                return false;

            GZIPCompressorOutputStream zipped (out);
            sound.writeTo (zipped);
        }

        return temp.overwriteTargetFileWithTemporary();
    }
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

StringArray SoundLibrary::getGroups() const
{
    StringArray groups;

    for (auto& group : folder.getChildFile (presetsFolderName).findChildFiles (File::findDirectories, false))
        groups.add (group.getFileName());

    groups.sortNatural();
    return groups;
}

StringArray SoundLibrary::getNames (const String& group) const
{
    StringArray names;

    for (auto& file : getGroupFolder (group).findChildFiles (File::findFiles, false, "*" + fileExtension))
        names.add (file.getFileNameWithoutExtension());

    names.sortNatural();
    return names;
}

String SoundLibrary::makeId (const String& group, const String& name)
{
    return group.isNotEmpty() ? group + "/" + name : name;
}

String SoundLibrary::getGroup (const String& id)
{
    // (upToLastOccurrenceOf() would give the whole of a name without one)
    return id.containsChar ('/') ? id.upToLastOccurrenceOf ("/", false, false) : String();
}

String SoundLibrary::getName (const String& id)
{
    return id.fromLastOccurrenceOf ("/", false, false);
}

bool SoundLibrary::contains (const String& id) const
{
    return getName (id).isNotEmpty() && getFile (id).existsAsFile();
}

bool SoundLibrary::save (AudioProcessor& processor, const String& id) const
{
    if (makeLegalName (getName (id)).isEmpty() || getGroupFolder (getGroup (id)).createDirectory().failed())
        return false;

    MemoryBlock state;
    processor.getStateInformation (state);

    XmlElement sound ("SOUND");
    sound.setAttribute ("plugin", pluginName);

    if (moduleName.isNotEmpty())
        sound.setAttribute ("module", moduleName);

    sound.setAttribute ("saved", Time::getCurrentTime().toISO8601 (true));
    sound.createNewChildElement ("STATE")->addTextElement (state.toBase64Encoding());

    return writeSound (sound, getFile (id));
}

bool SoundLibrary::load (AudioProcessor& processor, const String& id) const
{
    const auto sound = readSound (getFile (id));
    auto* stateElement = sound != nullptr && sound->hasTagName ("SOUND") ? sound->getChildByName ("STATE") : nullptr;

    MemoryBlock state;

    if (stateElement == nullptr || ! state.fromBase64Encoding (stateElement->getAllSubText()) || state.isEmpty())
        return false;

    processor.setStateInformation (state.getData(), (int) state.getSize());
    return true;
}

bool SoundLibrary::rename (const String& id, const String& newName) const
{
    const auto file = getFile (id);
    const auto renamed = getFile (makeId (getGroup (id), newName));

    if (makeLegalName (newName).isEmpty() || ! file.existsAsFile())
        return false;

    // Only a change of capitals, which Windows sees as the same file
    if (renamed.getFullPathName().equalsIgnoreCase (file.getFullPathName()))
        return file.moveFileTo (renamed);

    return ! renamed.exists() && file.moveFileTo (renamed);
}

bool SoundLibrary::remove (const String& id) const
{
    const auto file = getFile (id);
    return file.existsAsFile() && file.moveToTrash();
}

Time SoundLibrary::getSaveTime (const String& id) const
{
    return getFile (id).getLastModificationTime();
}

String SoundLibrary::getUnusedName (const String& name, const String& group) const
{
    const auto legalName = makeLegalName (name);
    const auto isUsed = [&] (const String& n) { return contains (makeId (group, n)); };

    if (! isUsed (legalName))
        return legalName;

    // Counting on from a number it already ends with, e.g. "Pad 2" goes to "Pad 3"
    auto base = legalName;
    int number = 2;

    if (const auto lastWord = legalName.fromLastOccurrenceOf (" ", false, false); lastWord.containsOnly ("0123456789") && lastWord.isNotEmpty())
    {
        base = legalName.upToLastOccurrenceOf (" ", false, false);
        number = lastWord.getIntValue() + 1;
    }

    while (isUsed (base + " " + String (number)))
        ++number;

    return base + " " + String (number);
}

String SoundLibrary::getCurrentSound (const AudioProcessorGraph::Node& node)
{
    return node.properties[currentSoundProperty].toString();
}

void SoundLibrary::setCurrentSound (AudioProcessorGraph::Node& node, const String& id)
{
    if (id.isEmpty())
        node.properties.remove (currentSoundProperty);
    else
        node.properties.set (currentSoundProperty, id);
}

String SoundLibrary::makeLegalName (const String& name)
{
    return File::createLegalFileName (name.trim()).trim();
}

File SoundLibrary::getGroupFolder (const String& group) const
{
    return group.isNotEmpty() ? folder.getChildFile (presetsFolderName).getChildFile (makeLegalName (group)) : folder;
}

File SoundLibrary::getFile (const String& id) const
{
    return getGroupFolder (getGroup (id)).getChildFile (makeLegalName (getName (id)) + fileExtension);
}
