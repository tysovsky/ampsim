#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    The sounds saved for a plugin, each a copy of the plugin's whole state under a
    name, so it comes back exactly as it was. They're kept in the app's Sounds
    folder, in a folder for each plugin (and one for each module of a plugin, as a
    module's sounds are of that module), as files named after the sounds.

    Plugins don't say what their own presets are called, so this is how the host
    remembers sounds by name. As well as the user's own sounds, a plugin's folder
    can have its presets, in a Presets folder with a folder for each group of
    them, such as each type of sound:

      Sounds/Pigments/Warm Pad.sound              one of the user's own
      Sounds/Pigments/Presets/Bass/Acid Line.sound   one of Pigments' presets

    A sound's id is its name, for one of the user's own, or its group and name,
    e.g. "Bass/Acid Line".
*/
class SoundLibrary
{
public:
    /** The sounds of a plugin, or of one of its modules if there's a module name. */
    SoundLibrary (const String& pluginName, const String& moduleName);

    /** The sounds of a block's plugin, or of its module if it's one. */
    static SoundLibrary forBlock (const AudioProcessorGraph::Node&);

    /** The groups of the plugin's presets, in order. */
    StringArray getGroups() const;

    /** The names of the sounds in a group, in order, or of the user's own for no group. */
    StringArray getNames (const String& group = {}) const;

    static String makeId (const String& group, const String& name);
    static String getGroup (const String& id);
    static String getName (const String& id);

    bool contains (const String& id) const;

    /** Keeps the plugin as it is now as a sound, replacing a sound with the id. */
    bool save (AudioProcessor&, const String& id) const;

    /** Puts the plugin back as it was when a sound was saved. */
    bool load (AudioProcessor&, const String& id) const;

    /** Gives a sound another name, in the same group. Returns false if there's already a sound with it. */
    bool rename (const String& id, const String& newName) const;

    /** Moves a sound's file to the Recycle Bin, so one deleted by mistake can be got back. */
    bool remove (const String& id) const;

    /** When a sound was saved. */
    Time getSaveTime (const String& id) const;

    /** The name, or if the group has a sound with it already, the name with the first number on the
        end that makes it one it hasn't, e.g. "Pad 2". */
    String getUnusedName (const String& name, const String& group = {}) const;

    /** The id of the sound a block was last set to, or empty if it hasn't been set to one or has
        been changed to something else since, such as by stepping through the plugin's presets in
        the plugin itself. It's one of the block's properties, so it's saved with the chain. */
    static String getCurrentSound (const AudioProcessorGraph::Node&);
    static void setCurrentSound (AudioProcessorGraph::Node&, const String& id);

    /** A name that's allowed in a file name, as sounds' names have to be. */
    static String makeLegalName (const String& name);

private:
    File getGroupFolder (const String& group) const;
    File getFile (const String& id) const;

    String pluginName, moduleName;
    File folder;
};
