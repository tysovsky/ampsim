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
    std::shared_ptr<const XmlElement> controls;     // its <Controls>, or nullptr if it has none
};

//==============================================================================
/**
    What a plugin's definition file in PluginUIDefinitions says about it: the
    modules it can be split into, read from the file's <Modules> element, and the
    controls shown for the whole plugin, from a <Controls> element of its own.

    Controls are shown on the block controls panel when a block is selected; see
    BlockControlsPanel for what a <Controls> element can contain. A
    <Controls use="name"/> stands for a <Controls name="name"> inside <Modules>,
    for modules (or a module and the whole plugin) with the same controls. Any
    controls inside it go before the shared ones.
*/
class PluginModuleSet
{
public:
    /** Reads a definition file. Returns an empty set if it has no modules or controls. */
    static PluginModuleSet load (const File& definitionFile);

    /** Reads every definition file in a folder, keyed by plugin name. */
    static std::map<String, PluginModuleSet> loadAll (const File& folder);

    bool isEmpty() const noexcept                                { return modules.empty() && pluginControls == nullptr; }
    bool hasModules() const noexcept                             { return ! modules.empty(); }
    const std::vector<PluginModule>& getModules() const noexcept { return modules; }
    const PluginModule* findModule (const String& name) const;

    /** The controls for the whole plugin, or nullptr if it has none. */
    const XmlElement* getPluginControls() const noexcept         { return pluginControls.get(); }

    /** Switches everything off apart from what the module's settings switch on. */
    void apply (AudioProcessor&, const PluginModule&) const;

private:
    static void applySettings (AudioProcessor&, const PluginModule::Settings&);

    String pluginName;
    PluginModule::Settings offSettings;
    std::vector<PluginModule> modules;
    std::shared_ptr<const XmlElement> pluginControls;
};

/** The folders definition files are read from: next to the app, then the old install location. */
Array<File> getPluginDefinitionFolders();

/** Finds a plugin's parameter by its name, as definition files refer to them. */
AudioProcessorParameter* findParameterByName (AudioProcessor&, const String& name);
