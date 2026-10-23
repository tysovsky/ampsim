#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    A part of a plugin that can be added to the chain on its own, such as one amp
    or one pedal of an amp suite. Adding it loads the whole plugin and switches
    everything else off.
*/
struct PluginModule
{
    using Settings = std::vector<std::pair<String, String>>;   // parameter name and value text

    String group;       // e.g. "Amps", or empty for a module that isn't in a group
    String name;        // e.g. "Roses (Clean)"
    String icon;        // what kind of block it is, e.g. "amp", from the module or else its group
    Settings settings;  // applied after the plugin's "off" settings
};

//==============================================================================
/**
    The modules defined for one plugin, read from the <Modules> element of its
    definition file in PluginUIDefinitions.
*/
class PluginModuleSet
{
public:
    /** Reads the modules from a definition file. Returns an empty set if it has none. */
    static PluginModuleSet load (const File& definitionFile);

    /** Reads every definition file in a folder, keyed by plugin name. */
    static std::map<String, PluginModuleSet> loadAll (const File& folder);

    bool isEmpty() const noexcept                                { return modules.empty(); }
    const std::vector<PluginModule>& getModules() const noexcept { return modules; }
    const PluginModule* findModule (const String& name) const;

    /** Switches everything off apart from what the module's settings switch on. */
    void apply (AudioProcessor&, const PluginModule&) const;

private:
    static void applySettings (AudioProcessor&, const PluginModule::Settings&);

    String pluginName;
    PluginModule::Settings offSettings;
    std::vector<PluginModule> modules;
};

/** The folders definition files are read from: next to the app, then the old install location. */
Array<File> getPluginDefinitionFolders();
