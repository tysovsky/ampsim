#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    The sounds saved for a plugin, each a copy of the plugin's whole state under a
    name, so it comes back exactly as it was. They're kept in the app's Sounds
    folder, in a folder for each plugin (and one for each module of a plugin, as a
    module's sounds are of that module), as files named after the sounds.

    Plugins don't say what their own presets are called, so this is how the host
    remembers sounds by name.
*/
class SoundLibrary
{
public:
    /** The sounds of a plugin, or of one of its modules if there's a module name. */
    SoundLibrary (const String& pluginName, const String& moduleName);

    /** The sounds of a block's plugin, or of its module if it's one. */
    static SoundLibrary forBlock (const AudioProcessorGraph::Node&);

    /** The names of the sounds, in order. */
    StringArray getNames() const;

    bool contains (const String& name) const;

    /** Keeps the plugin as it is now under a name, replacing a sound with the name. */
    bool save (AudioProcessor&, const String& name) const;

    /** Puts the plugin back as it was when a sound was saved. */
    bool load (AudioProcessor&, const String& name) const;

    /** Gives a sound another name. Returns false if there's already a sound with it. */
    bool rename (const String& name, const String& newName) const;

    /** Moves a sound's file to the Recycle Bin, so one deleted by mistake can be got back. */
    bool remove (const String& name) const;

    /** When a sound was saved. */
    Time getSaveTime (const String& name) const;

    /** The name, or if there's a sound with it already, the name with the first number on the end
        that makes it one there isn't, e.g. "Pad 2". */
    String getUnusedName (const String& name) const;

    /** The sound a block was last set to, or empty if it hasn't been set to one or has been
        changed to something else since, such as one of the plugin's own presets. It's one of the
        block's properties, so it's saved with the chain. */
    static String getCurrentSound (const AudioProcessorGraph::Node&);
    static void setCurrentSound (AudioProcessorGraph::Node&, const String& name);

    /** A name that's allowed in a file name, as sounds' names have to be. */
    static String makeLegalName (const String& name);

private:
    File getFile (const String& name) const;

    String pluginName, moduleName;
    File folder;
};
